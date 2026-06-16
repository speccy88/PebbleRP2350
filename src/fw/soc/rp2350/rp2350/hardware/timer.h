/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define RP2350_TIMER0_BASE 0x400b0000U
#define RP2350_TIMER_TIMERAWL_OFFSET 0x28U
#define RP2350_TICKS_BASE 0x40108000U
#define RP2350_TICKS_SLICE_STRIDE 0x0cU
#define RP2350_TICKS_CTRL_OFFSET 0x00U
#define RP2350_TICKS_CYCLES_OFFSET 0x04U
#define RP2350_TICKS_COUNT_OFFSET 0x08U
#define RP2350_TICKS_CTRL_ENABLE 0x00000001U
#define RP2350_TICKS_CTRL_RUNNING 0x00000002U
#define RP2350_TICKS_TIMER0 2U
#define RP2350_TICKS_COUNT 6U
#define RP2350_XOSC_HZ 12000000U
#define RP2350_TIMER_TICK_HZ 1000000U
#define RP2350_TIMER_TICK_CYCLES (RP2350_XOSC_HZ / RP2350_TIMER_TICK_HZ)

static inline volatile uint32_t *rp2350_tick_register(uint32_t tick, uint32_t offset) {
  return (volatile uint32_t *)(RP2350_TICKS_BASE + (tick * RP2350_TICKS_SLICE_STRIDE) + offset);
}

static inline void rp2350_timer_init_ticks(void) {
  for (uint32_t tick = 0; tick < RP2350_TICKS_COUNT; ++tick) {
    *rp2350_tick_register(tick, RP2350_TICKS_CYCLES_OFFSET) = RP2350_TIMER_TICK_CYCLES;
    *rp2350_tick_register(tick, RP2350_TICKS_CTRL_OFFSET) = RP2350_TICKS_CTRL_ENABLE;
  }
}

static inline uint32_t time_us_32(void) {
  return *(volatile uint32_t *)(RP2350_TIMER0_BASE + RP2350_TIMER_TIMERAWL_OFFSET);
}

static inline uint32_t rp2350_timer0_tick_ctrl(void) {
  return *rp2350_tick_register(RP2350_TICKS_TIMER0, RP2350_TICKS_CTRL_OFFSET);
}

static inline uint32_t rp2350_timer0_tick_cycles(void) {
  return *rp2350_tick_register(RP2350_TICKS_TIMER0, RP2350_TICKS_CYCLES_OFFSET);
}

static inline uint32_t rp2350_timer0_tick_count(void) {
  return *rp2350_tick_register(RP2350_TICKS_TIMER0, RP2350_TICKS_COUNT_OFFSET);
}
