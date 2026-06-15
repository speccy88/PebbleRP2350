/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_esp.h"

#include "board/board.h"
#include "board/boards/board_fruitjam_rp2350.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define RESETS_BASE 0x40020000U
#define CLOCKS_BASE 0x40010000U
#define XOSC_BASE 0x40048000U
#define UART1_BASE 0x40078000U
#define SIO_BASE 0xd0000000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define REG_ALIAS_SET_BITS 0x2000U
#define REG_ALIAS_CLR_BITS 0x3000U

#define GPIO_FUNC_UART 2U
#define GPIO_FUNC_SIO 5U
#define GPIO_PAD_NORMAL 0x52U
#define GPIO_PAD_INPUT 0x42U
#define GPIO_PAD_INPUT_PULLUP 0x4aU

#define RESETS_RESET_OFFSET 0x00U
#define RESETS_RESET_DONE_OFFSET 0x08U
#define RESETS_RESET_UART1_BITS (1U << 27)

#define CLOCKS_CLK_PERI_CTRL_OFFSET 0x48U
#define CLOCKS_CLK_PERI_DIV_OFFSET 0x4cU
#define CLOCKS_CLK_PERI_CTRL_ENABLE_BITS (1U << 11)
#define CLOCKS_CLK_PERI_CTRL_ENABLED_BITS (1U << 28)
#define CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB 5U
#define CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC 0x4U
#define CLOCKS_CLK_PERI_DIV_1 0x00010000U

#define XOSC_CTRL_OFFSET 0x00U
#define XOSC_STATUS_OFFSET 0x04U
#define XOSC_STARTUP_OFFSET 0x0cU
#define XOSC_CTRL_ENABLE_LSB 12U
#define XOSC_CTRL_ENABLE_VALUE_ENABLE 0xfabU
#define XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ 0xaa0U
#define XOSC_STATUS_STABLE_BITS (1U << 31)
#define XOSC_STARTUP_DELAY_12MHZ 47U

#define SIO_GPIO_IN_OFFSET 0x04U
#define SIO_GPIO_HI_IN_OFFSET 0x08U
#define SIO_GPIO_OUT_SET_OFFSET 0x18U
#define SIO_GPIO_HI_OUT_SET_OFFSET 0x1cU
#define SIO_GPIO_OUT_CLR_OFFSET 0x20U
#define SIO_GPIO_HI_OUT_CLR_OFFSET 0x24U
#define SIO_GPIO_OE_SET_OFFSET 0x38U
#define SIO_GPIO_HI_OE_SET_OFFSET 0x3cU
#define SIO_GPIO_OE_CLR_OFFSET 0x40U
#define SIO_GPIO_HI_OE_CLR_OFFSET 0x44U

#define UART_UARTDR_OFFSET 0x00U
#define UART_UARTRSR_OFFSET 0x04U
#define UART_UARTFR_OFFSET 0x18U
#define UART_UARTIBRD_OFFSET 0x24U
#define UART_UARTFBRD_OFFSET 0x28U
#define UART_UARTLCR_H_OFFSET 0x2cU
#define UART_UARTCR_OFFSET 0x30U
#define UART_UARTIMSC_OFFSET 0x38U
#define UART_UARTICR_OFFSET 0x44U
#define UART_UARTDMACR_OFFSET 0x48U

#define UART_UARTDR_ERROR_BITS 0x00000f00U
#define UART_UARTRSR_ERROR_BITS 0x0000000fU
#define UART_UARTFR_TXFF_BITS 0x00000020U
#define UART_UARTFR_RXFE_BITS 0x00000010U
#define UART_UARTFR_BUSY_BITS 0x00000008U
#define UART_UARTLCR_H_FEN_BITS 0x00000010U
#define UART_UARTLCR_H_WLEN_8_BITS (3U << 5)
#define UART_UARTCR_RXE_BITS 0x00000200U
#define UART_UARTCR_TXE_BITS 0x00000100U
#define UART_UARTCR_UARTEN_BITS 0x00000001U

#define UART_ERROR_OVERRUN 0x08U
#define HCI_EVENT_COMMAND_COMPLETE 0x0eU
#define HCI_EVENT_COMMAND_STATUS 0x0fU
#define ESP_HCI_BAUD 115200U
#define ESP_UART_CLOCK_HZ 12000000U
#define ESP_RESET_ASSERT_MS 100U
#define ESP_BOOT_READY_TIMEOUT_MS 3000U
#define ESP_BOOT_SETTLE_MS 50U
#define ESP_BOOTLOADER_SETTLE_MS 100U
#define ESP_HCI_DRAIN_SETTLE_MS 100U
#define ESP_CTS_TIMEOUT 20000U

extern uint32_t SystemCoreClock;

static volatile FruitJamEspHciDebugSnapshot s_debug;

static void prv_delay_cycles(uint32_t cycles) {
  for (volatile uint32_t i = 0; i < cycles; ++i) {
    __asm__ volatile("nop");
  }
}

static void prv_delay_ms(uint32_t ms) {
  const uint32_t cycles_per_ms = SystemCoreClock / 6000U;

  for (uint32_t i = 0; i < ms; ++i) {
    prv_delay_cycles(cycles_per_ms);
  }
}

static void prv_gpio_put(uint8_t pin, bool value) {
  if (pin < 32U) {
    REG32(SIO_BASE + (value ? SIO_GPIO_OUT_SET_OFFSET : SIO_GPIO_OUT_CLR_OFFSET)) = 1U << pin;
  } else {
    REG32(SIO_BASE + (value ? SIO_GPIO_HI_OUT_SET_OFFSET : SIO_GPIO_HI_OUT_CLR_OFFSET)) =
        1U << (pin - 32U);
  }
}

static bool prv_gpio_get(uint8_t pin) {
  if (pin < 32U) {
    return (REG32(SIO_BASE + SIO_GPIO_IN_OFFSET) & (1U << pin)) != 0U;
  }

  return (REG32(SIO_BASE + SIO_GPIO_HI_IN_OFFSET) & (1U << (pin - 32U))) != 0U;
}

static void prv_gpio_output_init(uint8_t pin, bool value) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = GPIO_FUNC_SIO;
  prv_gpio_put(pin, value);
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_SET_OFFSET) = 1U << pin;
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_SET_OFFSET) = 1U << (pin - 32U);
  }
}

static void prv_gpio_input_init(uint8_t pin, bool pull_up) {
  REG32(PADS_BANK0_GPIO(pin)) = pull_up ? GPIO_PAD_INPUT_PULLUP : GPIO_PAD_INPUT;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = GPIO_FUNC_SIO;
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_CLR_OFFSET) = 1U << pin;
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_CLR_OFFSET) = 1U << (pin - 32U);
  }
}

static void prv_gpio_set_function(uint8_t pin, uint32_t function) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = function;
}

static bool prv_wait_for_bits(uint32_t address, uint32_t mask, uint32_t value) {
  const uint32_t k_timeout = 1000000U;
  for (uint32_t i = 0; i < k_timeout; ++i) {
    if ((REG32(address) & mask) == value) {
      return true;
    }
  }
  return false;
}

static void prv_enable_xosc(void) {
  if (REG32(XOSC_BASE + XOSC_STATUS_OFFSET) & XOSC_STATUS_STABLE_BITS) {
    return;
  }

  REG32(XOSC_BASE + XOSC_CTRL_OFFSET) = XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;
  REG32(XOSC_BASE + XOSC_STARTUP_OFFSET) = XOSC_STARTUP_DELAY_12MHZ;
  REG32(XOSC_BASE + XOSC_CTRL_OFFSET) =
      (XOSC_CTRL_ENABLE_VALUE_ENABLE << XOSC_CTRL_ENABLE_LSB) |
      XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;
  (void)prv_wait_for_bits(XOSC_BASE + XOSC_STATUS_OFFSET, XOSC_STATUS_STABLE_BITS,
                          XOSC_STATUS_STABLE_BITS);
}

static void prv_enable_clk_peri_from_xosc(void) {
  prv_enable_xosc();

  REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_CTRL_OFFSET) = 0U;
  REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_DIV_OFFSET) = CLOCKS_CLK_PERI_DIV_1;
  REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_CTRL_OFFSET) =
      CLOCKS_CLK_PERI_CTRL_ENABLE_BITS |
      (CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC << CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB);

  while (!(REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_CTRL_OFFSET) & CLOCKS_CLK_PERI_CTRL_ENABLED_BITS)) {
  }
}

static void prv_uart1_reset(void) {
  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_SET_BITS) = RESETS_RESET_UART1_BITS;
  prv_delay_cycles(64U);
  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_CLR_BITS) = RESETS_RESET_UART1_BITS;
  while (!(REG32(RESETS_BASE + RESETS_RESET_DONE_OFFSET) & RESETS_RESET_UART1_BITS)) {
  }
}

static void prv_uart1_set_baud(uint32_t baud) {
  const uint32_t baud_rate_div = (uint32_t)(((uint64_t)8U * ESP_UART_CLOCK_HZ / baud) + 1U);
  uint32_t baud_ibrd = baud_rate_div >> 7;
  uint32_t baud_fbrd;

  if (baud_ibrd == 0U) {
    baud_ibrd = 1U;
    baud_fbrd = 0U;
  } else if (baud_ibrd >= 65535U) {
    baud_ibrd = 65535U;
    baud_fbrd = 0U;
  } else {
    baud_fbrd = (baud_rate_div & 0x7fU) >> 1;
  }

  REG32(UART1_BASE + UART_UARTIBRD_OFFSET) = baud_ibrd;
  REG32(UART1_BASE + UART_UARTFBRD_OFFSET) = baud_fbrd;
}

static void prv_uart1_init(void) {
  prv_enable_clk_peri_from_xosc();
  prv_uart1_reset();

  REG32(UART1_BASE + UART_UARTCR_OFFSET) = 0U;
  REG32(UART1_BASE + UART_UARTICR_OFFSET) = 0x7ffU;
  REG32(UART1_BASE + UART_UARTIMSC_OFFSET) = 0U;
  REG32(UART1_BASE + UART_UARTDMACR_OFFSET) = 0U;
  prv_uart1_set_baud(ESP_HCI_BAUD);
  REG32(UART1_BASE + UART_UARTLCR_H_OFFSET) = UART_UARTLCR_H_WLEN_8_BITS | UART_UARTLCR_H_FEN_BITS;
  REG32(UART1_BASE + UART_UARTCR_OFFSET) =
      UART_UARTCR_UARTEN_BITS | UART_UARTCR_TXE_BITS | UART_UARTCR_RXE_BITS;

  prv_gpio_set_function(FRUITJAM_PIN_ESP_TX, GPIO_FUNC_UART);
  prv_gpio_set_function(FRUITJAM_PIN_ESP_RX, GPIO_FUNC_UART);
}

static void prv_uart1_deinit(void) {
  while (REG32(UART1_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_BUSY_BITS) {
  }
  REG32(UART1_BASE + UART_UARTCR_OFFSET) = 0U;
}

static bool prv_esp_cts_ready(void) {
  return !prv_gpio_get(FRUITJAM_PIN_ESP_BUSY);
}

static bool prv_esp_wait_cts(void) {
  for (uint32_t timeout = 0; timeout < ESP_CTS_TIMEOUT; ++timeout) {
    if (prv_esp_cts_ready()) {
      return true;
    }
    prv_delay_cycles(32U);
  }

  return false;
}

static bool prv_esp_wait_ready(void) {
  for (uint32_t timeout_ms = 0; timeout_ms < ESP_BOOT_READY_TIMEOUT_MS; ++timeout_ms) {
    if (prv_esp_cts_ready()) {
      return true;
    }
    prv_delay_ms(1U);
  }

  return false;
}

static uint32_t prv_uart1_drain_rx(void) {
  uint32_t count = 0;

  while (!(REG32(UART1_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_RXFE_BITS)) {
    const uint32_t data = REG32(UART1_BASE + UART_UARTDR_OFFSET);
    if (data & UART_UARTDR_ERROR_BITS) {
      ++s_debug.rx_error_count;
      s_debug.last_rx_error = (uint8_t)(data >> 8U);
      REG32(UART1_BASE + UART_UARTRSR_OFFSET) = UART_UARTRSR_ERROR_BITS;
    } else {
      s_debug.last_rx_byte = (uint8_t)data;
    }
    ++count;
  }

  return count;
}

void fruitjam_esp_hci_init(void) {
  ++s_debug.init_count;
  prv_gpio_output_init(FRUITJAM_PIN_ESP_CS, true);
  prv_gpio_input_init(FRUITJAM_PIN_ESP_IRQ, true);
  prv_gpio_input_init(FRUITJAM_PIN_ESP_BUSY, true);
  prv_uart1_init();

  prv_gpio_output_init(FRUITJAM_PIN_ESP_RESET, false);
  prv_delay_ms(ESP_RESET_ASSERT_MS);
  prv_gpio_put(FRUITJAM_PIN_ESP_RESET, true);
  prv_delay_ms(ESP_BOOT_SETTLE_MS);

  if (prv_esp_wait_ready()) {
    ++s_debug.ready_count;
    prv_delay_ms(ESP_HCI_DRAIN_SETTLE_MS);
    s_debug.drain_bytes += prv_uart1_drain_rx();
  } else {
    ++s_debug.ready_timeout_count;
  }
}

void fruitjam_esp_hci_deinit(void) {
  prv_uart1_deinit();
  prv_gpio_output_init(FRUITJAM_PIN_ESP_IRQ, true);
  prv_gpio_output_init(FRUITJAM_PIN_ESP_CS, true);
  prv_gpio_output_init(FRUITJAM_PIN_ESP_RESET, false);
}

bool fruitjam_esp_hci_read_byte(uint8_t *byte) {
  if (REG32(UART1_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_RXFE_BITS) {
    return false;
  }

  const uint32_t data = REG32(UART1_BASE + UART_UARTDR_OFFSET);
  if (data & UART_UARTDR_ERROR_BITS) {
    const uint8_t error = (uint8_t)(data >> 8U);
    ++s_debug.rx_error_count;
    s_debug.last_rx_error = error;
    REG32(UART1_BASE + UART_UARTRSR_OFFSET) = UART_UARTRSR_ERROR_BITS;
    if (error != UART_ERROR_OVERRUN) {
      return false;
    }
  }

  *byte = (uint8_t)data;
  ++s_debug.rx_bytes;
  s_debug.last_rx_byte = *byte;
  return true;
}

bool fruitjam_esp_hci_write(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (!prv_esp_wait_cts()) {
      ++s_debug.tx_timeout_count;
      return false;
    }
    while (REG32(UART1_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_TXFF_BITS) {
    }
    REG32(UART1_BASE + UART_UARTDR_OFFSET) = data[i];
    ++s_debug.tx_bytes;
  }

  return true;
}

void fruitjam_esp_passthrough_enter_bootloader(uint32_t baud) {
  prv_gpio_output_init(FRUITJAM_PIN_ESP_CS, true);
  prv_gpio_output_init(FRUITJAM_PIN_ESP_IRQ, false);
  prv_gpio_input_init(FRUITJAM_PIN_ESP_BUSY, true);
  prv_uart1_init();
  prv_uart1_set_baud(baud);

  prv_gpio_output_init(FRUITJAM_PIN_ESP_RESET, false);
  prv_delay_ms(ESP_RESET_ASSERT_MS);
  prv_gpio_put(FRUITJAM_PIN_ESP_RESET, true);
  prv_delay_ms(ESP_BOOTLOADER_SETTLE_MS);
  prv_gpio_input_init(FRUITJAM_PIN_ESP_IRQ, true);
  s_debug.drain_bytes += prv_uart1_drain_rx();
}

void fruitjam_esp_passthrough_set_baud(uint32_t baud) {
  prv_uart1_set_baud(baud);
}

bool fruitjam_esp_passthrough_read_byte(uint8_t *byte) {
  return fruitjam_esp_hci_read_byte(byte);
}

bool fruitjam_esp_passthrough_write(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    while (REG32(UART1_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_TXFF_BITS) {
    }
    REG32(UART1_BASE + UART_UARTDR_OFFSET) = data[i];
    ++s_debug.tx_bytes;
  }

  return true;
}

void fruitjam_esp_hci_debug_get_snapshot(FruitJamEspHciDebugSnapshot *snapshot) {
  *snapshot = s_debug;
  snapshot->cts_ready = prv_esp_cts_ready();
}

void fruitjam_esp_hci_debug_record_h4_discard(uint8_t byte) {
  ++s_debug.h4_discard_count;
  s_debug.last_h4_discard_byte = byte;
}

void fruitjam_esp_hci_debug_record_h4_parse_error(int error) {
  ++s_debug.h4_parse_error_count;
  s_debug.last_rx_error = (uint8_t)(-error);
}

void fruitjam_esp_hci_debug_record_h4_frame(uint8_t pkt_type) {
  s_debug.last_h4_type = pkt_type;
  switch (pkt_type) {
    case 0x02:
      ++s_debug.h4_acl_count;
      break;
    case 0x04:
      ++s_debug.h4_evt_count;
      break;
    case 0x05:
      ++s_debug.h4_iso_count;
      break;
    default:
      break;
  }
}

void fruitjam_esp_hci_debug_record_cmd(uint16_t opcode, uint8_t length, bool ok) {
  ++s_debug.hci_cmd_count;
  s_debug.last_hci_cmd_opcode = opcode;
  s_debug.last_hci_cmd_length = length;
  if (!ok) {
    ++s_debug.hci_cmd_fail_count;
  }
}

void fruitjam_esp_hci_debug_record_event(const uint8_t *event, uint16_t length) {
  uint8_t prefix_length = (length < FRUITJAM_ESP_HCI_DEBUG_EVT_PREFIX_SIZE)
                              ? (uint8_t)length
                              : FRUITJAM_ESP_HCI_DEBUG_EVT_PREFIX_SIZE;

  s_debug.last_hci_evt_prefix_length = prefix_length;
  for (uint8_t i = 0; i < prefix_length; ++i) {
    s_debug.last_hci_evt_prefix[i] = event[i];
  }

  if (length < 2U) {
    return;
  }

  const uint8_t event_code = event[0];
  s_debug.last_hci_evt_code = event_code;
  s_debug.last_hci_evt_length = event[1];
  s_debug.last_hci_evt_opcode = 0U;
  s_debug.last_hci_evt_status = 0U;

  if (event_code == HCI_EVENT_COMMAND_COMPLETE && length >= 6U) {
    ++s_debug.hci_evt_cmd_complete_count;
    s_debug.last_hci_evt_opcode = (uint16_t)event[3] | ((uint16_t)event[4] << 8U);
    s_debug.last_hci_evt_status = event[5];
  } else if (event_code == HCI_EVENT_COMMAND_STATUS && length >= 6U) {
    ++s_debug.hci_evt_cmd_status_count;
    s_debug.last_hci_evt_status = event[2];
    s_debug.last_hci_evt_opcode = (uint16_t)event[4] | ((uint16_t)event[5] << 8U);
  } else {
    ++s_debug.hci_evt_other_count;
  }
}
