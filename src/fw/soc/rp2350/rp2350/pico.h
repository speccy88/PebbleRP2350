/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PICO_RP2040 0
#define PICO_RP2350 1

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif

#ifndef __no_inline_not_in_flash_func
#define __no_inline_not_in_flash_func(name) name
#endif

#ifndef panic
#define panic(...) assert(false)
#endif

#ifndef hard_assert
#define hard_assert(condition) assert(condition)
#endif

typedef unsigned int uint;

static inline uint rp2040_chip_version(void) {
  return 2U;
}

static inline void busy_wait_at_least_cycles(uint32_t minimum_cycles) {
  for (volatile uint32_t i = 0; i < minimum_cycles; ++i) {
    __asm__ volatile("nop");
  }
}
