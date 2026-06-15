/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <cmsis_core.h>

#define USBCTRL_IRQ USBCTRL_IRQ_IRQn
#define PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY 0xff

typedef void (*irq_handler_t)(void);

void fruitjam_usb_irq_set_handler(irq_handler_t handler);

static inline void irq_add_shared_handler(int irq_num, irq_handler_t handler, uint8_t order_priority) {
  (void)order_priority;
  if (irq_num == USBCTRL_IRQ) {
    fruitjam_usb_irq_set_handler(handler);
  }
}

static inline void irq_remove_handler(int irq_num, irq_handler_t handler) {
  (void)handler;
  if (irq_num == USBCTRL_IRQ) {
    fruitjam_usb_irq_set_handler(NULL);
  }
}

static inline void irq_set_enabled(int irq_num, bool enabled) {
  if (enabled) {
    NVIC_EnableIRQ((IRQn_Type)irq_num);
  } else {
    NVIC_DisableIRQ((IRQn_Type)irq_num);
  }
}
