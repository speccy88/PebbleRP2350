/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define remove_volatile_cast(type, value) (type)(value)
#define remove_volatile_cast_no_barrier(type, value) (type)(value)

static inline void __dsb(void) {
  __asm volatile("dsb" ::: "memory");
}

static inline void __isb(void) {
  __asm volatile("isb" ::: "memory");
}
