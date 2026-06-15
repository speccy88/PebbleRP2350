/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"
#include "drivers/uart.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct UARTDeviceState {
  UARTRXInterruptHandler rx_irq_handler;
  UARTTXInterruptHandler tx_irq_handler;
  bool initialized;
  bool rx_int_enabled;
  bool tx_int_enabled;
} UARTDeviceState;

struct UARTDevice {
  UARTDeviceState *state;
  uintptr_t base_addr;
  uint8_t tx_gpio;
  uint8_t rx_gpio;
  uint8_t tx_function;
  uint8_t rx_function;
  uint32_t reset_bits;
  IRQn_Type irqn;
  uint8_t irq_priority;
};

void uart_irq_handler(UARTDevice *dev);
