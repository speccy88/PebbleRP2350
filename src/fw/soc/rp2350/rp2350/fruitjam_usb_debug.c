/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_usb_debug.h"

#include "hardware/resets.h"
#include "soc/rp2350/rp2350/hardware/irq.h"
#include "soc/rp2350/rp2350/hardware/timer.h"
#include "applib/app_launch_reason.h"
#include "applib/graphics/gcolor_definitions.h"
#include "board/board.h"
#include "comm/bt_lock.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gatt_client_discovery.h"
#include "comm/ble/gap_le_slave_discovery.h"
#include "comm/ble/gap_le_slave_reconnect.h"
#include "drivers/button.h"
#include "drivers/flash.h"
#include "drivers/rp2350/button.h"
#include "kernel/event_loop.h"
#include "pbl/services/app_fetch_endpoint.h"
#include "pbl/services/app_cache.h"
#include "pbl/services/blob_db/app_db.h"
#include "pbl/services/clock.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/put_bytes/put_bytes.h"
#include "pbl/services/process_management/app_storage.h"
#include "pbl/services/timezone_database.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_info.h"
#include "resource/resource_storage.h"
#include "soc/rp2350/rp2350/fruitjam_bootsel.h"
#include "soc/rp2350/rp2350/fruitjam_boot_progress.h"
#include "soc/rp2350/rp2350/fruitjam_bt_debug.h"
#include "soc/rp2350/rp2350/fruitjam_esp.h"
#include "soc/rp2350/rp2350/fruitjam_lcd.h"
#include "soc/rp2350/rp2350/hardware/clocks.h"
#include "soc/rp2350/rp2350/rp2350_external_rtc.h"
#include "shell/normal/watchface.h"
#include "system/bootbits.h"
#include "system/reboot_reason.h"
#include "system/reset.h"
#include "util/uuid.h"

#include "bluetooth/gap_le_connect.h"
#include "pbl/services/bluetooth/bluetooth_ctl.h"
#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"
#include "pbl/services/bluetooth/pairability.h"

#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USB_TASK_STACK_SIZE 512U
#define USB_TASK_PRIORITY (tskIDLE_PRIORITY + 4U)
#define TX_RING_SIZE 2048U
#define RX_LINE_SIZE 192U
#define USB_WRITE_STALL_TIMEOUT_MS 500U
#define TASK_SNAPSHOT_COUNT 24U
#define ESP_PASSTHROUGH_DEFAULT_BAUD 115200U
#define ESP_PASSTHROUGH_BURST_LIMIT 128U
#define BLE_PAIRABLE_DEBUG_WINDOW_SECS 300U
#define RESET_AFTER_FLUSH_DELAY_MS 250U
#define APP_SIDELOAD_CHUNK_BYTES 80U
#define BUTTON_WATCH_DEFAULT_MS 5000U
#define BUTTON_WATCH_MAX_MS 30000U
#define BUTTON_WATCH_PERIOD_MS 20U
#define BUTTON_WATCH_HEARTBEAT_MS 500U
#define GPIO_WATCH_MAX_MS 30000U
#define GPIO_WATCH_DEFAULT_MS 5000U
#define GPIO_WATCH_PERIOD_MS 20U
#define GPIO_WATCH_HEARTBEAT_MS 500U
#define GPIO_WATCH_PIN_COUNT 48U
#define GPIO_WATCH_ALL_MASK ((UINT64_C(1) << GPIO_WATCH_PIN_COUNT) - 1U)
#define I2C_SCAN_MAX_ADDRESSES 32U
#define USB_REALTIME_BOOTSEL_MS 15000U
#define USB_POLLED_MESSAGE_INTERVAL_MS 1000U
#define RTC_SET_MIN_TIMESTAMP 1262304000UL
#define RTC_SET_MAX_TIMESTAMP 2145916799UL
#define BOOTSEL_REASON_CODE(a, b, c, d) \
  ((uint32_t)(a) | ((uint32_t)(b) << 8U) | ((uint32_t)(c) << 16U) | ((uint32_t)(d) << 24U))
#define BOOTSEL_REASON_POLLED_CLOCK_FAIL BOOTSEL_REASON_CODE('P', 'O', 'L', 'C')
#define BOOTSEL_REASON_POLLED_TUSB_FAIL BOOTSEL_REASON_CODE('P', 'O', 'L', 'I')
#define BOOTSEL_REASON_POLLED_TIMEOUT BOOTSEL_REASON_CODE('P', 'O', 'L', 'D')
#define BOOTSEL_REASON_USB_TASK_TIMEOUT BOOTSEL_REASON_CODE('U', 'S', 'B', 'T')
#define USB_STATUS_MAIN_ENABLED (1U << 0U)
#define USB_STATUS_MAIN_DEVICE_MODE (1U << 1U)
#define USB_STATUS_PHY_UNISOLATED (1U << 2U)
#define USB_STATUS_SIE_PULLUP (1U << 3U)
#define USB_STATUS_TRANSCEIVER_POWERED (1U << 4U)
#define USB_STATUS_SIE_CONNECTED (1U << 5U)
#define USB_STATUS_VBUS_DETECTED (1U << 6U)
#define USB_STATUS_VBUS_FORCED (1U << 7U)
#define USB_STATUS_MUX_TO_PHY (1U << 8U)
#define USB_STATUS_MUX_SOFTCON (1U << 9U)
#define USB_EVENT_MOUNT_CB (1U << 0U)
#define USB_EVENT_UMOUNT_CB (1U << 1U)
#define USB_EVENT_SUSPEND_CB (1U << 2U)
#define USB_EVENT_RESUME_CB (1U << 3U)
#define USB_EVENT_LINE_CODING_CB (1U << 4U)
#define USB_EVENT_LINE_STATE_CB (1U << 5U)
#define USB_EVENT_MOUNTED_NOW (1U << 6U)
#define USB_EVENT_CDC_CONNECTED_NOW (1U << 7U)
#define USB_SETUP_GET_STATUS (1U << 0U)
#define USB_SETUP_CLEAR_FEATURE (1U << 1U)
#define USB_SETUP_SET_FEATURE (1U << 2U)
#define USB_SETUP_SET_ADDRESS (1U << 3U)
#define USB_SETUP_GET_DESCRIPTOR (1U << 4U)
#define USB_SETUP_GET_CONFIGURATION (1U << 5U)
#define USB_SETUP_SET_CONFIGURATION (1U << 6U)
#define USB_SETUP_OTHER (1U << 7U)
#define USB_SETUP_TYPE_MASK 0x60U
#define USB_SETUP_TYPE_STANDARD 0x00U
#define USB_STATUS_KIND_SETUP 0xa0000000U
#define USB_STATUS_KIND_DESCRIPTOR 0xd0000000U
#define USB_STATUS_KIND_HARDWARE 0xe0000000U
#define USB_SETUP_HISTORY_OTHER 0xeU
#define USB_SETUP_HISTORY_ADDRESS_STATUS 0xfU
#define USB_REQUEST_GET_STATUS 0U
#define USB_REQUEST_CLEAR_FEATURE 1U
#define USB_REQUEST_SET_FEATURE 3U
#define USB_REQUEST_SET_ADDRESS 5U
#define USB_REQUEST_GET_DESCRIPTOR 6U
#define USB_REQUEST_GET_CONFIGURATION 8U
#define USB_REQUEST_SET_CONFIGURATION 9U
#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
#define GPIO_WATCH_DEFAULT_IGNORE_MASK (UINT64_C(1) << 10U)
#else
#define GPIO_WATCH_DEFAULT_IGNORE_MASK 0U
#endif

#if defined(CONFIG_RP2350_PEBBLE_STORAGE_EXTERNAL_NOR)
#define RP2350_PEBBLE_STORAGE_NAME "external-nor"
#else
#define RP2350_PEBBLE_STORAGE_NAME "onboard-xip"
#endif

#if defined(CONFIG_RP2350_PEBBLE_STORAGE_XIP_ERASE_HAZARD)
#define RP2350_PEBBLE_STORAGE_HAZARD 1U
#else
#define RP2350_PEBBLE_STORAGE_HAZARD 0U
#endif

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define SIO_BASE 0xd0000000U
#define SIO_GPIO_IN_OFFSET 0x04U
#define SIO_GPIO_HI_IN_OFFSET 0x08U

#define CLOCKS_BASE 0x40010000U
#define XOSC_BASE 0x40048000U
#define PLL_USB_BASE 0x40058000U

#define CLOCKS_CLK_USB_CTRL_OFFSET 0x60U
#define CLOCKS_CLK_USB_DIV_OFFSET 0x64U
#define CLOCKS_CLK_USB_CTRL_ENABLE_BITS (1U << 11)
#define CLOCKS_CLK_USB_CTRL_ENABLED_BITS (1U << 28)
#define CLOCKS_CLK_USB_DIV_1 0x00010000U

#define XOSC_CTRL_OFFSET 0x00U
#define XOSC_STATUS_OFFSET 0x04U
#define XOSC_STARTUP_OFFSET 0x0cU
#define XOSC_CTRL_ENABLE_LSB 12U
#define XOSC_CTRL_ENABLE_VALUE_ENABLE 0xfabU
#define XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ 0xaa0U
#define XOSC_STATUS_STABLE_BITS (1U << 31)
#define XOSC_STARTUP_DELAY_12MHZ 47U

#define PLL_CS_OFFSET 0x00U
#define PLL_PWR_OFFSET 0x04U
#define PLL_FBDIV_INT_OFFSET 0x08U
#define PLL_PRIM_OFFSET 0x0cU
#define PLL_CS_LOCK_BITS (1U << 31)
#define PLL_PWR_PD_BITS (1U << 0)
#define PLL_PWR_POSTDIVPD_BITS (1U << 3)
#define PLL_PWR_VCOPD_BITS (1U << 5)
#define PLL_PRIM_POSTDIV1_LSB 16U
#define PLL_PRIM_POSTDIV2_LSB 12U
#define PLL_USB_REFDIV 1U
#define PLL_USB_FBDIV 100U
#define PLL_USB_POSTDIV1 5U
#define PLL_USB_POSTDIV2 5U
#define USB_CLOCK_WAIT_LIMIT 2000000U

#define USBCTRL_REGS_BASE 0x50110000U
#define USB_MAIN_CTRL_OFFSET 0x40U
#define USB_SIE_CTRL_OFFSET 0x4cU
#define USB_SIE_STATUS_OFFSET 0x50U
#define USB_USB_MUXING_OFFSET 0x74U
#define USB_USB_PWR_OFFSET 0x78U
#define USB_INTS_OFFSET 0x98U
#define USB_MAIN_CTRL_CONTROLLER_EN_BITS (1U << 0U)
#define USB_MAIN_CTRL_HOST_NDEVICE_BITS (1U << 1U)
#define USB_MAIN_CTRL_PHY_ISO_BITS (1U << 2U)
#define USB_SIE_CTRL_TRANSCEIVER_PD_BITS (1U << 18U)
#define USB_SIE_CTRL_PULLUP_EN_BITS (1U << 16U)
#define USB_SIE_STATUS_CONNECTED_BITS (1U << 16U)
#define USB_SIE_STATUS_VBUS_DETECTED_BITS (1U << 0U)
#define USB_USB_MUXING_TO_PHY_BITS (1U << 0U)
#define USB_USB_MUXING_SOFTCON_BITS (1U << 3U)
#define USB_USB_PWR_VBUS_DETECT_BITS (1U << 2U)
#define USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS (1U << 3U)

#define BOOTSEL_CDC_MAGIC_BAUD_RATE 1200U

typedef void (*IrqHandler)(void);

static TaskHandle_t s_usb_task;
static IrqHandler s_usb_irq_handler;
static volatile uint32_t s_usb_irq_count;
static volatile uint32_t s_usb_irq_no_handler_count;
static volatile uint32_t s_usb_polled_irq_service_count;
static volatile uint32_t s_usb_event_flags;
static volatile uint32_t s_usb_setup_count;
static volatile uint32_t s_usb_setup_flags;
static volatile uint32_t s_usb_setup_last_request;
static volatile uint32_t s_usb_setup_last_request_type;
static volatile uint32_t s_usb_setup_address_complete_count;
static volatile uint32_t s_usb_setup_last_dev_addr_ctrl;
static volatile uint32_t s_usb_setup_request_history;
static volatile uint32_t s_tx_head;
static volatile uint32_t s_tx_tail;
static uint8_t s_tx_ring[TX_RING_SIZE];
static char s_rx_line[RX_LINE_SIZE];
static uint8_t s_rx_line_length;
static bool s_initialized;
static bool s_esp_passthrough;
static bool s_bluetooth_force_pending;
static bool s_bluetooth_pairing_pending;
static bool s_bluetooth_reset_pending;

typedef enum {
  AppSideloadPartNone = 0,
  AppSideloadPartApp,
  AppSideloadPartResources,
} AppSideloadPart;

typedef struct {
  bool active;
  bool app_db_entry_existed;
  AppInstallId install_id;
  int fd;
  AppSideloadPart open_part;
  uint32_t app_size;
  uint32_t resources_size;
  uint32_t app_written;
  uint32_t resources_written;
} AppSideloadState;

static AppSideloadState s_app_sideload = {
    .fd = -1,
};

extern uint32_t fruitjam_usb_descriptor_debug_word(void);

static const char *prv_read_token(const char *input, char *out, size_t out_size);
static bool prv_parse_timestamp_arg(const char *args, time_t *time_out);

void fruitjam_usb_irq_set_handler(IrqHandler handler) {
  s_usb_irq_handler = handler;
}

void USBCTRL_IRQ_IRQHandler(void) {
  ++s_usb_irq_count;
  if (s_usb_irq_handler) {
    s_usb_irq_handler();
  } else {
    ++s_usb_irq_no_handler_count;
  }
}

uint32_t tusb_time_millis_api(void) {
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    return time_us_32() / 1000U;
  }

  return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void prv_record_usb_setup_history(uint32_t value) {
  s_usb_setup_request_history = ((s_usb_setup_request_history << 4U) | (value & 0x0fU)) & 0xffffU;
}

void fruitjam_usb_debug_record_setup_packet(const uint8_t *setup) {
  ++s_usb_setup_count;
  s_usb_setup_last_request_type = setup[0];
  s_usb_setup_last_request = setup[1];

  if ((setup[0] & USB_SETUP_TYPE_MASK) != USB_SETUP_TYPE_STANDARD) {
    s_usb_setup_flags |= USB_SETUP_OTHER;
    prv_record_usb_setup_history(USB_SETUP_HISTORY_OTHER);
    return;
  }

  prv_record_usb_setup_history(setup[1]);

  switch (setup[1]) {
    case USB_REQUEST_GET_STATUS:
      s_usb_setup_flags |= USB_SETUP_GET_STATUS;
      break;
    case USB_REQUEST_CLEAR_FEATURE:
      s_usb_setup_flags |= USB_SETUP_CLEAR_FEATURE;
      break;
    case USB_REQUEST_SET_FEATURE:
      s_usb_setup_flags |= USB_SETUP_SET_FEATURE;
      break;
    case USB_REQUEST_SET_ADDRESS:
      s_usb_setup_flags |= USB_SETUP_SET_ADDRESS;
      break;
    case USB_REQUEST_GET_DESCRIPTOR:
      s_usb_setup_flags |= USB_SETUP_GET_DESCRIPTOR;
      break;
    case USB_REQUEST_GET_CONFIGURATION:
      s_usb_setup_flags |= USB_SETUP_GET_CONFIGURATION;
      break;
    case USB_REQUEST_SET_CONFIGURATION:
      s_usb_setup_flags |= USB_SETUP_SET_CONFIGURATION;
      break;
    default:
      s_usb_setup_flags |= USB_SETUP_OTHER;
      break;
  }
}

void fruitjam_usb_debug_record_address_status_complete(uint32_t dev_addr_ctrl) {
  ++s_usb_setup_address_complete_count;
  s_usb_setup_last_dev_addr_ctrl = dev_addr_ctrl;
  prv_record_usb_setup_history(USB_SETUP_HISTORY_ADDRESS_STATUS);
}

void fruitjam_usb_debug_putchar(uint8_t c) {
  if (!s_initialized || s_esp_passthrough) {
    return;
  }

  const uint32_t next_head = (s_tx_head + 1U) % TX_RING_SIZE;
  if (next_head == s_tx_tail) {
    return;
  }

  s_tx_ring[s_tx_head] = c;
  s_tx_head = next_head;
}

static void prv_usb_write(const void *data, uint32_t length) {
  const uint8_t *bytes = data;
  uint32_t last_progress_ms = tusb_time_millis_api();

  while (length != 0U && tud_cdc_connected()) {
    uint32_t available = tud_cdc_write_available();
    if (available == 0U) {
      tud_task();
      const uint32_t now_ms = tusb_time_millis_api();
      if ((now_ms - last_progress_ms) >= USB_WRITE_STALL_TIMEOUT_MS) {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (available > length) {
      available = length;
    }
    tud_cdc_write(bytes, available);
    tud_cdc_write_flush();
    bytes += available;
    length -= available;
    last_progress_ms = tusb_time_millis_api();
  }
}

static void prv_usb_write_str(const char *str) {
  prv_usb_write(str, (uint32_t)strlen(str));
}

static void prv_usb_write_formatted_line(const char *line, int written, uint32_t capacity) {
  if (written <= 0) {
    return;
  }

  uint32_t length = (uint32_t)written;
  if (length >= capacity) {
    length = capacity - 1U;
  }
  prv_usb_write(line, length);
}

static void prv_usb_polled_write_str(const char *str) {
  if (!tud_cdc_connected()) {
    return;
  }

  const uint8_t *bytes = (const uint8_t *)str;
  uint32_t length = (uint32_t)strlen(str);
  while (length != 0U) {
    uint32_t available = tud_cdc_write_available();
    if (available == 0U) {
      tud_task();
      continue;
    }
    if (available > length) {
      available = length;
    }
    tud_cdc_write(bytes, available);
    tud_cdc_write_flush();
    bytes += available;
    length -= available;
    tud_task();
  }
}

static bool prv_usb_clock_init(void) {
  return rp2350_usb_clocks_init();
}

static uint32_t prv_usb_status_flags(void) {
  const uint32_t main_ctrl = REG32(USBCTRL_REGS_BASE + USB_MAIN_CTRL_OFFSET);
  const uint32_t sie_ctrl = REG32(USBCTRL_REGS_BASE + USB_SIE_CTRL_OFFSET);
  const uint32_t sie_status = REG32(USBCTRL_REGS_BASE + USB_SIE_STATUS_OFFSET);
  const uint32_t muxing = REG32(USBCTRL_REGS_BASE + USB_USB_MUXING_OFFSET);
  const uint32_t pwr = REG32(USBCTRL_REGS_BASE + USB_USB_PWR_OFFSET);

  uint32_t flags = 0U;
  if (main_ctrl & USB_MAIN_CTRL_CONTROLLER_EN_BITS) {
    flags |= USB_STATUS_MAIN_ENABLED;
  }
  if (!(main_ctrl & USB_MAIN_CTRL_HOST_NDEVICE_BITS)) {
    flags |= USB_STATUS_MAIN_DEVICE_MODE;
  }
  if (!(main_ctrl & USB_MAIN_CTRL_PHY_ISO_BITS)) {
    flags |= USB_STATUS_PHY_UNISOLATED;
  }
  if (sie_ctrl & USB_SIE_CTRL_PULLUP_EN_BITS) {
    flags |= USB_STATUS_SIE_PULLUP;
  }
  if (!(sie_ctrl & USB_SIE_CTRL_TRANSCEIVER_PD_BITS)) {
    flags |= USB_STATUS_TRANSCEIVER_POWERED;
  }
  if (sie_status & USB_SIE_STATUS_CONNECTED_BITS) {
    flags |= USB_STATUS_SIE_CONNECTED;
  }
  if (sie_status & USB_SIE_STATUS_VBUS_DETECTED_BITS) {
    flags |= USB_STATUS_VBUS_DETECTED;
  }
  if ((pwr & (USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS)) ==
      (USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS)) {
    flags |= USB_STATUS_VBUS_FORCED;
  }
  if (muxing & USB_USB_MUXING_TO_PHY_BITS) {
    flags |= USB_STATUS_MUX_TO_PHY;
  }
  if (muxing & USB_USB_MUXING_SOFTCON_BITS) {
    flags |= USB_STATUS_MUX_SOFTCON;
  }

  return flags;
}

static uint32_t prv_usb_packed_status(void) {
  if (fruitjam_usb_descriptor_debug_word() != 0U) {
    uint32_t event_flags = s_usb_event_flags;
    if (tud_mounted()) {
      event_flags |= USB_EVENT_MOUNTED_NOW;
    }
    if (tud_cdc_connected()) {
      event_flags |= USB_EVENT_CDC_CONNECTED_NOW;
    }
    return USB_STATUS_KIND_DESCRIPTOR | (fruitjam_usb_descriptor_debug_word() & 0x00ffffffU) |
           ((event_flags & 0x0fU) << 24U);
  }

  uint32_t setup_count = s_usb_setup_count;
  if (setup_count > 0x0fU) {
    setup_count = 0x0fU;
  }

  if (setup_count != 0U) {
    return USB_STATUS_KIND_SETUP | setup_count | ((s_usb_setup_flags & 0xffU) << 4U) |
           ((s_usb_setup_request_history & 0xffffU) << 12U);
  }

  uint32_t irq_count = s_usb_irq_count;
  if (irq_count > 0xffU) {
    irq_count = 0xffU;
  }

  uint32_t polled_irq_count = s_usb_polled_irq_service_count;
  if (polled_irq_count > 0xffU) {
    polled_irq_count = 0xffU;
  }

  return USB_STATUS_KIND_HARDWARE | prv_usb_status_flags() | (irq_count << 16U) |
         ((polled_irq_count & 0x0fU) << 24U);
}

static void prv_usb_service_pending_irq_polled(void) {
  if (!s_usb_irq_handler || REG32(USBCTRL_REGS_BASE + USB_INTS_OFFSET) == 0U) {
    return;
  }

  ++s_usb_polled_irq_service_count;
  s_usb_irq_handler();
}

static char prv_hex_digit(uint8_t value) {
  value &= 0x0fU;
  return (value < 10U) ? (char)('0' + value) : (char)('A' + value - 10U);
}

static void prv_append_hex_u32(char *out, uint32_t value) {
  for (uint32_t i = 0; i < 8U; ++i) {
    const uint32_t shift = 28U - i * 4U;
    out[i] = prv_hex_digit((uint8_t)(value >> shift));
  }
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *line_coding) {
  (void)itf;
  s_usb_event_flags |= USB_EVENT_LINE_CODING_CB;

  if (line_coding->bit_rate == BOOTSEL_CDC_MAGIC_BAUD_RATE) {
    fruitjam_bootsel_enter();
  } else if (s_esp_passthrough) {
    fruitjam_esp_passthrough_set_baud(line_coding->bit_rate);
  }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf;
  (void)dtr;
  (void)rts;
  s_usb_event_flags |= USB_EVENT_LINE_STATE_CB;
}

void tud_mount_cb(void) {
  s_usb_event_flags |= USB_EVENT_MOUNT_CB;
}

void tud_umount_cb(void) {
  s_usb_event_flags |= USB_EVENT_UMOUNT_CB;
}

void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  s_usb_event_flags |= USB_EVENT_SUSPEND_CB;
}

void tud_resume_cb(void) {
  s_usb_event_flags |= USB_EVENT_RESUME_CB;
}

static void prv_send_reason(void) {
  RebootReason reason;
  reboot_reason_get(&reason);
  const uint32_t *raw = (const uint32_t *)&reason;
  char line[] = "reason code=00 raw=00000000 00000000 00000000 00000000\r\n";

  line[12] = prv_hex_digit((uint8_t)reason.code >> 4U);
  line[13] = prv_hex_digit((uint8_t)reason.code);
  prv_append_hex_u32(&line[19], raw[0]);
  prv_append_hex_u32(&line[28], raw[1]);
  prv_append_hex_u32(&line[37], raw[2]);
  prv_append_hex_u32(&line[46], raw[3]);
  prv_usb_write_str(line);
}

static void prv_send_frame(void) {
  const uint8_t *frame = fruitjam_lcd_capture_data();
  const uint32_t sequence = fruitjam_lcd_capture_sequence();
  char header[] = "FJFRAME 144 168 3024 seq=00000000 format=pbm-p4-inverted\r\n";
  uint8_t chunk[64];

  prv_append_hex_u32(&header[25], sequence);
  prv_usb_write_str(header);

  for (uint32_t offset = 0; offset < FRUITJAM_LCD_CAPTURE_BYTES; offset += sizeof(chunk)) {
    uint32_t count = FRUITJAM_LCD_CAPTURE_BYTES - offset;
    if (count > sizeof(chunk)) {
      count = sizeof(chunk);
    }

    for (uint32_t i = 0; i < count; ++i) {
      chunk[i] = (uint8_t)~frame[offset + i];
    }
    prv_usb_write(chunk, count);
  }

  prv_usb_write_str("\r\nFJEND\r\n");
}

static char prv_task_state_char(eTaskState state) {
  switch (state) {
    case eRunning:
      return 'R';
    case eReady:
      return 'r';
    case eBlocked:
      return 'b';
    case eSuspended:
      return 's';
    case eDeleted:
      return 'd';
  }

  return '?';
}

static void prv_send_progress(void) {
  char line[128];
  const FruitJamBootProgressStage stage = fruitjam_boot_progress_last_stage();
  const int written =
      snprintf(line, sizeof(line),
               "progress seq=%" PRIu32 " stage=%u label=\"%s\" frame_seq=%" PRIu32 "\r\n",
               fruitjam_boot_progress_sequence(), (unsigned)stage,
               fruitjam_boot_progress_last_label(), fruitjam_lcd_capture_sequence());

  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_send_lcd(void) {
  char line[192];
  const int written =
      snprintf(line, sizeof(line),
               "lcd hw_spi=%u spi=%" PRIu32 " cr0=%08" PRIx32 " cpsr=%" PRIu32 " sr=%08" PRIx32
               " hz=%" PRIu32 " peri_ctrl=%08" PRIx32 " peri_div=%08" PRIx32 " frame_seq=%" PRIu32
               " transfers=%" PRIu32 " rows=%u last_us=%" PRIu32 " max_us=%" PRIu32 "\r\n",
               fruitjam_lcd_is_hardware_spi_enabled() ? 1U : 0U, fruitjam_lcd_spi_index(),
               fruitjam_lcd_spi_cr0(), fruitjam_lcd_spi_cpsr(), fruitjam_lcd_spi_status(),
               fruitjam_lcd_spi_hz(), fruitjam_lcd_clk_peri_ctrl(), fruitjam_lcd_clk_peri_div(),
               fruitjam_lcd_capture_sequence(), fruitjam_lcd_transfer_count(),
               (unsigned)fruitjam_lcd_last_transfer_rows(), fruitjam_lcd_last_transfer_us(),
               fruitjam_lcd_max_transfer_us());

  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static uint8_t prv_lcd_test_byte(uint16_t y, uint16_t byte_x, void *context) {
  (void)context;

  if (y < 4U || y >= FRUITJAM_LCD_HEIGHT - 4U || byte_x == 0U ||
      byte_x == FRUITJAM_LCD_LINE_BYTES - 1U) {
    return 0x00U;
  }

  if (((y / 8U) + byte_x) & 1U) {
    return 0x00U;
  }

  return 0xffU;
}

static void prv_handle_lcd_test_command(const char *args) {
  char mode[8] = {0};
  (void)prv_read_token(args, mode, sizeof(mode));

  const bool clear_only = strcmp(mode, "clear") == 0;
  const bool use_gpio = strcmp(mode, "gpio") == 0;
  const bool use_spi = !use_gpio;

  fruitjam_lcd_init_pins();
  fruitjam_lcd_set_display_enabled(false);
  vTaskDelay(pdMS_TO_TICKS(20));
  fruitjam_lcd_set_vcom(false);
  fruitjam_lcd_set_display_enabled(true);
  vTaskDelay(pdMS_TO_TICKS(20));
  fruitjam_lcd_use_hardware_spi(use_spi);
  fruitjam_lcd_clear();

  if (!clear_only) {
    fruitjam_lcd_write_generated_frame(prv_lcd_test_byte, NULL);
  }

  char line[160];
  const int written =
      snprintf(line, sizeof(line),
               "lcdtest mode=%s hw_spi=%u transfers=%" PRIu32 " rows=%u last_us=%" PRIu32 "\r\n",
               clear_only ? "clear" : (use_gpio ? "gpio" : "spi"),
               fruitjam_lcd_is_hardware_spi_enabled() ? 1U : 0U, fruitjam_lcd_transfer_count(),
               (unsigned)fruitjam_lcd_last_transfer_rows(), fruitjam_lcd_last_transfer_us());
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_send_rtc(void) {
  time_t seconds;
  uint16_t milliseconds;
  rtc_get_time_ms(&seconds, &milliseconds);

  struct tm time_tm;
  gmtime_r(&seconds, &time_tm);
  struct tm local_tm;
  localtime_r(&seconds, &local_tm);

  ClockRp2350PhoneTimeDebug phone_debug;
  clock_rp2350_get_phone_time_debug(&phone_debug);

  char line[640];
  const int written = snprintf(
      line, sizeof(line),
      "rtc ticks=%llu time=%ld.%03u utc=%04d-%02d-%02dT%02d:%02d:%02dZ"
      " local=%04d-%02d-%02dT%02d:%02d:%02d"
      " raw_us=%" PRIu32 " tick0=%08" PRIx32 "/%" PRIu32 "/%" PRIu32
      " tz=%u"
      " ext_present=%u ext_valid=%u ext_err=%" PRId32 " ext_read=%" PRIu32 "/%" PRIu32
      " ext_write=%" PRIu32 "/%" PRIu32 " ext_tz=%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32
      " phone_poll=%u/%" PRIu32 " phone_tick=%" PRIu32
      " phone_req=%" PRIu32 " phone_no_session=%" PRIu32 " phone_skip=%" PRIu32 "/%" PRIu32 "\r\n",
      (unsigned long long)rtc_get_ticks(), (long)seconds, (unsigned)milliseconds,
      time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min,
      time_tm.tm_sec, local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
      local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec, time_us_32(), rp2350_timer0_tick_ctrl(),
      rp2350_timer0_tick_cycles(), rp2350_timer0_tick_count(), clock_is_timezone_set() ? 1U : 0U,
      rp2350_external_rtc_is_present() ? 1U : 0U, rp2350_external_rtc_has_valid_time() ? 1U : 0U,
      rp2350_external_rtc_last_error(), rp2350_external_rtc_read_success_count(),
      rp2350_external_rtc_read_failure_count(), rp2350_external_rtc_write_success_count(),
      rp2350_external_rtc_write_failure_count(),
      rp2350_external_rtc_timezone_read_success_count(),
      rp2350_external_rtc_timezone_read_failure_count(),
      rp2350_external_rtc_timezone_write_success_count(),
      rp2350_external_rtc_timezone_write_failure_count(),
      phone_debug.polling_enabled ? 1U : 0U, phone_debug.poll_seconds, phone_debug.timer_count,
      phone_debug.request_count, phone_debug.no_session_count, phone_debug.skip_manual_count,
      phone_debug.skip_external_rtc_count);

  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_handle_timezone_set_command(const char *args) {
  char region[TIMEZONE_NAME_LENGTH] = {0};
  (void)prv_read_token(args, region, sizeof(region));

  if (region[0] == '\0') {
    prv_usb_write_str("tzset error=missing-region\r\n");
    return;
  }

  if (strcmp(region, "auto") == 0) {
    clock_set_manual_timezone_source(false);
    clock_request_time_from_phone();
    prv_send_rtc();
    return;
  }

  const int region_id = timezone_database_find_region_by_name(region, strlen(region));
  if (region_id < 0) {
    char line[96];
    const int written =
        snprintf(line, sizeof(line), "tzset error=unknown-region region=%s\r\n", region);
    prv_usb_write_formatted_line(line, written, sizeof(line));
    return;
  }

  clock_set_manual_timezone_source(true);
  clock_set_timezone_by_region_id((uint16_t)region_id);

  char resolved_region[TIMEZONE_NAME_LENGTH] = {0};
  (void)timezone_database_load_region_name((uint16_t)region_id, resolved_region);

  char line[128];
  const int written =
      snprintf(line, sizeof(line), "tzset ok id=%d region=%s\r\n", region_id, resolved_region);
  prv_usb_write_formatted_line(line, written, sizeof(line));
  prv_send_rtc();
}

static void prv_request_phone_time(void) {
  clock_request_time_from_phone();
  prv_send_rtc();
}

static void prv_handle_rtc_set_command(const char *args) {
  time_t timestamp;
  if (!prv_parse_timestamp_arg(args, &timestamp)) {
    char line[128];
    const int written =
        snprintf(line, sizeof(line), "rtcset usage: rtcset <unix_utc_seconds> range=%lu..%lu\r\n",
                 RTC_SET_MIN_TIMESTAMP, RTC_SET_MAX_TIMESTAMP);
    prv_usb_write_formatted_line(line, written, sizeof(line));
    return;
  }

  const uint32_t write_success_before = rp2350_external_rtc_write_success_count();
  const uint32_t write_failure_before = rp2350_external_rtc_write_failure_count();
  clock_set_time(timestamp);

  struct tm time_tm;
  gmtime_r(&timestamp, &time_tm);

  const bool write_succeeded = rp2350_external_rtc_write_success_count() > write_success_before;
  const bool write_failed = rp2350_external_rtc_write_failure_count() > write_failure_before;

  char line[192];
  const int written = snprintf(
      line, sizeof(line),
      "rtcset epoch=%ld utc=%04d-%02d-%02dT%02d:%02d:%02dZ ext_write=%s err=%" PRId32 "\r\n",
      (long)timestamp, time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour,
      time_tm.tm_min, time_tm.tm_sec,
      write_succeeded ? "ok" : (write_failed ? "fail" : "unchanged"),
      rp2350_external_rtc_last_error());
  prv_usb_write_formatted_line(line, written, sizeof(line));
  prv_send_rtc();
}

static void prv_send_i2c_scan(void) {
  uint8_t addresses[I2C_SCAN_MAX_ADDRESSES];
  const uint32_t found_count = rp2350_external_rtc_scan(addresses, I2C_SCAN_MAX_ADDRESSES);
  char line[192];
  int offset = snprintf(line, sizeof(line), "i2cscan count=%" PRIu32 " addrs=", found_count);

  if (offset < 0) {
    return;
  }

  if (found_count == 0U) {
    offset += snprintf(&line[offset], sizeof(line) - (size_t)offset, "none");
  } else {
    const uint32_t shown_count =
        (found_count < I2C_SCAN_MAX_ADDRESSES) ? found_count : I2C_SCAN_MAX_ADDRESSES;
    for (uint32_t i = 0; i < shown_count && (size_t)offset < sizeof(line) - 8U; ++i) {
      offset += snprintf(&line[offset], sizeof(line) - (size_t)offset, "%s%02x",
                         (i == 0U) ? "" : ",", addresses[i]);
    }
    if (found_count > shown_count && (size_t)offset < sizeof(line) - 12U) {
      offset += snprintf(&line[offset], sizeof(line) - (size_t)offset, ",...");
    }
  }

  if ((size_t)offset < sizeof(line) - 3U) {
    offset += snprintf(&line[offset], sizeof(line) - (size_t)offset, "\r\n");
  }
  prv_usb_write_formatted_line(line, offset, sizeof(line));
}

static void prv_send_tasks(void) {
  TaskStatus_t tasks[TASK_SNAPSHOT_COUNT];
  const UBaseType_t total = uxTaskGetNumberOfTasks();
  const UBaseType_t count = uxTaskGetSystemState(tasks, TASK_SNAPSHOT_COUNT, NULL);
  char line[128];
  int written = snprintf(line, sizeof(line), "tasks total=%lu captured=%lu\r\n",
                         (unsigned long)total, (unsigned long)count);

  prv_usb_write_formatted_line(line, written, sizeof(line));

  if (count == 0U && total > TASK_SNAPSHOT_COUNT) {
    prv_usb_write_str("tasks snapshot too small\r\n");
    return;
  }

  for (UBaseType_t i = 0; i < count; ++i) {
    written = snprintf(
        line, sizeof(line), "task %-12s %c pri=%lu base=%lu stack=%u\r\n", tasks[i].pcTaskName,
        prv_task_state_char(tasks[i].eCurrentState), (unsigned long)tasks[i].uxCurrentPriority,
        (unsigned long)tasks[i].uxBasePriority, (unsigned)tasks[i].usStackHighWaterMark);
    prv_usb_write_formatted_line(line, written, sizeof(line));
  }
}

static char prv_button_state_char(uint32_t state, ButtonId id) {
  return (state & (1U << id)) ? '1' : '0';
}

static const char *prv_button_name(ButtonId id) {
  switch (id) {
    case BUTTON_ID_BACK:
      return "back";
    case BUTTON_ID_UP:
      return "up";
    case BUTTON_ID_SELECT:
      return "select";
    case BUTTON_ID_DOWN:
      return "down";
    case NUM_BUTTONS:
      return "none";
  }

  return "?";
}

static const char *prv_pull_name(GPIOPuPd_TypeDef pull) {
  switch (pull) {
    case GPIO_PuPd_NOPULL:
      return "none";
    case GPIO_PuPd_UP:
      return "up";
    case GPIO_PuPd_DOWN:
      return "down";
  }

  return "?";
}

static void prv_send_buttonmap(void) {
  char line[160];
  int written = snprintf(
      line, sizeof(line),
      "buttonmap poll_ms=%u debounce_ms=%u down_combo=up+select combo_grace_ms=%u "
      "bootsel_hold_ms=%u\r\n",
      (unsigned)RP2350_BUTTON_POLL_PERIOD_MS,
      (unsigned)(RP2350_BUTTON_POLL_PERIOD_MS * RP2350_BUTTON_DEBOUNCE_SAMPLES),
      (unsigned)RP2350_BUTTON_DOWN_COMBO_GRACE_MS, (unsigned)RP2350_BUTTON_BOOTSEL_HOLD_MS);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  for (ButtonId id = BUTTON_ID_BACK; id < NUM_BUTTONS; ++id) {
    const ButtonConfig *config = &BOARD_CONFIG_BUTTON.buttons[id];
    const bool is_physical = (config->pin != GPIO_Pin_NULL && config->port != GPIO_Port_NULL);

    if (is_physical) {
      written =
          snprintf(line, sizeof(line), "buttonmap %-6s pin=%u pull=%s active=%s physical=1\r\n",
                   prv_button_name(id), (unsigned)config->pin, prv_pull_name(config->pull),
                   config->active_high ? "high" : "low");
    } else if (id == BUTTON_ID_DOWN) {
      written = snprintf(line, sizeof(line),
                         "buttonmap %-6s pin=none pull=none active=none physical=0 "
                         "synthetic=up+select\r\n",
                         prv_button_name(id));
    } else {
      written = snprintf(line, sizeof(line),
                         "buttonmap %-6s pin=none pull=none active=none physical=0\r\n",
                         prv_button_name(id));
    }
    prv_usb_write_formatted_line(line, written, sizeof(line));
  }
}

static void prv_send_buttons(void) {
  FruitJamButtonDebugSnapshot snapshot;
  char line[192];

  button_debug_get_snapshot(&snapshot);
  const uint32_t live_state = button_get_state_bits();

  int written = snprintf(line, sizeof(line),
                         "buttons level=%c%c%c%c raw=%c%c%c%c debounced=%c%c%c%c emitted=%c%c%c%c"
                         " injected=%c%c%c%c live=%c%c%c%c combo=%u rotated=%u\r\n",
                         prv_button_state_char(snapshot.pin_level_state, BUTTON_ID_BACK),
                         prv_button_state_char(snapshot.pin_level_state, BUTTON_ID_UP),
                         prv_button_state_char(snapshot.pin_level_state, BUTTON_ID_SELECT),
                         prv_button_state_char(snapshot.pin_level_state, BUTTON_ID_DOWN),
                         prv_button_state_char(snapshot.raw_physical_state, BUTTON_ID_BACK),
                         prv_button_state_char(snapshot.raw_physical_state, BUTTON_ID_UP),
                         prv_button_state_char(snapshot.raw_physical_state, BUTTON_ID_SELECT),
                         prv_button_state_char(snapshot.raw_physical_state, BUTTON_ID_DOWN),
                         prv_button_state_char(snapshot.debounced_physical_state, BUTTON_ID_BACK),
                         prv_button_state_char(snapshot.debounced_physical_state, BUTTON_ID_UP),
                         prv_button_state_char(snapshot.debounced_physical_state, BUTTON_ID_SELECT),
                         prv_button_state_char(snapshot.debounced_physical_state, BUTTON_ID_DOWN),
                         prv_button_state_char(snapshot.emitted_state, BUTTON_ID_BACK),
                         prv_button_state_char(snapshot.emitted_state, BUTTON_ID_UP),
                         prv_button_state_char(snapshot.emitted_state, BUTTON_ID_SELECT),
                         prv_button_state_char(snapshot.emitted_state, BUTTON_ID_DOWN),
                         prv_button_state_char(snapshot.injected_state, BUTTON_ID_BACK),
                         prv_button_state_char(snapshot.injected_state, BUTTON_ID_UP),
                         prv_button_state_char(snapshot.injected_state, BUTTON_ID_SELECT),
                         prv_button_state_char(snapshot.injected_state, BUTTON_ID_DOWN),
                         prv_button_state_char(live_state, BUTTON_ID_BACK),
                         prv_button_state_char(live_state, BUTTON_ID_UP),
                         prv_button_state_char(live_state, BUTTON_ID_SELECT),
                         prv_button_state_char(live_state, BUTTON_ID_DOWN),
                         snapshot.down_combo_active ? 1U : 0U, snapshot.rotated_180 ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "buttons pending=%s samples=%" PRIu32
                     " suppress=%c%c%c%c"
                     " bootsel=%" PRIu32 " events=%" PRIu32 " last=%s:%s\r\n",
                     prv_button_name(snapshot.pending_button), snapshot.pending_samples,
                     prv_button_state_char(snapshot.suppress_until_release_mask, BUTTON_ID_BACK),
                     prv_button_state_char(snapshot.suppress_until_release_mask, BUTTON_ID_UP),
                     prv_button_state_char(snapshot.suppress_until_release_mask, BUTTON_ID_SELECT),
                     prv_button_state_char(snapshot.suppress_until_release_mask, BUTTON_ID_DOWN),
                     snapshot.bootsel_hold_samples, snapshot.event_count,
                     prv_button_name(snapshot.last_event_button),
                     snapshot.last_event_down ? "down" : "up");
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static bool prv_button_watch_changed(const FruitJamButtonDebugSnapshot *previous,
                                     const FruitJamButtonDebugSnapshot *current,
                                     uint32_t previous_live_state, uint32_t live_state) {
  return previous->pin_level_state != current->pin_level_state ||
         previous->raw_physical_state != current->raw_physical_state ||
         previous->debounced_physical_state != current->debounced_physical_state ||
         previous->emitted_state != current->emitted_state ||
         previous->injected_state != current->injected_state ||
         previous->suppress_until_release_mask != current->suppress_until_release_mask ||
         previous->pending_button != current->pending_button ||
         previous->bootsel_hold_samples != current->bootsel_hold_samples ||
         previous->event_count != current->event_count ||
         previous->last_event_button != current->last_event_button ||
         previous->last_event_down != current->last_event_down ||
         previous->down_combo_active != current->down_combo_active ||
         previous->rotated_180 != current->rotated_180 || previous_live_state != live_state;
}

static void prv_send_button_watch_line(uint32_t elapsed_ms,
                                       const FruitJamButtonDebugSnapshot *snapshot,
                                       uint32_t live_state) {
  char line[256];
  const int written = snprintf(
      line, sizeof(line),
      "buttonwatch t=%" PRIu32
      " level=%c%c%c%c raw=%c%c%c%c debounced=%c%c%c%c emitted=%c%c%c%c "
      "live=%c%c%c%c combo=%u pending=%s suppress=%c%c%c%c bootsel=%" PRIu32 " events=%" PRIu32
      " last=%s:%s\r\n",
      elapsed_ms, prv_button_state_char(snapshot->pin_level_state, BUTTON_ID_BACK),
      prv_button_state_char(snapshot->pin_level_state, BUTTON_ID_UP),
      prv_button_state_char(snapshot->pin_level_state, BUTTON_ID_SELECT),
      prv_button_state_char(snapshot->pin_level_state, BUTTON_ID_DOWN),
      prv_button_state_char(snapshot->raw_physical_state, BUTTON_ID_BACK),
      prv_button_state_char(snapshot->raw_physical_state, BUTTON_ID_UP),
      prv_button_state_char(snapshot->raw_physical_state, BUTTON_ID_SELECT),
      prv_button_state_char(snapshot->raw_physical_state, BUTTON_ID_DOWN),
      prv_button_state_char(snapshot->debounced_physical_state, BUTTON_ID_BACK),
      prv_button_state_char(snapshot->debounced_physical_state, BUTTON_ID_UP),
      prv_button_state_char(snapshot->debounced_physical_state, BUTTON_ID_SELECT),
      prv_button_state_char(snapshot->debounced_physical_state, BUTTON_ID_DOWN),
      prv_button_state_char(snapshot->emitted_state, BUTTON_ID_BACK),
      prv_button_state_char(snapshot->emitted_state, BUTTON_ID_UP),
      prv_button_state_char(snapshot->emitted_state, BUTTON_ID_SELECT),
      prv_button_state_char(snapshot->emitted_state, BUTTON_ID_DOWN),
      prv_button_state_char(live_state, BUTTON_ID_BACK),
      prv_button_state_char(live_state, BUTTON_ID_UP),
      prv_button_state_char(live_state, BUTTON_ID_SELECT),
      prv_button_state_char(live_state, BUTTON_ID_DOWN), snapshot->down_combo_active ? 1U : 0U,
      prv_button_name(snapshot->pending_button),
      prv_button_state_char(snapshot->suppress_until_release_mask, BUTTON_ID_BACK),
      prv_button_state_char(snapshot->suppress_until_release_mask, BUTTON_ID_UP),
      prv_button_state_char(snapshot->suppress_until_release_mask, BUTTON_ID_SELECT),
      prv_button_state_char(snapshot->suppress_until_release_mask, BUTTON_ID_DOWN),
      snapshot->bootsel_hold_samples, snapshot->event_count,
      prv_button_name(snapshot->last_event_button), snapshot->last_event_down ? "down" : "up");
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static uint32_t prv_parse_button_watch_duration_ms(const char *args) {
  char duration[12] = {0};
  (void)prv_read_token(args, duration, sizeof(duration));

  if (duration[0] == '\0') {
    return GPIO_WATCH_DEFAULT_MS;
  }

  char *end = NULL;
  const unsigned long requested_ms = strtoul(duration, &end, 10);
  if (end == duration || *end != '\0') {
    return BUTTON_WATCH_DEFAULT_MS;
  }

  if (requested_ms > BUTTON_WATCH_MAX_MS) {
    return BUTTON_WATCH_MAX_MS;
  }
  if (requested_ms < BUTTON_WATCH_PERIOD_MS) {
    return BUTTON_WATCH_PERIOD_MS;
  }

  return (uint32_t)requested_ms;
}

static uint64_t prv_gpio_level_mask(void) {
  const uint32_t lo = REG32(SIO_BASE + SIO_GPIO_IN_OFFSET);
  const uint32_t hi = REG32(SIO_BASE + SIO_GPIO_HI_IN_OFFSET);

  return (uint64_t)lo | ((uint64_t)hi << 32U);
}

static bool prv_parse_uint32(const char *text, uint32_t *value_out) {
  char *end = NULL;
  const unsigned long value = strtoul(text, &end, 10);

  if (end == text || *end != '\0') {
    return false;
  }

  *value_out = (uint32_t)value;
  return true;
}

static void prv_usb_configure_runtime(void) {
  irq_set_priority(USBCTRL_IRQ, configMAX_SYSCALL_INTERRUPT_PRIORITY);
  tud_cdc_configure_fifo(&(const tud_cdc_configure_fifo_t){
      .rx_persistent = true,
      .tx_persistent = true,
  });
}

static bool prv_parse_timestamp_arg(const char *args, time_t *time_out) {
  char token[16] = {0};
  (void)prv_read_token(args, token, sizeof(token));
  if (token[0] == '\0') {
    return false;
  }

  char *end = NULL;
  const unsigned long value = strtoul(token, &end, 10);
  if (end == token || *end != '\0' || value < RTC_SET_MIN_TIMESTAMP ||
      value > RTC_SET_MAX_TIMESTAMP) {
    return false;
  }

  *time_out = (time_t)value;
  return true;
}

static uint32_t prv_parse_gpio_watch_duration_ms(const char *args) {
  char duration[12] = {0};
  uint32_t requested_ms;

  (void)prv_read_token(args, duration, sizeof(duration));

  if (duration[0] == '\0') {
    return GPIO_WATCH_DEFAULT_MS;
  }

  if (!prv_parse_uint32(duration, &requested_ms)) {
    return GPIO_WATCH_DEFAULT_MS;
  }

  if (requested_ms > GPIO_WATCH_MAX_MS) {
    return GPIO_WATCH_MAX_MS;
  }
  if (requested_ms < GPIO_WATCH_PERIOD_MS) {
    return GPIO_WATCH_PERIOD_MS;
  }

  return (uint32_t)requested_ms;
}

static uint64_t prv_parse_gpio_watch_mask(const char *args) {
  char duration[12] = {0};
  char selector[96] = {0};
  const char *next = prv_read_token(args, duration, sizeof(duration));
  uint64_t mask = 0U;

  (void)prv_read_token(next, selector, sizeof(selector));
  if (selector[0] == '\0') {
    return GPIO_WATCH_ALL_MASK & ~GPIO_WATCH_DEFAULT_IGNORE_MASK;
  }
  if (strcmp(selector, "all") == 0) {
    return GPIO_WATCH_ALL_MASK;
  }

  const char *cursor = selector;
  while (*cursor != '\0') {
    char *end = NULL;
    const unsigned long pin = strtoul(cursor, &end, 10);

    if (end == cursor || pin >= GPIO_WATCH_PIN_COUNT) {
      return GPIO_WATCH_ALL_MASK & ~GPIO_WATCH_DEFAULT_IGNORE_MASK;
    }

    mask |= UINT64_C(1) << pin;
    if (*end == '\0') {
      break;
    }
    if (*end != ',') {
      return GPIO_WATCH_ALL_MASK & ~GPIO_WATCH_DEFAULT_IGNORE_MASK;
    }
    cursor = end + 1;
  }

  return mask;
}

static int prv_append_gpio_change(char *line, size_t size, int offset, uint32_t pin, bool level,
                                  bool *first) {
  if (offset < 0 || (size_t)offset >= size - 8U) {
    return offset;
  }

  const size_t remaining = size - (size_t)offset;
  const int written = snprintf(&line[offset], remaining, "%s%" PRIu32 ":%u", *first ? "" : ",", pin,
                               level ? 1U : 0U);
  *first = false;
  if (written < 0) {
    return offset;
  }
  if ((size_t)written >= remaining) {
    return (int)size - 1;
  }
  return offset + written;
}

static int prv_append_gpio_line_end(char *line, size_t size, int offset) {
  if (offset < 0) {
    return offset;
  }

  if ((size_t)offset >= size - 3U) {
    offset = (int)size - 4;
    line[offset++] = '.';
  }

  const int written = snprintf(&line[offset], size - (size_t)offset, "\r\n");
  return (written < 0) ? offset : offset + written;
}

static void prv_send_gpio_watch_line(uint32_t elapsed_ms, uint64_t levels, uint64_t changed) {
  char line[256];
  const uint32_t lo = (uint32_t)levels;
  const uint32_t hi = (uint32_t)(levels >> 32U);
  int offset =
      snprintf(line, sizeof(line),
               "gpiowatch t=%" PRIu32 " lo=%08" PRIx32 " hi=%04" PRIx32 " changed=", elapsed_ms, lo,
               hi & 0xffffU);
  if (offset < 0) {
    return;
  }

  if (changed == 0U) {
    const int written = snprintf(&line[offset], sizeof(line) - (size_t)offset, "none");
    if (written > 0) {
      offset += written;
    }
  } else {
    bool first = true;
    for (uint32_t pin = 0; pin < GPIO_WATCH_PIN_COUNT; ++pin) {
      const uint64_t mask = UINT64_C(1) << pin;
      if ((changed & mask) == 0U) {
        continue;
      }
      offset =
          prv_append_gpio_change(line, sizeof(line), offset, pin, (levels & mask) != 0U, &first);
      if ((size_t)offset >= sizeof(line) - 8U) {
        break;
      }
    }
  }

  offset = prv_append_gpio_line_end(line, sizeof(line), offset);
  prv_usb_write_formatted_line(line, offset, sizeof(line));
}

static void prv_send_gpio(void) {
  const uint64_t levels = prv_gpio_level_mask();
  char line[96];
  const int written = snprintf(line, sizeof(line), "gpio lo=%08" PRIx32 " hi=%04" PRIx32 "\r\n",
                               (uint32_t)levels, (uint32_t)(levels >> 32U) & 0xffffU);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_handle_gpio_watch_command(const char *args) {
  const uint32_t duration_ms = prv_parse_gpio_watch_duration_ms(args);
  const uint64_t watch_mask = prv_parse_gpio_watch_mask(args);
  uint64_t previous_levels = prv_gpio_level_mask() & watch_mask;
  uint32_t elapsed_ms = 0U;
  uint32_t next_heartbeat_ms = 0U;
  uint32_t lines = 0U;
  uint32_t changes = 0U;
  char line[128];

  int written =
      snprintf(line, sizeof(line),
               "gpiowatch start ms=%" PRIu32
               " period_ms=%u heartbeat_ms=%u "
               "mask_lo=%08" PRIx32 " mask_hi=%04" PRIx32 "\r\n",
               duration_ms, (unsigned)GPIO_WATCH_PERIOD_MS, (unsigned)GPIO_WATCH_HEARTBEAT_MS,
               (uint32_t)watch_mask, (uint32_t)(watch_mask >> 32U) & 0xffffU);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  while (elapsed_ms <= duration_ms && tud_cdc_connected()) {
    const uint64_t levels = prv_gpio_level_mask() & watch_mask;
    const uint64_t changed = levels ^ previous_levels;
    const bool heartbeat_due = elapsed_ms >= next_heartbeat_ms;

    if (changed != 0U || heartbeat_due) {
      prv_send_gpio_watch_line(elapsed_ms, levels, changed);
      ++lines;
      if (changed != 0U) {
        ++changes;
        previous_levels = levels;
      }

      if (heartbeat_due) {
        next_heartbeat_ms = elapsed_ms + GPIO_WATCH_HEARTBEAT_MS;
      }
    }

    tud_task();
    vTaskDelay(pdMS_TO_TICKS(GPIO_WATCH_PERIOD_MS));
    elapsed_ms += GPIO_WATCH_PERIOD_MS;
  }

  written = snprintf(line, sizeof(line), "gpiowatch done changes=%" PRIu32 " lines=%" PRIu32 "\r\n",
                     changes, lines);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_handle_button_watch_command(const char *args) {
  const uint32_t duration_ms = prv_parse_button_watch_duration_ms(args);
  uint32_t elapsed_ms = 0U;
  uint32_t change_count = 0U;
  uint32_t line_count = 0U;
  uint32_t previous_live_state = 0U;
  bool have_previous = false;
  FruitJamButtonDebugSnapshot previous = {0};
  char line[96];

  int written = snprintf(
      line, sizeof(line), "buttonwatch start ms=%" PRIu32 " period_ms=%u heartbeat_ms=%u\r\n",
      duration_ms, (unsigned)BUTTON_WATCH_PERIOD_MS, (unsigned)BUTTON_WATCH_HEARTBEAT_MS);
  prv_usb_write_formatted_line(line, written, sizeof(line));
  tud_cdc_write_flush();

  while (elapsed_ms <= duration_ms && tud_cdc_connected()) {
    FruitJamButtonDebugSnapshot snapshot;
    button_debug_get_snapshot(&snapshot);
    const uint32_t live_state = button_get_state_bits();
    const bool changed =
        !have_previous ||
        prv_button_watch_changed(&previous, &snapshot, previous_live_state, live_state);
    const bool heartbeat_due = have_previous && ((elapsed_ms % BUTTON_WATCH_HEARTBEAT_MS) == 0U);

    if (changed || heartbeat_due) {
      prv_send_button_watch_line(elapsed_ms, &snapshot, live_state);
      line_count++;
      if (changed) {
        previous = snapshot;
        previous_live_state = live_state;
        have_previous = true;
        change_count++;
      }
      tud_cdc_write_flush();
    }

    tud_task();
    vTaskDelay(pdMS_TO_TICKS(BUTTON_WATCH_PERIOD_MS));
    elapsed_ms += BUTTON_WATCH_PERIOD_MS;
  }

  written =
      snprintf(line, sizeof(line), "buttonwatch done changes=%" PRIu32 " lines=%" PRIu32 "\r\n",
               change_count, line_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));
  tud_cdc_write_flush();
}

static bool prv_parse_button_name(const char *name, ButtonId *id_out) {
  if (strcmp(name, "back") == 0) {
    *id_out = BUTTON_ID_BACK;
    return true;
  }
  if (strcmp(name, "up") == 0) {
    *id_out = BUTTON_ID_UP;
    return true;
  }
  if (strcmp(name, "select") == 0) {
    *id_out = BUTTON_ID_SELECT;
    return true;
  }
  if (strcmp(name, "down") == 0) {
    *id_out = BUTTON_ID_DOWN;
    return true;
  }

  return false;
}

static void prv_send_button_usage(void) {
  prv_usb_write_str("usage: button back|up|select|down [tap|down|up]\r\n");
}

static const char *prv_read_token(const char *input, char *out, size_t out_size) {
  while (*input == ' ') {
    ++input;
  }

  size_t used = 0U;
  while (*input != '\0' && *input != ' ') {
    if (used + 1U < out_size) {
      out[used++] = *input;
    }
    ++input;
  }
  if (out_size != 0U) {
    out[used] = '\0';
  }

  while (*input == ' ') {
    ++input;
  }
  return input;
}

static void prv_handle_button_command(const char *args) {
  char button_name[12] = {0};
  char action[8] = "tap";
  ButtonId id;

  const char *next = prv_read_token(args, button_name, sizeof(button_name));
  if (button_name[0] == '\0' || !prv_parse_button_name(button_name, &id)) {
    prv_send_button_usage();
    return;
  }
  char parsed_action[8] = {0};
  (void)prv_read_token(next, parsed_action, sizeof(parsed_action));
  if (parsed_action[0] != '\0') {
    strncpy(action, parsed_action, sizeof(action) - 1U);
  }

  bool ok = true;
  if (strcmp(action, "tap") == 0) {
    ok = button_debug_tap(id);
  } else if (strcmp(action, "down") == 0) {
    ok = button_debug_emit_event(id, true);
  } else if (strcmp(action, "up") == 0) {
    ok = button_debug_emit_event(id, false);
  } else {
    prv_send_button_usage();
    return;
  }

  if (!ok) {
    prv_usb_write_str("button injection failed\r\n");
    return;
  }

  char line[48];
  const int written = snprintf(line, sizeof(line), "button %s %s\r\n", prv_button_name(id), action);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static const char *prv_app_type_name(const AppInstallEntry *entry) {
  return app_install_entry_is_watchface(entry) ? "watchface" : "app";
}

static bool prv_send_app_entry(AppInstallEntry *entry, void *data) {
  (void)data;

  char uuid[UUID_STRING_BUFFER_LENGTH];
  char line[192];

  uuid_to_string(&entry->uuid, uuid);
  const int written =
      snprintf(line, sizeof(line), "app %" PRId32 " %s sdk=%u.%u name=\"%.64s\" uuid=%s\r\n",
               entry->install_id, prv_app_type_name(entry), entry->sdk_version.major,
               entry->sdk_version.minor, entry->name, uuid);
  prv_usb_write_formatted_line(line, written, sizeof(line));
  return true;
}

static void prv_send_apps(void) {
  prv_usb_write_str("apps begin\r\n");
  app_install_enumerate_entries(prv_send_app_entry, NULL);
  prv_usb_write_str("apps end\r\n");
}

static void prv_send_current_app(void) {
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  char uuid[UUID_STRING_BUFFER_LENGTH];
  char line[192];

  uuid_to_string(&md->uuid, uuid);
  const int written =
      snprintf(line, sizeof(line), "appcur %" PRId32 " name=\"%s\" uuid=%s watchface=%u\r\n",
               app_manager_get_current_app_id(), process_metadata_get_name(md), uuid,
               app_manager_is_watchface_running() ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_handle_app_check_command(const char *args) {
  const int32_t id = atoi(args);
  if (id <= INSTALL_ID_INVALID) {
    prv_usb_write_str("usage: appcheck <positive_install_id>\r\n");
    return;
  }

  AppInstallEntry entry = {0};
  PebbleProcessInfo info = {0};
  const bool appdb_exists = app_install_get_entry_for_install_id((AppInstallId)id, &entry);
  const AppStorageGetAppInfoResult info_result =
      app_storage_get_process_info(&info, NULL, (AppInstallId)id, PebbleTask_App);
  const bool resources_ok = resource_storage_check((ResAppNum)id, 0, NULL);
  const bool storage_exists = app_storage_app_exists((AppInstallId)id);
  const bool cache_exists = app_cache_entry_exists((AppInstallId)id);

  char line[256];
  int written =
      snprintf(line, sizeof(line),
               "appcheck id=%" PRId32
               " appdb=%u type=%s cache=%u storage=%u info=%u "
               "resources=%u\r\n",
               id, appdb_exists ? 1U : 0U, appdb_exists ? prv_app_type_name(&entry) : "unknown",
               cache_exists ? 1U : 0U, storage_exists ? 1U : 0U, (unsigned)info_result,
               resources_ok ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  char db_uuid[UUID_STRING_BUFFER_LENGTH];
  char file_uuid[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(appdb_exists ? &entry.uuid : NULL, db_uuid);
  Uuid info_uuid = UUID_INVALID;
  if (info_result == GET_APP_INFO_SUCCESS) {
    memcpy(&info_uuid, &info.uuid, sizeof(info_uuid));
  }
  uuid_to_string((info_result == GET_APP_INFO_SUCCESS) ? &info_uuid : NULL, file_uuid);

  written = snprintf(line, sizeof(line),
                     "appcheck db=\"%.64s\" db_uuid=%s file=\"%.64s\" file_uuid=%s\r\n",
                     appdb_exists ? entry.name : "", db_uuid,
                     (info_result == GET_APP_INFO_SUCCESS) ? info.name : "", file_uuid);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static const char *prv_app_fetch_result_name(AppFetchResult result) {
  switch (result) {
    case AppFetchResultSuccess:
      return "success";
    case AppFetchResultTimeoutError:
      return "timeout";
    case AppFetchResultGeneralFailure:
      return "general";
    case AppFetchResultPhoneBusy:
      return "phone-busy";
    case AppFetchResultUUIDInvalid:
      return "uuid-invalid";
    case AppFetchResultNoBluetooth:
      return "no-bluetooth";
    case AppFetchResultPutBytesFailure:
      return "putbytes";
    case AppFetchResultNoData:
      return "no-data";
    case AppFetchResultUserCancelled:
      return "cancelled";
    case AppFetchResultIncompatibleJSFailure:
      return "bad-js";
  }
  return "unknown";
}

static const char *prv_app_fetch_response_name(uint8_t result) {
  switch (result) {
    case 1:
      return "start";
    case 2:
      return "busy";
    case 3:
      return "uuid-invalid";
    case 4:
      return "no-data";
  }
  return "unknown";
}

static void prv_send_app_fetch(void) {
  AppFetchDebugInfo fetch = {0};
  PutBytesDebugInfo put = {0};
  char uuid[UUID_STRING_BUFFER_LENGTH];
  char line[512];

  app_fetch_get_debug_info(&fetch);
  put_bytes_get_debug_info(&put);
  uuid_to_string(&fetch.request_uuid, uuid);

  int written = snprintf(
      line, sizeof(line),
      "appfetch in=%u id=%" PRId32
      " uuid=%s prev=%u:%s cleanup=%u:%s "
      "req=%" PRIu32 " ok=%" PRIu32 " fail=%" PRIu32 " resp=%" PRIu32
      " short=%" PRIu32 " invalid=%" PRIu32 " len=%" PRIu32
      " last_resp=%u/%u:%s started=%u done=%u/%u/%u pb_ev=%" PRIu32 " start=%" PRIu32
      " prog=%" PRIu32
      " clean=%" PRIu32 " timeout=%" PRIu32 " pbfail=%" PRIu32
      " last_pb=%u obj=%u pct=%u failed=%u\r\n",
      fetch.in_progress ? 1U : 0U, fetch.app_id, uuid, (unsigned)fetch.prev_error,
      prv_app_fetch_result_name(fetch.prev_error), (unsigned)fetch.last_cleanup_result,
      prv_app_fetch_result_name(fetch.last_cleanup_result), fetch.request_count,
      fetch.request_send_ok_count, fetch.request_send_fail_count, fetch.response_count,
      fetch.response_short_count, fetch.response_invalid_count, fetch.last_response_length,
      fetch.last_response_command, fetch.last_response_result,
      prv_app_fetch_response_name(fetch.last_response_result), fetch.phone_started ? 1U : 0U,
      fetch.app_done ? 1U : 0U, fetch.worker_done ? 1U : 0U,
      fetch.resources_done ? 1U : 0U, fetch.put_bytes_event_count, fetch.put_bytes_start_count,
      fetch.put_bytes_progress_count, fetch.put_bytes_cleanup_count,
      fetch.put_bytes_timeout_count, fetch.put_bytes_failure_count,
      fetch.last_put_bytes_event_type, fetch.last_put_bytes_object_type,
      fetch.last_put_bytes_progress_percent, fetch.last_put_bytes_failed ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(
      line, sizeof(line),
      "putbytes lock=%u cmd=%u obj=%u cookie=%u ok=%u tok=%" PRIu32 " idx=%" PRIu32
      " total=%" PRIu32 " rem=%" PRIu32 " off=%" PRIu32 " cur=%" PRIu32 " recv=%" PRIu32 "/%" PRIu32
      " nack=%u jobs=%u/%u/%u preack=%u ack_later=%u counts=%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32
      " cleanup=%" PRIu32 " timeout=%" PRIu32 " fail=%" PRIu32 " ack=%" PRIu32 " nack=%" PRIu32
      " storefail=%" PRIu32 " crcfail=%" PRIu32 " prepfail=%" PRIu32 " last=%u resp=%u len=%" PRIu32
      " data=%" PRIu32 " crc=%08" PRIx32 "/%08" PRIx32 "\r\n",
      put.lock_ok ? 1U : 0U, put.current_command, put.object_type, put.has_cookie, put.is_success,
      put.token, put.index, put.total_size, put.remaining_bytes, put.append_offset,
      put.current_offset, put.receiver_pos, put.receiver_length, put.receiver_should_nack,
      put.pending_jobs, put.allocated_jobs, put.read_idx, put.preack_enabled, put.ack_later,
      put.init_count, put.put_count,
      put.commit_count, put.abort_count, put.install_count, put.cleanup_count, put.timeout_count,
      put.fail_count, put.ack_count, put.nack_count, put.storage_init_fail_count,
      put.crc_fail_count, put.prepare_fail_count, put.last_command, put.last_response_code,
      put.request_length, put.last_data_length, put.last_crc, put.last_expected_crc);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_send_storage(void) {
  AppFetchDebugInfo fetch = {0};
  PutBytesDebugInfo put = {0};
  FlashDebugInfo flash = {0};
  char line[512];

  app_fetch_get_debug_info(&fetch);
  put_bytes_get_debug_info(&put);
  flash_get_debug_info(&flash);

  const int written =
      snprintf(line, sizeof(line),
               "storage pfs_active=%u pfs_size=%" PRIu32 " pfs_free=%" PRIu32
               " appfetch_in=%u cleanup=%u:%s storefail=%" PRIu32 " crcfail=%" PRIu32
               " prepfail=%" PRIu32
               " storage=%s xip_hazard=%u flash=%s hazard=%u write=%" PRIu32 "/%" PRIu32
               " erase=%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32
               " xip=%" PRIu32 "/%" PRIu32 "/%" PRIu32
               " block_us=%" PRIu32 "/%" PRIu32 " last=%08" PRIx32 "/%" PRIu32 "\r\n",
               pfs_active() ? 1U : 0U, pfs_get_size(), get_available_pfs_space(),
               fetch.in_progress ? 1U : 0U, (unsigned)fetch.last_cleanup_result,
               prv_app_fetch_result_name(fetch.last_cleanup_result), put.storage_init_fail_count,
               put.crc_fail_count, put.prepare_fail_count, RP2350_PEBBLE_STORAGE_NAME,
               RP2350_PEBBLE_STORAGE_HAZARD,
               flash.backend_is_rp2350_xip ? "rp2350-xip" : "external",
               flash.backend_is_rp2350_xip ? 1U : 0U, flash.write_calls, flash.write_bytes,
               flash.erase_async_calls, flash.erase_blocking_calls,
               flash.erase_subsector_calls, flash.erase_sector_calls, flash.xip_ops,
               flash.xip_program_ops, flash.xip_erase_ops, flash.xip_last_block_us,
               flash.xip_max_block_us, flash.xip_last_offset, flash.xip_last_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_launch_app_callback(void *data) {
  const AppInstallId id = (AppInstallId)(intptr_t)data;
  app_manager_put_launch_app_event(&(AppLaunchEventConfig){
      .id = id,
      .common.reason = APP_LAUNCH_PHONE,
  });
}

static void prv_handle_app_launch_command(const char *args) {
  const int32_t id = atoi(args);
  AppInstallEntry entry;

  if (id == INSTALL_ID_INVALID) {
    prv_usb_write_str("usage: applaunch <install_id>\r\n");
    return;
  }

  if (!app_install_get_entry_for_install_id((AppInstallId)id, &entry)) {
    prv_usb_write_str("applaunch no such app\r\n");
    return;
  }

  launcher_task_add_callback(prv_launch_app_callback, (void *)(intptr_t)id);

  char line[160];
  const int written =
      snprintf(line, sizeof(line), "applaunch queued %" PRId32 " %s name=\"%.64s\"\r\n", id,
               prv_app_type_name(&entry), entry.name);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_set_default_watchface_callback(void *data) {
  const AppInstallId id = (AppInstallId)(intptr_t)data;
  watchface_set_default_install_id(id);
  prv_launch_app_callback(data);
}

static void prv_launch_default_watchface_callback(void *data) {
  (void)data;
  watchface_launch_default(NULL);
}

static void prv_handle_watch_command(void) {
  const AppInstallId id = watchface_get_default_install_id();
  AppInstallEntry entry;

  launcher_task_add_callback(prv_launch_default_watchface_callback, NULL);

  char line[160];
  if (app_install_get_entry_for_install_id(id, &entry)) {
    const int written = snprintf(line, sizeof(line), "watch queued %" PRId32 " name=\"%.64s\"\r\n",
                                 (int32_t)id, entry.name);
    prv_usb_write_formatted_line(line, written, sizeof(line));
  } else {
    const int written = snprintf(line, sizeof(line), "watch queued %" PRId32 "\r\n", (int32_t)id);
    prv_usb_write_formatted_line(line, written, sizeof(line));
  }
}

static void prv_handle_watch_default_command(const char *args) {
  const int32_t id = atoi(args);
  AppInstallEntry entry;

  if (id == INSTALL_ID_INVALID) {
    prv_usb_write_str("usage: watchdefault <install_id>\r\n");
    return;
  }

  if (!app_install_get_entry_for_install_id((AppInstallId)id, &entry)) {
    prv_usb_write_str("watchdefault no such app\r\n");
    return;
  }

  if (!app_install_entry_is_watchface(&entry)) {
    prv_usb_write_str("watchdefault not a watchface\r\n");
    return;
  }

  launcher_task_add_callback(prv_set_default_watchface_callback, (void *)(intptr_t)id);

  char line[160];
  const int written = snprintf(
      line, sizeof(line), "watchdefault queued %" PRId32 " name=\"%.64s\"\r\n", id, entry.name);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_app_sideload_get_filename(AppSideloadPart part, char *filename, size_t size) {
  if (part == AppSideloadPartApp) {
    app_storage_get_file_name(filename, size, s_app_sideload.install_id, PebbleTask_App);
  } else {
    resource_storage_get_file_name(filename, size, (ResAppNum)s_app_sideload.install_id);
  }
}

static uint32_t prv_app_sideload_total_for_part(AppSideloadPart part) {
  return (part == AppSideloadPartApp) ? s_app_sideload.app_size : s_app_sideload.resources_size;
}

static uint32_t *prv_app_sideload_written_for_part(AppSideloadPart part) {
  return (part == AppSideloadPartApp) ? &s_app_sideload.app_written
                                      : &s_app_sideload.resources_written;
}

static const char *prv_app_sideload_part_name(AppSideloadPart part) {
  return (part == AppSideloadPartApp) ? "app" : "res";
}

static void prv_app_sideload_remove_files(AppInstallId install_id) {
  if (install_id <= INSTALL_ID_INVALID) {
    return;
  }

  char filename[64];
  app_storage_get_file_name(filename, sizeof(filename), install_id, PebbleTask_App);
  pfs_remove(filename);
  resource_storage_get_file_name(filename, sizeof(filename), (ResAppNum)install_id);
  pfs_remove(filename);
}

static void prv_app_sideload_reset(bool remove_open_file) {
  if (s_app_sideload.fd >= 0) {
    if (remove_open_file) {
      pfs_close_and_remove(s_app_sideload.fd);
    } else {
      pfs_close(s_app_sideload.fd);
    }
  }

  if (remove_open_file) {
    prv_app_sideload_remove_files(s_app_sideload.install_id);
  }

  s_app_sideload = (AppSideloadState){
      .fd = -1,
  };
}

static bool prv_parse_hex_nibble(char c, uint8_t *nibble_out) {
  if (c >= '0' && c <= '9') {
    *nibble_out = (uint8_t)(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *nibble_out = (uint8_t)(10 + c - 'a');
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *nibble_out = (uint8_t)(10 + c - 'A');
    return true;
  }
  return false;
}

static int prv_decode_hex_bytes(const char *hex, uint8_t *out, uint32_t out_size) {
  const size_t length = strlen(hex);
  if (length == 0 || (length & 1U) != 0U || (length / 2U) > out_size) {
    return -1;
  }

  for (size_t i = 0; i < length; i += 2U) {
    uint8_t high;
    uint8_t low;
    if (!prv_parse_hex_nibble(hex[i], &high) || !prv_parse_hex_nibble(hex[i + 1U], &low)) {
      return -1;
    }
    out[i / 2U] = (uint8_t)((high << 4U) | low);
  }

  return (int)(length / 2U);
}

static bool prv_parse_uuid_token(const char *token, Uuid *uuid_out) {
  const char *cursor = token;
  const bool has_braces = (*cursor == '{');
  if (has_braces) {
    ++cursor;
  }

  uint8_t bytes[UUID_SIZE];
  for (size_t i = 0; i < UUID_SIZE; ++i) {
    if ((i == 4U) || (i == 6U) || (i == 8U) || (i == 10U)) {
      if (*cursor != '-') {
        return false;
      }
      ++cursor;
    }

    uint8_t high;
    uint8_t low;
    if (!prv_parse_hex_nibble(cursor[0], &high) || !prv_parse_hex_nibble(cursor[1], &low)) {
      return false;
    }
    bytes[i] = (uint8_t)((high << 4U) | low);
    cursor += 2U;
  }

  if (has_braces) {
    if (*cursor != '}') {
      return false;
    }
    ++cursor;
  }
  if (*cursor != '\0') {
    return false;
  }

  *uuid_out = UuidMakeFromBEBytes(bytes);
  return true;
}

static bool prv_app_sideload_open_part(AppSideloadPart part) {
  if (s_app_sideload.fd >= 0) {
    if (s_app_sideload.open_part == part) {
      return true;
    }

    uint32_t *written = prv_app_sideload_written_for_part(s_app_sideload.open_part);
    const uint32_t total = prv_app_sideload_total_for_part(s_app_sideload.open_part);
    if (*written != total) {
      prv_usb_write_str("appsideload previous part incomplete\r\n");
      return false;
    }

    pfs_close(s_app_sideload.fd);
    s_app_sideload.fd = -1;
    s_app_sideload.open_part = AppSideloadPartNone;
  }

  uint32_t *written = prv_app_sideload_written_for_part(part);
  const uint32_t total = prv_app_sideload_total_for_part(part);
  if (*written != 0U || total == 0U) {
    prv_usb_write_str("appsideload part already complete\r\n");
    return false;
  }

  char filename[64];
  prv_app_sideload_get_filename(part, filename, sizeof(filename));
  pfs_remove(filename);
  s_app_sideload.fd = pfs_open(filename, OP_FLAG_READ | OP_FLAG_WRITE, FILE_TYPE_STATIC, total);
  if (s_app_sideload.fd < 0) {
    char line[96];
    const int written_line = snprintf(line, sizeof(line), "appsideload open %s failed fd=%d\r\n",
                                      prv_app_sideload_part_name(part), s_app_sideload.fd);
    prv_usb_write_formatted_line(line, written_line, sizeof(line));
    s_app_sideload.fd = -1;
    return false;
  }

  s_app_sideload.open_part = part;
  return true;
}

static void prv_handle_app_sideload_begin(const char *args) {
  char app_size_token[12] = {0};
  char resources_size_token[12] = {0};
  char uuid_token[UUID_STRING_BUFFER_LENGTH] = {0};
  const char *next = prv_read_token(args, app_size_token, sizeof(app_size_token));
  next = prv_read_token(next, resources_size_token, sizeof(resources_size_token));
  (void)prv_read_token(next, uuid_token, sizeof(uuid_token));

  const uint32_t app_size = strtoul(app_size_token, NULL, 0);
  const uint32_t resources_size = strtoul(resources_size_token, NULL, 0);
  if (app_size == 0U || resources_size == 0U) {
    prv_usb_write_str("usage: appsideload begin <app_size> <resources_size> [uuid]\r\n");
    return;
  }

  prv_app_sideload_reset(true);

  AppInstallId install_id = INSTALL_ID_INVALID;
  bool app_db_entry_existed = false;
  if (uuid_token[0] != '\0') {
    Uuid uuid;
    if (!prv_parse_uuid_token(uuid_token, &uuid)) {
      prv_usb_write_str("appsideload bad uuid\r\n");
      return;
    }

    install_id = app_db_get_install_id_for_uuid(&uuid);
    app_db_entry_existed = (install_id > INSTALL_ID_INVALID);
  }
  if (install_id <= INSTALL_ID_INVALID) {
    install_id = (AppInstallId)app_db_check_next_unique_id();
  }
  if (install_id <= INSTALL_ID_INVALID) {
    prv_usb_write_str("appsideload no valid install id\r\n");
    return;
  }

  s_app_sideload = (AppSideloadState){
      .active = true,
      .app_db_entry_existed = app_db_entry_existed,
      .install_id = install_id,
      .fd = -1,
      .app_size = app_size,
      .resources_size = resources_size,
  };

  char line[160];
  const int written =
      (uuid_token[0] != '\0')
          ? snprintf(line, sizeof(line),
                     "appsideload begin id=%" PRId32 " app=%" PRIu32 " res=%" PRIu32 " uuid=%s\r\n",
                     install_id, app_size, resources_size, uuid_token)
          : snprintf(line, sizeof(line),
                     "appsideload begin id=%" PRId32 " app=%" PRIu32 " res=%" PRIu32 "\r\n",
                     install_id, app_size, resources_size);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_handle_app_sideload_chunk(AppSideloadPart part, const char *hex) {
  if (!s_app_sideload.active) {
    prv_usb_write_str("appsideload not active\r\n");
    return;
  }

  uint8_t bytes[APP_SIDELOAD_CHUNK_BYTES];
  const int length = prv_decode_hex_bytes(hex, bytes, sizeof(bytes));
  if (length <= 0) {
    prv_usb_write_str("appsideload bad hex\r\n");
    return;
  }

  if (!prv_app_sideload_open_part(part)) {
    return;
  }

  uint32_t *written = prv_app_sideload_written_for_part(part);
  const uint32_t total = prv_app_sideload_total_for_part(part);
  if (*written + (uint32_t)length > total) {
    prv_usb_write_str("appsideload chunk exceeds part size\r\n");
    prv_app_sideload_reset(true);
    return;
  }

  if (pfs_write(s_app_sideload.fd, bytes, (size_t)length) != length) {
    prv_usb_write_str("appsideload write failed\r\n");
    prv_app_sideload_reset(true);
    return;
  }

  *written += (uint32_t)length;
  if (*written == total) {
    pfs_close(s_app_sideload.fd);
    s_app_sideload.fd = -1;
    s_app_sideload.open_part = AppSideloadPartNone;
  }

  char line[96];
  const int written_line =
      snprintf(line, sizeof(line), "appsideload %s=%" PRIu32 "/%" PRIu32 "\r\n",
               prv_app_sideload_part_name(part), *written, total);
  prv_usb_write_formatted_line(line, written_line, sizeof(line));
}

static void prv_handle_app_sideload_end(void) {
  if (!s_app_sideload.active) {
    prv_usb_write_str("appsideload not active\r\n");
    return;
  }
  if (s_app_sideload.fd >= 0) {
    prv_usb_write_str("appsideload file still open\r\n");
    return;
  }
  if (s_app_sideload.app_written != s_app_sideload.app_size ||
      s_app_sideload.resources_written != s_app_sideload.resources_size) {
    prv_usb_write_str("appsideload incomplete\r\n");
    return;
  }

  PebbleProcessInfo info;
  const AppStorageGetAppInfoResult info_result =
      app_storage_get_process_info(&info, NULL, s_app_sideload.install_id, PebbleTask_App);
  if (info_result != GET_APP_INFO_SUCCESS) {
    char line[80];
    const int written =
        snprintf(line, sizeof(line), "appsideload bad app info=%d\r\n", (int)info_result);
    prv_usb_write_formatted_line(line, written, sizeof(line));
    prv_app_sideload_reset(true);
    return;
  }

  if (!resource_storage_check((ResAppNum)s_app_sideload.install_id, 0, NULL)) {
    prv_usb_write_str("appsideload bad resources\r\n");
    prv_app_sideload_reset(true);
    return;
  }

  AppDBEntry entry = {
      .info_flags = info.flags,
      .icon_resource_id = info.icon_resource_id,
      .app_version = info.process_version,
      .sdk_version = info.sdk_version,
      .app_face_bg_color = GColorBlack,
  };
  memcpy(&entry.uuid, &info.uuid, sizeof(entry.uuid));
  strncpy(entry.name, info.name, APP_NAME_SIZE_BYTES - 1U);

  status_t status = S_SUCCESS;
  if (!s_app_sideload.app_db_entry_existed) {
    status = app_db_insert((const uint8_t *)&entry.uuid, sizeof(entry.uuid),
                           (const uint8_t *)&entry, sizeof(entry));
    if (status != S_SUCCESS) {
      char line[80];
      const int written =
          snprintf(line, sizeof(line), "appsideload appdb failed status=%" PRId32 "\r\n", status);
      prv_usb_write_formatted_line(line, written, sizeof(line));
      prv_app_sideload_reset(true);
      return;
    }
  }

  const AppInstallId actual_id = app_db_get_install_id_for_uuid(&entry.uuid);
  if (actual_id != s_app_sideload.install_id) {
    char line[96];
    const int written = snprintf(
        line, sizeof(line), "appsideload id mismatch expected=%" PRId32 " actual=%" PRId32 "\r\n",
        s_app_sideload.install_id, actual_id);
    prv_usb_write_formatted_line(line, written, sizeof(line));
    prv_app_sideload_reset(false);
    return;
  }

  status = app_cache_add_entry(s_app_sideload.install_id,
                               s_app_sideload.app_size + s_app_sideload.resources_size);
  char line[160];
  const int written = snprintf(
      line, sizeof(line), "appsideload done id=%" PRId32 " cache=%" PRId32 " name=\"%.64s\"\r\n",
      s_app_sideload.install_id, status, entry.name);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  s_app_sideload = (AppSideloadState){
      .fd = -1,
  };
}

static void prv_handle_app_sideload_command(const char *args) {
  if (strncmp(args, "begin ", 6) == 0) {
    prv_handle_app_sideload_begin(args + 6);
  } else if (strncmp(args, "app ", 4) == 0) {
    prv_handle_app_sideload_chunk(AppSideloadPartApp, args + 4);
  } else if (strncmp(args, "res ", 4) == 0) {
    prv_handle_app_sideload_chunk(AppSideloadPartResources, args + 4);
  } else if (strcmp(args, "end") == 0) {
    prv_handle_app_sideload_end();
  } else if (strcmp(args, "cancel") == 0) {
    prv_app_sideload_reset(true);
    prv_usb_write_str("appsideload cancelled\r\n");
  } else {
    prv_usb_write_str(
        "usage: appsideload begin <app_size> <resources_size> [uuid]|app <hex>|res "
        "<hex>|end|cancel\r\n");
  }
}

static void prv_append_hex_bytes(char *out, const uint8_t *data, uint8_t length);
static void prv_append_hci_cmd_history(char *out, size_t out_size,
                                       const FruitJamEspHciDebugSnapshot *snapshot);
static void prv_append_hci_evt_history(char *out, size_t out_size,
                                       const FruitJamEspHciDebugSnapshot *snapshot);
static const char *prv_bt_driver_stage_name(uint8_t stage);

static void prv_send_esp(void) {
  FruitJamEspHciDebugSnapshot snapshot;
  char line[160];
  char evt_hex[FRUITJAM_ESP_HCI_DEBUG_EVT_PREFIX_SIZE * 2U + 1U];
  char cmd_history[FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE * 11U + 1U];
  char evt_history[FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE * 12U + 1U];

  fruitjam_esp_hci_debug_get_snapshot(&snapshot);
  prv_append_hex_bytes(evt_hex, snapshot.last_hci_evt_prefix, snapshot.last_hci_evt_prefix_length);
  prv_append_hci_cmd_history(cmd_history, sizeof(cmd_history), &snapshot);
  prv_append_hci_evt_history(evt_history, sizeof(evt_history), &snapshot);

  int written = snprintf(line, sizeof(line),
                         "esp init=%" PRIu32 " ready=%" PRIu32 " timeout=%" PRIu32
                         " cts=%u drain=%" PRIu32 "\r\n",
                         snapshot.init_count, snapshot.ready_count, snapshot.ready_timeout_count,
                         snapshot.cts_ready ? 1U : 0U, snapshot.drain_bytes);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "esp rx=%" PRIu32 " tx=%" PRIu32 " rxerr=%" PRIu32 " rxirq=%" PRIu32
                     " rxdrops=%" PRIu32 " txtimeout=%" PRIu32 "\r\n",
                     snapshot.rx_bytes, snapshot.tx_bytes, snapshot.rx_error_count,
                     snapshot.rx_irq_count, snapshot.rx_ring_drop_count, snapshot.tx_timeout_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line), "esp last_rx=%02x last_err=%02x rxring=%u high=%u\r\n",
                     snapshot.last_rx_byte, snapshot.last_rx_error, snapshot.rx_ring_depth,
                     snapshot.rx_ring_high_watermark);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "h4 discard=%" PRIu32 " parseerr=%" PRIu32 " evt=%" PRIu32 " acl=%" PRIu32
                     " iso=%" PRIu32 " last_drop=%02x last_type=%02x\r\n",
                     snapshot.h4_discard_count, snapshot.h4_parse_error_count,
                     snapshot.h4_evt_count, snapshot.h4_acl_count, snapshot.h4_iso_count,
                     snapshot.last_h4_discard_byte, snapshot.last_h4_type);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written =
      snprintf(line, sizeof(line), "hci cmd=%" PRIu32 " fail=%" PRIu32 " last_cmd=%04x len=%u\r\n",
               snapshot.hci_cmd_count, snapshot.hci_cmd_fail_count, snapshot.last_hci_cmd_opcode,
               snapshot.last_hci_cmd_length);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "hci evt cc=%" PRIu32 " cs=%" PRIu32 " other=%" PRIu32
                     " code=%02x len=%u opcode=%04x status=%02x hex=%s\r\n",
                     snapshot.hci_evt_cmd_complete_count, snapshot.hci_evt_cmd_status_count,
                     snapshot.hci_evt_other_count, snapshot.last_hci_evt_code,
                     snapshot.last_hci_evt_length, snapshot.last_hci_evt_opcode,
                     snapshot.last_hci_evt_status, evt_hex);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line), "hci cmd_hist seq=%" PRIu32 " %s\r\n",
                     snapshot.hci_cmd_seq, cmd_history);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line), "hci evt_hist seq=%" PRIu32 " %s\r\n",
                     snapshot.hci_evt_seq, evt_history);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_append_hex_bytes(char *out, const uint8_t *data, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    out[i * 2U] = prv_hex_digit(data[i] >> 4U);
    out[i * 2U + 1U] = prv_hex_digit(data[i]);
  }
  out[length * 2U] = '\0';
}

static void prv_append_hci_cmd_history(char *out, size_t out_size,
                                       const FruitJamEspHciDebugSnapshot *snapshot) {
  uint32_t count = snapshot->hci_cmd_seq;
  size_t used = 0U;

  if (out_size == 0U) {
    return;
  }
  out[0] = '\0';

  if (count > FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE) {
    count = FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE;
  }

  const uint32_t start = snapshot->hci_cmd_seq - count;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t seq = start + i;
    const uint8_t index = (uint8_t)(seq & (FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE - 1U));
    const FruitJamEspHciDebugCmdEntry *entry = &snapshot->hci_cmd_history[index];
    const int written =
        snprintf(out + used, out_size - used, "%s%04x:%02x:%u", (i == 0U) ? "" : ",", entry->opcode,
                 entry->length, (unsigned)entry->ok);

    if (written < 0) {
      break;
    }
    if ((size_t)written >= out_size - used) {
      out[out_size - 1U] = '\0';
      break;
    }
    used += (size_t)written;
  }
}

static void prv_append_hci_evt_history(char *out, size_t out_size,
                                       const FruitJamEspHciDebugSnapshot *snapshot) {
  uint32_t count = snapshot->hci_evt_seq;
  size_t used = 0U;

  if (out_size == 0U) {
    return;
  }
  out[0] = '\0';

  if (count > FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE) {
    count = FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE;
  }

  const uint32_t start = snapshot->hci_evt_seq - count;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t seq = start + i;
    const uint8_t index = (uint8_t)(seq & (FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE - 1U));
    const FruitJamEspHciDebugEvtEntry *entry = &snapshot->hci_evt_history[index];
    const int written = snprintf(out + used, out_size - used, "%s%02x:%04x:%02x",
                                 (i == 0U) ? "" : ",", entry->code, entry->opcode, entry->status);

    if (written < 0) {
      break;
    }
    if ((size_t)written >= out_size - used) {
      out[out_size - 1U] = '\0';
      break;
    }
    used += (size_t)written;
  }
}

static void prv_send_bt(void) {
  FruitJamBtDebugSnapshot snapshot;
  char line[256];
  char ad_hex[FRUITJAM_BT_DEBUG_AD_PREFIX_SIZE * 2U + 1U];
  char scan_hex[FRUITJAM_BT_DEBUG_AD_PREFIX_SIZE * 2U + 1U];

  fruitjam_bt_debug_get_snapshot(&snapshot);
  prv_append_hex_bytes(ad_hex, snapshot.ad_prefix, snapshot.ad_prefix_length);
  prv_append_hex_bytes(scan_hex, snapshot.scan_rsp_prefix, snapshot.scan_rsp_prefix_length);

  int written =
      snprintf(line, sizeof(line),
               "bt driver enter=%" PRIu32 " done=%" PRIu32 " ok=%" PRIu32 " fail=%" PRIu32
               " stage=%u:%s state=%u rc=%" PRId32 "\r\n",
               snapshot.driver_start_enter_count, snapshot.driver_start_count,
               snapshot.driver_start_ok_count, snapshot.driver_start_fail_count,
               snapshot.last_driver_stage, prv_bt_driver_stage_name(snapshot.last_driver_stage),
               snapshot.driver_state, snapshot.last_driver_rc);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt recovery pending=%u count=%" PRIu32 " fail=%" PRIu32
                     " reason=%u rc=%" PRId32 "\r\n",
                     snapshot.recovery_pending ? 1U : 0U, snapshot.recovery_schedule_count,
                     snapshot.recovery_schedule_fail_count, snapshot.recovery_last_reason,
                     snapshot.recovery_last_rc);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(
      line, sizeof(line),
      "bt host sync=%" PRIu32 " reset=%" PRIu32 " reset_reason=%" PRId32 " ctl=%" PRIu32
      " init=%u enabled=%u airplane=%u active=%u running=%u"
      " override=%" PRId32 " holdoff=%u\r\n",
      snapshot.host_sync_count, snapshot.host_reset_count, snapshot.last_host_reset_reason,
      snapshot.ctl_state_count, snapshot.ctl_initialized ? 1U : 0U, snapshot.ctl_enabled ? 1U : 0U,
      snapshot.ctl_airplane ? 1U : 0U, snapshot.ctl_active ? 1U : 0U,
      snapshot.ctl_running ? 1U : 0U, snapshot.ctl_override, snapshot.ctl_holdoff ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written =
      snprintf(line, sizeof(line),
               "bt adv active=%u set=%" PRIu32 " setfail=%" PRIu32 " start=%" PRIu32
               " startok=%" PRIu32 " startfail=%" PRIu32 " stop=%" PRIu32 "\r\n",
               snapshot.adv_active ? 1U : 0U, snapshot.adv_data_set_count,
               snapshot.adv_data_set_fail_count, snapshot.adv_start_count,
               snapshot.adv_start_ok_count, snapshot.adv_start_fail_count, snapshot.adv_stop_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt adv addr_rc=%" PRId32 " start_rc=%" PRId32
                     " own_addr_type=%u"
                     " interval=%" PRIu32 "-%" PRIu32 " ad_len=%u scan_len=%u name=\"%s\"\r\n",
                     snapshot.last_addr_rc, snapshot.last_adv_start_rc, snapshot.own_addr_type,
                     snapshot.min_interval_ms, snapshot.max_interval_ms, snapshot.ad_data_length,
                     snapshot.scan_rsp_data_length, snapshot.local_name);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(
      line, sizeof(line),
      "bt gap events=%" PRIu32 " connect=%" PRIu32 " connok=%" PRIu32 " disconnect=%" PRIu32
      " pair=%" PRIu32 " last=%" PRId32 " status=%" PRId32 " reason=%" PRId32 "\r\n",
      snapshot.gap_event_count, snapshot.gap_connect_count, snapshot.gap_connect_ok_count,
      snapshot.gap_disconnect_count, snapshot.gap_pairing_complete_count, snapshot.last_gap_event,
      snapshot.last_gap_status, snapshot.last_gap_reason);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(
      line, sizeof(line),
      "bt gap2 enc=%" PRIu32 " mtu=%" PRIu32 " sub=%" PRIu32 " nrx=%" PRIu32 " ntx=%" PRIu32
      " repeat=%" PRIu32 " hdl=%u m=%u master=%u encd=%u bonded=%u\r\n",
      snapshot.gap_enc_change_count, snapshot.gap_mtu_count, snapshot.gap_subscribe_count,
      snapshot.gap_notify_rx_count, snapshot.gap_notify_tx_count, snapshot.gap_repeat_pairing_count,
      snapshot.last_conn_handle, snapshot.last_conn_mtu, snapshot.last_conn_master ? 1U : 0U,
      snapshot.last_conn_encrypted ? 1U : 0U, snapshot.last_conn_bonded ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt sm io=%u bond=%u mitm=%u sc=%u keypress=%u our=%02x their=%02x\r\n",
                     snapshot.sm_io_cap, snapshot.sm_bonding ? 1U : 0U, snapshot.sm_mitm ? 1U : 0U,
                     snapshot.sm_sc ? 1U : 0U, snapshot.sm_keypress ? 1U : 0U,
                     snapshot.sm_our_key_dist, snapshot.sm_their_key_dist);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt smpair cfg=%" PRIu32 " persist=%" PRIu32
                     " req=%02x rsp=%02x init=%02x resp=%02x rx=%02x proc=%02x key=%u"
                     " pproc=%02x ourk=%02x peerk=%02x\r\n",
                     snapshot.sm_pair_cfg_count, snapshot.sm_persist_count,
                     snapshot.sm_pair_req_auth, snapshot.sm_pair_rsp_auth,
                     snapshot.sm_pair_init_key_dist, snapshot.sm_pair_resp_key_dist,
                     snapshot.sm_pair_rx_key_flags, snapshot.sm_pair_proc_flags,
                     snapshot.sm_pair_key_size, snapshot.sm_persist_proc_flags,
                     snapshot.sm_persist_our_key_flags, snapshot.sm_persist_peer_key_flags);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(
      line, sizeof(line),
      "bt smflow proc=%" PRIu32 " exec=%" PRIu32 " rm=%" PRIu32 " passkey=%" PRIu32
      " st=%u->%u x=%u enc=%u app=%" PRId32
      " err=%02x"
      " act=%u alg=%u rx=%02x flags=%04x pk=%" PRIu32 "/%" PRIu32 " dh=%" PRIu32 "/%" PRIu32
      " sc=%u:%u:%" PRId32 "\r\n",
      snapshot.sm_process_count, snapshot.sm_execute_count, snapshot.sm_remove_count,
      snapshot.sm_passkey_event_count, snapshot.sm_last_proc_state, snapshot.sm_last_after_state,
      snapshot.sm_last_execute, snapshot.sm_last_enc_cb, snapshot.sm_last_app_status,
      snapshot.sm_last_sm_err, snapshot.sm_last_passkey_action, snapshot.sm_last_pair_alg,
      snapshot.sm_last_rx_key_flags, snapshot.sm_last_proc_flags, snapshot.sm_public_key_exec_count,
      snapshot.sm_public_key_rx_count, snapshot.sm_dhkey_exec_count, snapshot.sm_dhkey_rx_count,
      snapshot.sm_last_sc_step, snapshot.sm_last_sc_state, snapshot.sm_last_sc_status);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line), "bt sub attr=%u notify=%u indicate=%u\r\n",
                     snapshot.last_subscribe_attr_handle, snapshot.last_subscribe_notify ? 1U : 0U,
                     snapshot.last_subscribe_indicate ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt pps status_r=%" PRIu32 " trig_r=%" PRIu32 " trig_w=%" PRIu32
                     " notify=%" PRIu32 " notify_fail=%" PRIu32 " rc=%" PRId32
                     " flags=%02x conn=%u bond=%u enc=%u gw=%u\r\n",
                     snapshot.pps_status_read_count, snapshot.pps_trigger_read_count,
                     snapshot.pps_trigger_write_count, snapshot.pps_notify_count,
                     snapshot.pps_notify_fail_count, snapshot.pps_last_rc, snapshot.pps_last_flags,
                     snapshot.pps_last_connected ? 1U : 0U, snapshot.pps_last_bonded ? 1U : 0U,
                     snapshot.pps_last_encrypted ? 1U : 0U, snapshot.pps_last_gateway ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written =
      snprintf(line, sizeof(line),
               "bt disc req=%" PRIu32 " reqfail=%" PRIu32 " reqrc=%" PRId32 " complete=%" PRIu32
               " ok=%" PRIu32 " fail=%" PRIu32 " status=%" PRId32 " services=%" PRIu32 "\r\n",
               snapshot.discovery_request_count, snapshot.discovery_request_fail_count,
               snapshot.discovery_last_request_rc, snapshot.discovery_complete_count,
               snapshot.discovery_success_count, snapshot.discovery_fail_count,
               snapshot.discovery_last_status, snapshot.discovery_last_service_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt kernel conn_evt=%" PRIu32 " connected=%" PRIu32 " disconnected=%" PRIu32
                     " bond=%" PRIu32
                     " bond_id=%u"
                     " bond_op=%" PRId32 " gw_connect=%" PRIu32 " ppogatt=%" PRIu32 "\r\n",
                     snapshot.kernel_connection_event_count, snapshot.kernel_connected_count,
                     snapshot.kernel_disconnected_count, snapshot.kernel_bond_change_count,
                     snapshot.kernel_last_bonding_id, snapshot.kernel_last_bond_op,
                     snapshot.kernel_gateway_connect_count, snapshot.kernel_ppogatt_found_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt store sec=%" PRIu32 " our=%" PRIu32 " peer=%" PRIu32 " reject=%" PRIu32
                     " bond_create=%" PRIu32 " bond_skip=%" PRIu32
                     " obj=%u key_size=%u key_flags=%02x bond_flags=%02x\r\n",
                     snapshot.store_sec_write_count, snapshot.store_our_sec_write_count,
                     snapshot.store_peer_sec_write_count, snapshot.store_sec_reject_count,
                     snapshot.store_bond_create_count, snapshot.store_bond_skip_count,
                     snapshot.store_last_obj_type, snapshot.store_last_key_size,
                     snapshot.store_last_key_flags, snapshot.store_last_bond_flags);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written =
      snprintf(line, sizeof(line),
               "bt ppog clients=%u st=%u ver=%u dst=%u txw=%u rxw=%u in=%u out=%u/%u q=%u"
               " meta=%" PRIu32 "/%" PRIu32 " sub=%" PRIu32 "/%" PRIu32 " reset=%" PRIu32
               " rr=%" PRIu32 " rc=%" PRIu32 " open=%" PRIu32 "\r\n",
               snapshot.ppogatt_client_count, snapshot.ppogatt_state, snapshot.ppogatt_version,
               snapshot.ppogatt_destination, snapshot.ppogatt_tx_window, snapshot.ppogatt_rx_window,
               snapshot.ppogatt_in_sn, snapshot.ppogatt_out_ack_sn, snapshot.ppogatt_out_data_sn,
               snapshot.ppogatt_queue_length, snapshot.ppogatt_meta_ok_count,
               snapshot.ppogatt_meta_fail_count, snapshot.ppogatt_subscribe_ok_count,
               snapshot.ppogatt_subscribe_fail_count, snapshot.ppogatt_reset_start_count,
               snapshot.ppogatt_reset_request_rx_count, snapshot.ppogatt_reset_complete_rx_count,
               snapshot.ppogatt_session_open_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(
      line, sizeof(line),
      "bt ppog2 rx=%" PRIu32 " data=%" PRIu32 " ack=%" PRIu32 " tx=%" PRIu32 " ok=%" PRIu32
      " fail=%" PRIu32 " sendnext=%" PRIu32 " lrx=%u:%u:%u ltx=%u:%u:%u err=%" PRId32 " ev=%u\r\n",
      snapshot.ppogatt_packet_rx_count, snapshot.ppogatt_data_rx_count,
      snapshot.ppogatt_ack_rx_count, snapshot.ppogatt_packet_tx_count,
      snapshot.ppogatt_write_ok_count, snapshot.ppogatt_write_fail_count,
      snapshot.ppogatt_send_next_count, snapshot.ppogatt_last_rx_type, snapshot.ppogatt_last_rx_sn,
      snapshot.ppogatt_last_rx_length, snapshot.ppogatt_last_tx_type, snapshot.ppogatt_last_tx_sn,
      snapshot.ppogatt_last_tx_length, snapshot.ppogatt_last_error, snapshot.ppogatt_last_event);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt pp rx=%" PRIu32 " ep=%u len=%u tx=%" PRIu32
                     " ep=%u len=%u"
                     " errors=%" PRIu32 " no_buf=%" PRIu32 " err_ep=%u err=%u\r\n",
                     snapshot.pp_rx_message_count, snapshot.pp_last_rx_endpoint,
                     snapshot.pp_last_rx_length, snapshot.pp_tx_message_count,
                     snapshot.pp_last_tx_endpoint, snapshot.pp_last_tx_length,
                     snapshot.pp_error_count, snapshot.pp_no_buffer_count,
                     snapshot.pp_last_error_endpoint, snapshot.pp_last_error_code);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line), "bt adv_hex ad=%s scan=%s\r\n", ad_hex, scan_hex);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_retry_bt_discovery(void) {
  BTDeviceInternal device = {0};
  bool have_connection = false;
  bool gateway = false;
  bool encrypted = false;
  bool in_progress = false;

  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_get_gateway();
    if (!connection) {
      connection = gap_le_connection_any();
    }
    if (connection) {
      device = connection->device;
      gateway = connection->is_gateway;
      encrypted = connection->is_encrypted;
      in_progress = connection->gatt_is_service_discovery_in_progress;
      have_connection = true;
    }
  }
  bt_unlock();

  if (!have_connection) {
    prv_usb_write_str("btretry no active BLE connection\r\n");
    return;
  }

  const BTErrno rc = gatt_client_discovery_rediscover_all(&device);
  fruitjam_bt_debug_record_discovery_request(rc);

  char line[128];
  const int written =
      snprintf(line, sizeof(line), "btretry rc=%d gateway=%u encrypted=%u progress=%u\r\n", (int)rc,
               gateway ? 1U : 0U, encrypted ? 1U : 0U, in_progress ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static void prv_disconnect_bt_link(void) {
  BTDeviceInternal device = {0};
  bool have_connection = false;

  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_get_gateway();
    if (!connection) {
      connection = gap_le_connection_any();
    }
    if (connection) {
      device = connection->device;
      have_connection = true;
    }
  }
  bt_unlock();

  if (!have_connection) {
    prv_usb_write_str("btdisc no active BLE connection\r\n");
    return;
  }

  const int rc = bt_driver_gap_le_disconnect(&device);

  char line[64];
  const int written = snprintf(line, sizeof(line), "btdisc rc=%d\r\n", rc);
  prv_usb_write_formatted_line(line, written, sizeof(line));
}

static const char *prv_bt_driver_stage_name(uint8_t stage) {
  switch (stage) {
    case FruitJamBtDebugDriverStageIdle:
      return "idle";
    case FruitJamBtDebugDriverStageHostSync:
      return "host-sync";
    case FruitJamBtDebugDriverStageHostReset:
      return "host-reset";
    case FruitJamBtDebugDriverStageStartEnter:
      return "start-enter";
    case FruitJamBtDebugDriverStageAlreadyStarted:
      return "already";
    case FruitJamBtDebugDriverStageBadState:
      return "bad-state";
    case FruitJamBtDebugDriverStageStarting:
      return "starting";
    case FruitJamBtDebugDriverStageServicesReady:
      return "svc-ready";
    case FruitJamBtDebugDriverStageWaitSync:
      return "wait-sync";
    case FruitJamBtDebugDriverStageSyncTimeout:
      return "sync-timeout";
    case FruitJamBtDebugDriverStageSynced:
      return "synced";
    case FruitJamBtDebugDriverStageEnsureAddr:
      return "ensure-addr";
    case FruitJamBtDebugDriverStageAddrDone:
      return "addr-done";
    case FruitJamBtDebugDriverStageStarted:
      return "started";
    case FruitJamBtDebugDriverStageError:
      return "error";
    default:
      return "unknown";
  }
}

static void prv_clear_fault_state(void) {
  fruitjam_bootsel_clear_fault_state();
}

static void prv_reset_after_flush(void) {
  tud_cdc_write_flush();
  vTaskDelay(pdMS_TO_TICKS(RESET_AFTER_FLUSH_DELAY_MS));
  system_hard_reset();
}

static void prv_enter_esp_passthrough(void) {
  prv_clear_fault_state();
  prv_usb_write_str(
      "entering esp passthrough; reopen this port with esptool --before no_reset --after "
      "no_reset\r\n");
  tud_cdc_write_flush();
  s_tx_tail = s_tx_head;
  s_esp_passthrough = true;
  fruitjam_esp_passthrough_enter_bootloader(ESP_PASSTHROUGH_DEFAULT_BAUD);
}

static bool prv_force_bluetooth_on(void) {
  if (!bt_ctl_is_initialized()) {
    s_bluetooth_force_pending = true;
    return false;
  }

  bt_ctl_set_enabled(true);
  bt_ctl_set_override_mode(BtCtlModeOverrideRun);
  return true;
}

static bool prv_enable_pairing_window(void) {
  if (!bt_ctl_is_initialized()) {
    s_bluetooth_force_pending = true;
    s_bluetooth_pairing_pending = true;
    return false;
  }

  (void)prv_force_bluetooth_on();
  bt_pairability_use_ble_for_period(BLE_PAIRABLE_DEBUG_WINDOW_SECS);
  gap_le_slave_reconnect_stop();
  gap_le_slave_set_discoverable(true);
  return true;
}

static void prv_service_pending_bluetooth_request(void) {
  if (!bt_ctl_is_initialized() ||
      (!s_bluetooth_force_pending && !s_bluetooth_pairing_pending && !s_bluetooth_reset_pending)) {
    return;
  }

  const bool request_reset = s_bluetooth_reset_pending;
  const bool request_pairing = s_bluetooth_pairing_pending;
  s_bluetooth_force_pending = false;
  s_bluetooth_pairing_pending = false;
  s_bluetooth_reset_pending = false;

  if (request_reset) {
    (void)prv_force_bluetooth_on();
    bt_ctl_reset_bluetooth();
  } else if (request_pairing) {
    (void)prv_enable_pairing_window();
  } else {
    (void)prv_force_bluetooth_on();
  }
}

static void prv_handle_command(const char *command) {
  if (strcmp(command, "help") == 0) {
    prv_usb_write_str(
        "commands: help ping progress lcd lcdtest rtc rtcsync rtcset tzset i2cscan tasks gpio "
        "gpiowatch buttons buttonmap "
        "buttonwatch button esp bt reason frame clearfault "
        "reset bton btoff btreset btdisc btretry appfetch storage "
        "btforget btpair bootsel esppass apps appcur appcheck applaunch watch watchdefault "
        "appsideload\r\n");
  } else if (strcmp(command, "ping") == 0) {
    prv_usb_write_str("pong\r\n");
  } else if (strcmp(command, "progress") == 0) {
    prv_send_progress();
  } else if (strcmp(command, "lcd") == 0) {
    prv_send_lcd();
  } else if (strcmp(command, "lcdtest") == 0) {
    prv_handle_lcd_test_command("");
  } else if (strncmp(command, "lcdtest ", 8) == 0) {
    prv_handle_lcd_test_command(command + 8);
  } else if (strcmp(command, "rtc") == 0) {
    prv_send_rtc();
  } else if (strcmp(command, "rtcsync") == 0) {
    prv_request_phone_time();
  } else if (strncmp(command, "rtcset ", 7) == 0) {
    prv_handle_rtc_set_command(command + 7);
  } else if (strcmp(command, "tzset") == 0) {
    prv_handle_timezone_set_command("");
  } else if (strncmp(command, "tzset ", 6) == 0) {
    prv_handle_timezone_set_command(command + 6);
  } else if (strcmp(command, "i2cscan") == 0) {
    prv_send_i2c_scan();
  } else if (strcmp(command, "tasks") == 0) {
    prv_send_tasks();
  } else if (strcmp(command, "gpio") == 0) {
    prv_send_gpio();
  } else if (strcmp(command, "gpiowatch") == 0) {
    prv_handle_gpio_watch_command("");
  } else if (strncmp(command, "gpiowatch ", 10) == 0) {
    prv_handle_gpio_watch_command(command + 10);
  } else if (strcmp(command, "buttons") == 0) {
    prv_send_buttons();
  } else if (strcmp(command, "buttonmap") == 0) {
    prv_send_buttonmap();
  } else if (strcmp(command, "buttonwatch") == 0) {
    prv_handle_button_watch_command("");
  } else if (strncmp(command, "buttonwatch ", 12) == 0) {
    prv_handle_button_watch_command(command + 12);
  } else if (strncmp(command, "button ", 7) == 0) {
    prv_handle_button_command(command + 7);
  } else if (strcmp(command, "esp") == 0) {
    prv_send_esp();
  } else if (strcmp(command, "bt") == 0) {
    prv_send_bt();
  } else if (strcmp(command, "bton") == 0) {
    if (prv_force_bluetooth_on()) {
      prv_usb_write_str("bt forced on\r\n");
    } else {
      prv_usb_write_str("bt force-on pending until bt init\r\n");
    }
  } else if (strcmp(command, "btoff") == 0) {
    s_bluetooth_force_pending = false;
    s_bluetooth_pairing_pending = false;
    s_bluetooth_reset_pending = false;
    bt_ctl_set_override_mode(BtCtlModeOverrideNone);
    bt_ctl_set_enabled(false);
    prv_usb_write_str("bt disabled\r\n");
  } else if (strcmp(command, "btreset") == 0) {
    if (prv_force_bluetooth_on()) {
      bt_ctl_reset_bluetooth();
      prv_usb_write_str("bt reset requested\r\n");
    } else {
      s_bluetooth_reset_pending = true;
      prv_usb_write_str("bt reset pending until bt init\r\n");
    }
  } else if (strcmp(command, "btdisc") == 0) {
    prv_disconnect_bt_link();
  } else if (strcmp(command, "btretry") == 0) {
    prv_retry_bt_discovery();
  } else if (strcmp(command, "apps") == 0) {
    prv_send_apps();
  } else if (strcmp(command, "appcur") == 0) {
    prv_send_current_app();
  } else if (strcmp(command, "appfetch") == 0) {
    prv_send_app_fetch();
  } else if (strcmp(command, "storage") == 0) {
    prv_send_storage();
  } else if (strncmp(command, "appcheck ", 9) == 0) {
    prv_handle_app_check_command(command + 9);
  } else if (strncmp(command, "applaunch ", 10) == 0) {
    prv_handle_app_launch_command(command + 10);
  } else if (strcmp(command, "watch") == 0) {
    prv_handle_watch_command();
  } else if (strncmp(command, "watchdefault ", 13) == 0) {
    prv_handle_watch_default_command(command + 13);
  } else if (strncmp(command, "appsideload ", 12) == 0) {
    prv_handle_app_sideload_command(command + 12);
  } else if (strcmp(command, "reason") == 0) {
    prv_send_reason();
  } else if (strcmp(command, "frame") == 0) {
    prv_send_frame();
  } else if (strcmp(command, "clearfault") == 0) {
    prv_clear_fault_state();
    prv_usb_write_str("fault state cleared\r\n");
  } else if (strcmp(command, "reset") == 0) {
    prv_usb_write_str("resetting\r\n");
    prv_reset_after_flush();
  } else if (strcmp(command, "btforget") == 0) {
    bt_persistent_storage_delete_all_pairings();
    prv_usb_write_str("bt pairings deleted; resetting after flush\r\n");
    prv_reset_after_flush();
  } else if (strcmp(command, "btpair") == 0) {
    if (prv_enable_pairing_window()) {
      prv_usb_write_str("bt pairable requested for 300s\r\n");
    } else {
      prv_usb_write_str("bt pairable pending until bt init\r\n");
    }
  } else if (strcmp(command, "bootsel") == 0) {
    prv_usb_write_str("entering bootsel\r\n");
    fruitjam_bootsel_enter();
  } else if (strcmp(command, "esppass") == 0) {
    prv_enter_esp_passthrough();
  } else if (command[0] != '\0') {
    prv_usb_write_str("unknown command\r\n");
  }
}

static void prv_esp_passthrough_service(void) {
  uint8_t data[64];

  while (tud_cdc_available()) {
    const uint32_t count = tud_cdc_read(data, sizeof(data));
    (void)fruitjam_esp_passthrough_write(data, count);
  }

  uint32_t burst_count = 0;
  uint8_t byte;
  while (fruitjam_esp_passthrough_read_byte(&byte)) {
    while (tud_cdc_connected() && tud_cdc_write_available() == 0U) {
      tud_task();
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!tud_cdc_connected()) {
      break;
    }
    tud_cdc_write_char((char)byte);
    if (++burst_count >= ESP_PASSTHROUGH_BURST_LIMIT) {
      break;
    }
  }
  tud_cdc_write_flush();
}

static void prv_process_rx(void) {
  uint8_t data[32];

  while (tud_cdc_available()) {
    const uint32_t count = tud_cdc_read(data, sizeof(data));
    for (uint32_t i = 0; i < count; ++i) {
      const char c = (char)data[i];
      if (c == '\r' || c == '\n') {
        s_rx_line[s_rx_line_length] = '\0';
        prv_handle_command(s_rx_line);
        s_rx_line_length = 0;
      } else if (s_rx_line_length < RX_LINE_SIZE - 1U) {
        s_rx_line[s_rx_line_length++] = c;
      }
    }
  }
}

static void prv_drain_log_ring(void) {
  while (s_tx_tail != s_tx_head && tud_cdc_write_available() != 0U) {
    tud_cdc_write_char((char)s_tx_ring[s_tx_tail]);
    s_tx_tail = (s_tx_tail + 1U) % TX_RING_SIZE;
  }
  tud_cdc_write_flush();
}

static void prv_usb_task(void *data) {
  (void)data;

#if defined(CONFIG_RP2350_BOOTSEL_FROM_USB_TASK)
  fruitjam_bootsel_enter();
#endif

  const bool usb_clock_ok = prv_usb_clock_init();
#if defined(CONFIG_RP2350_BOOTSEL_AFTER_USB_CLOCK)
  if (usb_clock_ok) {
    fruitjam_bootsel_enter();
  }
#endif

  if (!usb_clock_ok) {
    s_usb_task = NULL;
    vTaskDelete(NULL);
    return;
  }

  tusb_init();
  prv_usb_configure_runtime();
#if defined(CONFIG_RP2350_BOOTSEL_AFTER_TUSB_INIT)
  fruitjam_bootsel_enter();
#endif

  s_initialized = true;
#if defined(CONFIG_RP2350_BOOTSEL_AFTER_TUD_TASK)
  tud_task();
  fruitjam_bootsel_enter();
#endif

#if defined(CONFIG_RP2350_BOOTSEL_AFTER_USB_LOOP)
  for (uint32_t i = 0; i < 5000U; ++i) {
    tud_task();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  fruitjam_bootsel_enter();
#endif

#if defined(CONFIG_RP2350_BOOTSEL_AFTER_USB_REALTIME)
  const uint32_t start_us = time_us_32();
  const uint32_t timeout_us = USB_REALTIME_BOOTSEL_MS * 1000U;
  while ((uint32_t)(time_us_32() - start_us) < timeout_us) {
    prv_usb_service_pending_irq_polled();
    tud_task();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  fruitjam_bootsel_enter_with_params(BOOTSEL_REASON_USB_TASK_TIMEOUT, prv_usb_packed_status());
#endif

  while (true) {
    tud_task();
    prv_service_pending_bluetooth_request();
    if (tud_cdc_connected()) {
      if (s_esp_passthrough) {
        prv_esp_passthrough_service();
      } else {
        prv_process_rx();
        prv_drain_log_ring();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void fruitjam_usb_debug_init(void) {
  if (s_usb_task) {
    return;
  }

  xTaskCreate(prv_usb_task, "USBDebug", USB_TASK_STACK_SIZE, NULL, USB_TASK_PRIORITY, &s_usb_task);
}

void fruitjam_usb_debug_polled_main(void) {
  if (!prv_usb_clock_init()) {
    fruitjam_bootsel_enter_with_params(BOOTSEL_REASON_POLLED_CLOCK_FAIL, 0U);
  }

  if (!tusb_init()) {
    fruitjam_bootsel_enter_with_params(BOOTSEL_REASON_POLLED_TUSB_FAIL, 0U);
  }
  prv_usb_configure_runtime();
  s_initialized = true;

  uint32_t last_message_ms = 0;
  const uint32_t start_us = time_us_32();
  const uint32_t timeout_us = USB_REALTIME_BOOTSEL_MS * 1000U;
  while ((uint32_t)(time_us_32() - start_us) < timeout_us) {
    prv_usb_service_pending_irq_polled();
    tud_task();

    const uint32_t now_ms = tusb_time_millis_api();
    if ((now_ms - last_message_ms) >= USB_POLLED_MESSAGE_INTERVAL_MS) {
      last_message_ms = now_ms;
      prv_usb_polled_write_str("pico polled usb alive\r\n");
    }
  }

  fruitjam_bootsel_enter_with_params(BOOTSEL_REASON_POLLED_TIMEOUT, prv_usb_packed_status());
}
