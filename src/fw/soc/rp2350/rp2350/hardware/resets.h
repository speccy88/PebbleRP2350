/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define RP2350_RESETS_BASE 0x40020000UL
#define RP2350_RESETS_RESET_OFFSET 0x00UL
#define RP2350_RESETS_RESET_DONE_OFFSET 0x08UL
#define RP2350_REG_ALIAS_SET_BITS 0x2000UL
#define RP2350_REG_ALIAS_CLR_BITS 0x3000UL

#define RESETS_RESET_PLL_USB_BITS (1UL << 15)
#define RESETS_RESET_PLL_SYS_BITS (1UL << 14)
#define RESETS_RESET_USBCTRL_BITS (1UL << 28)

#define RP2350_REG32(addr) (*(volatile uint32_t *)(addr))

static inline void reset_block(uint32_t bits) {
  RP2350_REG32(RP2350_RESETS_BASE + RP2350_RESETS_RESET_OFFSET + RP2350_REG_ALIAS_SET_BITS) =
      bits;
}

static inline void unreset_block_wait(uint32_t bits) {
  RP2350_REG32(RP2350_RESETS_BASE + RP2350_RESETS_RESET_OFFSET + RP2350_REG_ALIAS_CLR_BITS) =
      bits;
  while ((RP2350_REG32(RP2350_RESETS_BASE + RP2350_RESETS_RESET_DONE_OFFSET) & bits) != bits) {
  }
}
