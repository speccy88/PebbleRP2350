/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/uart/rp2350.h"

#include "FreeRTOS.h"
#include "soc/rp2350/rp2350/hardware/clocks.h"

#include <cmsis_core.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define RESETS_BASE 0x40020000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define REG_ALIAS_SET_BITS 0x2000U
#define REG_ALIAS_CLR_BITS 0x3000U

#define GPIO_PAD_NORMAL 0x52U

#define RESETS_RESET_OFFSET 0x00U
#define RESETS_RESET_DONE_OFFSET 0x08U

#define UART_UARTDR_OFFSET 0x00U
#define UART_UARTRSR_OFFSET 0x04U
#define UART_UARTFR_OFFSET 0x18U
#define UART_UARTIBRD_OFFSET 0x24U
#define UART_UARTFBRD_OFFSET 0x28U
#define UART_UARTLCR_H_OFFSET 0x2cU
#define UART_UARTCR_OFFSET 0x30U
#define UART_UARTIMSC_OFFSET 0x38U
#define UART_UARTMIS_OFFSET 0x40U
#define UART_UARTICR_OFFSET 0x44U
#define UART_UARTDMACR_OFFSET 0x48U

#define UART_UARTDR_FE_BITS 0x00000100U
#define UART_UARTDR_PE_BITS 0x00000200U
#define UART_UARTDR_BE_BITS 0x00000400U
#define UART_UARTDR_OE_BITS 0x00000800U
#define UART_UARTDR_ERROR_BITS \
  (UART_UARTDR_FE_BITS | UART_UARTDR_PE_BITS | UART_UARTDR_BE_BITS | UART_UARTDR_OE_BITS)

#define UART_UARTRSR_FE_BITS 0x00000001U
#define UART_UARTRSR_PE_BITS 0x00000002U
#define UART_UARTRSR_BE_BITS 0x00000004U
#define UART_UARTRSR_OE_BITS 0x00000008U
#define UART_UARTRSR_ERROR_BITS \
  (UART_UARTRSR_FE_BITS | UART_UARTRSR_PE_BITS | UART_UARTRSR_BE_BITS | UART_UARTRSR_OE_BITS)

#define UART_UARTFR_TXFE_BITS 0x00000080U
#define UART_UARTFR_RXFF_BITS 0x00000040U
#define UART_UARTFR_TXFF_BITS 0x00000020U
#define UART_UARTFR_RXFE_BITS 0x00000010U
#define UART_UARTFR_BUSY_BITS 0x00000008U

#define UART_UARTLCR_H_FEN_BITS 0x00000010U
#define UART_UARTLCR_H_WLEN_8_BITS (3U << 5)

#define UART_UARTCR_RXE_BITS 0x00000200U
#define UART_UARTCR_TXE_BITS 0x00000100U
#define UART_UARTCR_UARTEN_BITS 0x00000001U

#define UART_UARTIMSC_RXIM_BITS 0x00000010U
#define UART_UARTIMSC_TXIM_BITS 0x00000020U
#define UART_UARTIMSC_RTIM_BITS 0x00000040U
#define UART_UARTIMSC_FEIM_BITS 0x00000080U
#define UART_UARTIMSC_PEIM_BITS 0x00000100U
#define UART_UARTIMSC_BEIM_BITS 0x00000200U
#define UART_UARTIMSC_OEIM_BITS 0x00000400U
#define UART_UARTIMSC_RX_ERROR_BITS                                              \
  (UART_UARTIMSC_FEIM_BITS | UART_UARTIMSC_PEIM_BITS | UART_UARTIMSC_BEIM_BITS | \
   UART_UARTIMSC_OEIM_BITS)

#define UART_UARTMIS_RXMIS_BITS 0x00000010U
#define UART_UARTMIS_TXMIS_BITS 0x00000020U
#define UART_UARTMIS_RTMIS_BITS 0x00000040U
#define UART_UARTMIS_ERROR_BITS 0x00000780U

#define UART_UARTICR_ALL_BITS 0x000007ffU

static void prv_delay_cycles(uint32_t cycles) {
  for (volatile uint32_t i = 0; i < cycles; ++i) {
    __asm__ volatile("nop");
  }
}

static void prv_gpio_set_function(uint8_t pin, uint8_t function) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = function;
}

static void prv_reset_uart(UARTDevice *dev) {
  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_SET_BITS) = dev->reset_bits;
  prv_delay_cycles(64U);
  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_CLR_BITS) = dev->reset_bits;
  while (!(REG32(RESETS_BASE + RESETS_RESET_DONE_OFFSET) & dev->reset_bits)) {
  }
}

static void prv_set_nvic_enabled(UARTDevice *dev, bool enabled) {
  if (enabled) {
    NVIC_SetPriority(dev->irqn, dev->irq_priority);
    NVIC_EnableIRQ(dev->irqn);
  } else {
    NVIC_DisableIRQ(dev->irqn);
  }
}

static UARTRXErrorFlags prv_error_flags_from_dr(uint32_t data) {
  UARTRXErrorFlags flags = {
      .parity_error = (data & UART_UARTDR_PE_BITS) != 0U,
      .overrun_error = (data & UART_UARTDR_OE_BITS) != 0U,
      .framing_error = (data & (UART_UARTDR_FE_BITS | UART_UARTDR_BE_BITS)) != 0U,
  };

  return flags;
}

static UARTRXErrorFlags prv_error_flags_from_rsr(uint32_t status) {
  UARTRXErrorFlags flags = {
      .parity_error = (status & UART_UARTRSR_PE_BITS) != 0U,
      .overrun_error = (status & UART_UARTRSR_OE_BITS) != 0U,
      .framing_error = (status & (UART_UARTRSR_FE_BITS | UART_UARTRSR_BE_BITS)) != 0U,
  };

  return flags;
}

static void prv_clear_rx_errors(UARTDevice *dev) {
  REG32(dev->base_addr + UART_UARTRSR_OFFSET) = UART_UARTRSR_ERROR_BITS;
}

static void prv_init(UARTDevice *dev, bool tx_enabled, bool rx_enabled) {
  dev->state->rx_int_enabled = false;
  dev->state->tx_int_enabled = false;

  rp2350_clk_peri_enable_xosc();
  prv_reset_uart(dev);

  REG32(dev->base_addr + UART_UARTCR_OFFSET) = 0U;
  REG32(dev->base_addr + UART_UARTICR_OFFSET) = UART_UARTICR_ALL_BITS;
  REG32(dev->base_addr + UART_UARTIMSC_OFFSET) = 0U;
  REG32(dev->base_addr + UART_UARTDMACR_OFFSET) = 0U;
  prv_clear_rx_errors(dev);
  uart_set_baud_rate(dev, 115200U);
  REG32(dev->base_addr + UART_UARTLCR_H_OFFSET) =
      UART_UARTLCR_H_WLEN_8_BITS | UART_UARTLCR_H_FEN_BITS;

  uint32_t control = UART_UARTCR_UARTEN_BITS;
  if (tx_enabled) {
    prv_gpio_set_function(dev->tx_gpio, dev->tx_function);
    control |= UART_UARTCR_TXE_BITS;
  }
  if (rx_enabled) {
    prv_gpio_set_function(dev->rx_gpio, dev->rx_function);
    control |= UART_UARTCR_RXE_BITS;
  }

  REG32(dev->base_addr + UART_UARTCR_OFFSET) = control;
  dev->state->initialized = true;
}

void uart_init(UARTDevice *dev) {
  prv_init(dev, true, true);
}

void uart_init_open_drain(UARTDevice *dev) {
  prv_init(dev, true, true);
}

void uart_init_tx_only(UARTDevice *dev) {
  prv_init(dev, true, false);
}

void uart_init_rx_only(UARTDevice *dev) {
  prv_init(dev, false, true);
}

void uart_deinit(UARTDevice *dev) {
  while (!uart_is_tx_complete(dev)) {
  }

  REG32(dev->base_addr + UART_UARTCR_OFFSET) = 0U;
  REG32(dev->base_addr + UART_UARTIMSC_OFFSET) = 0U;
  REG32(dev->base_addr + UART_UARTICR_OFFSET) = UART_UARTICR_ALL_BITS;
  prv_clear_rx_errors(dev);
  prv_set_nvic_enabled(dev, false);
  dev->state->initialized = false;
}

void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate) {
  if (baud_rate == 0U) {
    return;
  }

  const uint64_t scaled_divisor = ((uint64_t)8U * RP2350_XOSC_HZ / baud_rate) + 1U;
  uint32_t baud_ibrd = (uint32_t)(scaled_divisor >> 7);
  uint32_t baud_fbrd;

  if (baud_ibrd == 0U) {
    baud_ibrd = 1U;
    baud_fbrd = 0U;
  } else if (baud_ibrd >= 65535U) {
    baud_ibrd = 65535U;
    baud_fbrd = 0U;
  } else {
    baud_fbrd = (uint32_t)((scaled_divisor & 0x7fU) >> 1);
  }

  REG32(dev->base_addr + UART_UARTIBRD_OFFSET) = baud_ibrd;
  REG32(dev->base_addr + UART_UARTFBRD_OFFSET) = baud_fbrd;
  REG32(dev->base_addr + UART_UARTLCR_H_OFFSET) =
      UART_UARTLCR_H_WLEN_8_BITS | UART_UARTLCR_H_FEN_BITS;
}

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  while (!uart_is_tx_ready(dev)) {
  }
  REG32(dev->base_addr + UART_UARTDR_OFFSET) = data;
}

uint8_t uart_read_byte(UARTDevice *dev) {
  const uint32_t data = REG32(dev->base_addr + UART_UARTDR_OFFSET);
  if (data & UART_UARTDR_ERROR_BITS) {
    prv_clear_rx_errors(dev);
  }

  return (uint8_t)data;
}

void uart_start_rx_dma(UARTDevice *dev, void *buffer, uint32_t length) {
  (void)dev;
  (void)buffer;
  (void)length;
}

void uart_stop_rx_dma(UARTDevice *dev) {
  (void)dev;
}

void uart_clear_rx_dma_buffer(UARTDevice *dev) {
  (void)dev;
}

bool uart_is_rx_ready(UARTDevice *dev) {
  return (REG32(dev->base_addr + UART_UARTFR_OFFSET) & UART_UARTFR_RXFE_BITS) == 0U;
}

bool uart_has_rx_overrun(UARTDevice *dev) {
  return (REG32(dev->base_addr + UART_UARTRSR_OFFSET) & UART_UARTRSR_OE_BITS) != 0U;
}

bool uart_has_rx_framing_error(UARTDevice *dev) {
  return (REG32(dev->base_addr + UART_UARTRSR_OFFSET) &
          (UART_UARTRSR_FE_BITS | UART_UARTRSR_BE_BITS)) != 0U;
}

bool uart_is_tx_ready(UARTDevice *dev) {
  return (REG32(dev->base_addr + UART_UARTFR_OFFSET) & UART_UARTFR_TXFF_BITS) == 0U;
}

bool uart_is_tx_complete(UARTDevice *dev) {
  const uint32_t flags = REG32(dev->base_addr + UART_UARTFR_OFFSET);
  return (flags & (UART_UARTFR_BUSY_BITS | UART_UARTFR_TXFE_BITS)) == UART_UARTFR_TXFE_BITS;
}

void uart_wait_for_tx_complete(UARTDevice *dev) {
  while (!uart_is_tx_complete(dev)) {
  }
}

UARTRXErrorFlags uart_has_errored_out(UARTDevice *dev) {
  return prv_error_flags_from_rsr(REG32(dev->base_addr + UART_UARTRSR_OFFSET));
}

void uart_clear_all_interrupt_flags(UARTDevice *dev) {
  REG32(dev->base_addr + UART_UARTICR_OFFSET) = UART_UARTICR_ALL_BITS;
  prv_clear_rx_errors(dev);
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
  dev->state->rx_irq_handler = irq_handler;
}

void uart_set_tx_interrupt_handler(UARTDevice *dev, UARTTXInterruptHandler irq_handler) {
  dev->state->tx_irq_handler = irq_handler;
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  uint32_t mask = REG32(dev->base_addr + UART_UARTIMSC_OFFSET);
  const uint32_t rx_mask =
      UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_RX_ERROR_BITS;

  if (enabled) {
    dev->state->rx_int_enabled = true;
    REG32(dev->base_addr + UART_UARTIMSC_OFFSET) = mask | rx_mask;
  } else {
    REG32(dev->base_addr + UART_UARTIMSC_OFFSET) = mask & ~rx_mask;
    dev->state->rx_int_enabled = false;
  }

  prv_set_nvic_enabled(dev, dev->state->rx_int_enabled || dev->state->tx_int_enabled);
}

void uart_set_tx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  uint32_t mask = REG32(dev->base_addr + UART_UARTIMSC_OFFSET);

  if (enabled) {
    dev->state->tx_int_enabled = true;
    REG32(dev->base_addr + UART_UARTIMSC_OFFSET) = mask | UART_UARTIMSC_TXIM_BITS;
  } else {
    REG32(dev->base_addr + UART_UARTIMSC_OFFSET) = mask & ~UART_UARTIMSC_TXIM_BITS;
    dev->state->tx_int_enabled = false;
  }

  prv_set_nvic_enabled(dev, dev->state->rx_int_enabled || dev->state->tx_int_enabled);
}

void uart_irq_handler(UARTDevice *dev) {
  bool should_context_switch = false;
  const uint32_t masked_status = REG32(dev->base_addr + UART_UARTMIS_OFFSET);

  if ((masked_status &
       (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS | UART_UARTMIS_ERROR_BITS)) != 0U) {
    while (dev->state->rx_int_enabled && uart_is_rx_ready(dev)) {
      const uint32_t data = REG32(dev->base_addr + UART_UARTDR_OFFSET);
      UARTRXErrorFlags error_flags = prv_error_flags_from_dr(data);

      if (data & UART_UARTDR_ERROR_BITS) {
        prv_clear_rx_errors(dev);
      }

      if (dev->state->rx_irq_handler) {
        should_context_switch |= dev->state->rx_irq_handler(dev, (uint8_t)data, &error_flags);
      }
    }

    REG32(dev->base_addr + UART_UARTICR_OFFSET) =
        UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS | UART_UARTMIS_ERROR_BITS;
  }

  if ((masked_status & UART_UARTMIS_TXMIS_BITS) != 0U) {
    REG32(dev->base_addr + UART_UARTICR_OFFSET) = UART_UARTMIS_TXMIS_BITS;
    if (dev->state->tx_irq_handler && dev->state->tx_int_enabled) {
      should_context_switch |= dev->state->tx_irq_handler(dev);
    }
  }

  portEND_SWITCHING_ISR(should_context_switch);
}
