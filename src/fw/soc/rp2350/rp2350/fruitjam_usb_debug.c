/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_usb_debug.h"

#include "hardware/resets.h"
#include "soc/rp2350/rp2350/fruitjam_bootsel.h"
#include "soc/rp2350/rp2350/fruitjam_boot_progress.h"
#include "soc/rp2350/rp2350/fruitjam_bt_debug.h"
#include "soc/rp2350/rp2350/fruitjam_esp.h"
#include "soc/rp2350/rp2350/fruitjam_lcd.h"
#include "system/bootbits.h"
#include "system/reboot_reason.h"
#include "system/reset.h"

#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define USB_TASK_STACK_SIZE 512U
#define USB_TASK_PRIORITY (tskIDLE_PRIORITY + 4U)
#define TX_RING_SIZE 2048U
#define RX_LINE_SIZE 48U
#define TASK_SNAPSHOT_COUNT 24U
#define ESP_PASSTHROUGH_DEFAULT_BAUD 115200U
#define ESP_PASSTHROUGH_BURST_LIMIT 128U

#define REG32(addr) (*(volatile uint32_t *)(addr))

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

#define BOOTSEL_CDC_MAGIC_BAUD_RATE 1200U

typedef void (*IrqHandler)(void);

static TaskHandle_t s_usb_task;
static IrqHandler s_usb_irq_handler;
static volatile uint32_t s_tx_head;
static volatile uint32_t s_tx_tail;
static uint8_t s_tx_ring[TX_RING_SIZE];
static char s_rx_line[RX_LINE_SIZE];
static uint8_t s_rx_line_length;
static bool s_initialized;
static bool s_esp_passthrough;

void fruitjam_usb_irq_set_handler(IrqHandler handler) {
  s_usb_irq_handler = handler;
}

void USBCTRL_IRQ_IRQHandler(void) {
  if (s_usb_irq_handler) {
    s_usb_irq_handler();
  }
}

uint32_t tusb_time_millis_api(void) {
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
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

  while (length != 0U && tud_cdc_connected()) {
    uint32_t available = tud_cdc_write_available();
    if (available == 0U) {
      tud_task();
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

static bool prv_wait_for_bits(uintptr_t address, uint32_t mask, uint32_t value) {
  for (uint32_t i = 0; i < USB_CLOCK_WAIT_LIMIT; ++i) {
    if ((REG32(address) & mask) == value) {
      return true;
    }
  }

  return false;
}

static bool prv_xosc_init(void) {
  if (REG32(XOSC_BASE + XOSC_STATUS_OFFSET) & XOSC_STATUS_STABLE_BITS) {
    return true;
  }

  REG32(XOSC_BASE + XOSC_CTRL_OFFSET) = XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;
  REG32(XOSC_BASE + XOSC_STARTUP_OFFSET) = XOSC_STARTUP_DELAY_12MHZ;
  REG32(XOSC_BASE + XOSC_CTRL_OFFSET) =
      (XOSC_CTRL_ENABLE_VALUE_ENABLE << XOSC_CTRL_ENABLE_LSB) | XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;

  return prv_wait_for_bits(XOSC_BASE + XOSC_STATUS_OFFSET, XOSC_STATUS_STABLE_BITS,
                           XOSC_STATUS_STABLE_BITS);
}

static bool prv_usb_clock_init(void) {
  if (!prv_xosc_init()) {
    return false;
  }

  reset_block(RESETS_RESET_PLL_USB_BITS);
  unreset_block_wait(RESETS_RESET_PLL_USB_BITS);

  REG32(PLL_USB_BASE + PLL_CS_OFFSET) = PLL_USB_REFDIV;
  REG32(PLL_USB_BASE + PLL_FBDIV_INT_OFFSET) = PLL_USB_FBDIV;
  REG32(PLL_USB_BASE + PLL_PWR_OFFSET) &= ~(PLL_PWR_PD_BITS | PLL_PWR_VCOPD_BITS);
  if (!prv_wait_for_bits(PLL_USB_BASE + PLL_CS_OFFSET, PLL_CS_LOCK_BITS, PLL_CS_LOCK_BITS)) {
    return false;
  }

  REG32(PLL_USB_BASE + PLL_PRIM_OFFSET) =
      (PLL_USB_POSTDIV1 << PLL_PRIM_POSTDIV1_LSB) | (PLL_USB_POSTDIV2 << PLL_PRIM_POSTDIV2_LSB);
  REG32(PLL_USB_BASE + PLL_PWR_OFFSET) &= ~PLL_PWR_POSTDIVPD_BITS;

  REG32(CLOCKS_BASE + CLOCKS_CLK_USB_CTRL_OFFSET) = 0U;
  REG32(CLOCKS_BASE + CLOCKS_CLK_USB_DIV_OFFSET) = CLOCKS_CLK_USB_DIV_1;
  REG32(CLOCKS_BASE + CLOCKS_CLK_USB_CTRL_OFFSET) = CLOCKS_CLK_USB_CTRL_ENABLE_BITS;
  return prv_wait_for_bits(CLOCKS_BASE + CLOCKS_CLK_USB_CTRL_OFFSET,
                           CLOCKS_CLK_USB_CTRL_ENABLED_BITS, CLOCKS_CLK_USB_CTRL_ENABLED_BITS);
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

  if (line_coding->bit_rate == BOOTSEL_CDC_MAGIC_BAUD_RATE) {
    fruitjam_bootsel_enter();
  } else if (s_esp_passthrough) {
    fruitjam_esp_passthrough_set_baud(line_coding->bit_rate);
  }
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
  prv_append_hex_bytes(evt_hex, snapshot.last_hci_evt_prefix,
                       snapshot.last_hci_evt_prefix_length);
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
                     snapshot.rx_irq_count, snapshot.rx_ring_drop_count,
                     snapshot.tx_timeout_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "esp last_rx=%02x last_err=%02x rxring=%u high=%u\r\n",
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

  written = snprintf(line, sizeof(line),
                     "hci cmd=%" PRIu32 " fail=%" PRIu32 " last_cmd=%04x len=%u\r\n",
                     snapshot.hci_cmd_count, snapshot.hci_cmd_fail_count,
                     snapshot.last_hci_cmd_opcode, snapshot.last_hci_cmd_length);
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
    const int written = snprintf(out + used, out_size - used, "%s%04x:%02x:%u",
                                 (i == 0U) ? "" : ",", entry->opcode, entry->length,
                                 (unsigned)entry->ok);

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
                                 (i == 0U) ? "" : ",", entry->code, entry->opcode,
                                 entry->status);

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
  char line[192];
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

  written =
      snprintf(line, sizeof(line),
               "bt host sync=%" PRIu32 " reset=%" PRIu32 " reset_reason=%" PRId32
               " ctl=%" PRIu32 " init=%u enabled=%u airplane=%u active=%u running=%u"
               " override=%" PRId32 " holdoff=%u\r\n",
               snapshot.host_sync_count, snapshot.host_reset_count,
               snapshot.last_host_reset_reason, snapshot.ctl_state_count,
               snapshot.ctl_initialized ? 1U : 0U, snapshot.ctl_enabled ? 1U : 0U,
               snapshot.ctl_airplane ? 1U : 0U, snapshot.ctl_active ? 1U : 0U,
               snapshot.ctl_running ? 1U : 0U, snapshot.ctl_override,
               snapshot.ctl_holdoff ? 1U : 0U);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written =
      snprintf(line, sizeof(line),
               "bt adv active=%u set=%" PRIu32 " setfail=%" PRIu32 " start=%" PRIu32
               " startok=%" PRIu32 " startfail=%" PRIu32 " stop=%" PRIu32 "\r\n",
               snapshot.adv_active ? 1U : 0U, snapshot.adv_data_set_count,
               snapshot.adv_data_set_fail_count, snapshot.adv_start_count,
               snapshot.adv_start_ok_count, snapshot.adv_start_fail_count,
               snapshot.adv_stop_count);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written =
      snprintf(line, sizeof(line),
               "bt adv addr_rc=%" PRId32 " start_rc=%" PRId32 " own_addr_type=%u"
               " interval=%" PRIu32 "-%" PRIu32 " ad_len=%u scan_len=%u name=\"%s\"\r\n",
               snapshot.last_addr_rc, snapshot.last_adv_start_rc, snapshot.own_addr_type,
               snapshot.min_interval_ms, snapshot.max_interval_ms, snapshot.ad_data_length,
               snapshot.scan_rsp_data_length, snapshot.local_name);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line),
                     "bt gap events=%" PRIu32 " connect=%" PRIu32 " connok=%" PRIu32
                     " disconnect=%" PRIu32 " pair=%" PRIu32 " last=%" PRId32
                     " status=%" PRId32 " reason=%" PRId32 "\r\n",
                     snapshot.gap_event_count, snapshot.gap_connect_count,
                     snapshot.gap_connect_ok_count, snapshot.gap_disconnect_count,
                     snapshot.gap_pairing_complete_count, snapshot.last_gap_event,
                     snapshot.last_gap_status, snapshot.last_gap_reason);
  prv_usb_write_formatted_line(line, written, sizeof(line));

  written = snprintf(line, sizeof(line), "bt adv_hex ad=%s scan=%s\r\n", ad_hex, scan_hex);
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

static void prv_handle_command(const char *command) {
  if (strcmp(command, "help") == 0) {
    prv_usb_write_str(
        "commands: help ping progress tasks esp bt reason frame clearfault reset bootsel esppass\r\n");
  } else if (strcmp(command, "ping") == 0) {
    prv_usb_write_str("pong\r\n");
  } else if (strcmp(command, "progress") == 0) {
    prv_send_progress();
  } else if (strcmp(command, "tasks") == 0) {
    prv_send_tasks();
  } else if (strcmp(command, "esp") == 0) {
    prv_send_esp();
  } else if (strcmp(command, "bt") == 0) {
    prv_send_bt();
  } else if (strcmp(command, "reason") == 0) {
    prv_send_reason();
  } else if (strcmp(command, "frame") == 0) {
    prv_send_frame();
  } else if (strcmp(command, "clearfault") == 0) {
    prv_clear_fault_state();
    prv_usb_write_str("fault state cleared\r\n");
  } else if (strcmp(command, "reset") == 0) {
    prv_usb_write_str("resetting\r\n");
    tud_cdc_write_flush();
    system_hard_reset();
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

  if (!prv_usb_clock_init()) {
    s_usb_task = NULL;
    vTaskDelete(NULL);
    return;
  }

  tusb_init();
  s_initialized = true;

  while (true) {
    tud_task();
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
