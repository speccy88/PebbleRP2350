/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "soc/rp2350/rp2350/hardware/timer.h"
#include "soc/rp2350/rp2350/hardware/resets.h"

#include <stdbool.h>
#include <stdint.h>

#define RP2350_CLOCKS_BASE 0x40010000U
#define RP2350_PLL_SYS_BASE 0x40050000U
#define RP2350_PLL_USB_BASE 0x40058000U
#define RP2350_XOSC_BASE 0x40048000U

#define RP2350_CLOCKS_CLK_REF_CTRL_OFFSET 0x30U
#define RP2350_CLOCKS_CLK_REF_DIV_OFFSET 0x34U
#define RP2350_CLOCKS_CLK_REF_SELECTED_OFFSET 0x38U
#define RP2350_CLOCKS_CLK_REF_CTRL_SRC_BITS 0x3U
#define RP2350_CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH 0x0U
#define RP2350_CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC 0x2U
#define RP2350_CLOCKS_CLK_REF_CTRL_AUXSRC_LSB 5U
#define RP2350_CLOCKS_CLK_REF_CTRL_AUXSRC_BITS 0x60U

#define RP2350_CLOCKS_CLK_SYS_CTRL_OFFSET 0x3cU
#define RP2350_CLOCKS_CLK_SYS_DIV_OFFSET 0x40U
#define RP2350_CLOCKS_CLK_SYS_SELECTED_OFFSET 0x44U
#define RP2350_CLOCKS_CLK_SYS_CTRL_SRC_BITS 0x1U
#define RP2350_CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF 0x0U
#define RP2350_CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX 0x1U
#define RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB 5U
#define RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS 0xe0U
#define RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS 0x0U

#define RP2350_CLOCKS_CLK_PERI_CTRL_OFFSET 0x48U
#define RP2350_CLOCKS_CLK_PERI_DIV_OFFSET 0x4cU
#define RP2350_CLOCKS_CLK_PERI_CTRL_ENABLE_BITS (1U << 11)
#define RP2350_CLOCKS_CLK_PERI_CTRL_ENABLED_BITS (1U << 28)
#define RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB 5U
#define RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_MASK (0xfU << RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB)
#define RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS 0x0U
#define RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC 0x4U
#define RP2350_CLOCKS_CLK_PERI_DIV_1 0x00010000U

#define RP2350_CLOCKS_CLK_HSTX_CTRL_OFFSET 0x54U
#define RP2350_CLOCKS_CLK_HSTX_DIV_OFFSET 0x58U
#define RP2350_CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS 0x0U

#define RP2350_CLOCKS_CLK_USB_CTRL_OFFSET 0x60U
#define RP2350_CLOCKS_CLK_USB_DIV_OFFSET 0x64U
#define RP2350_CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB 0x0U

#define RP2350_CLOCKS_CLK_ADC_CTRL_OFFSET 0x6cU
#define RP2350_CLOCKS_CLK_ADC_DIV_OFFSET 0x70U
#define RP2350_CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB 0x0U

#define RP2350_CLOCKS_CLK_SYS_RESUS_CTRL_OFFSET 0x84U

#define RP2350_CLOCKS_AUX_CTRL_ENABLE_BITS (1U << 11)
#define RP2350_CLOCKS_AUX_CTRL_ENABLED_BITS (1U << 28)
#define RP2350_CLOCKS_AUX_CTRL_AUXSRC_LSB 5U
#define RP2350_CLOCKS_AUX_CTRL_AUXSRC_MASK 0xe0U
#define RP2350_CLOCKS_DIV_1 0x00010000U

#define RP2350_PLL_CS_OFFSET 0x00U
#define RP2350_PLL_PWR_OFFSET 0x04U
#define RP2350_PLL_FBDIV_INT_OFFSET 0x08U
#define RP2350_PLL_PRIM_OFFSET 0x0cU
#define RP2350_PLL_CS_LOCK_BITS (1U << 31)
#define RP2350_PLL_PWR_PD_BITS (1U << 0)
#define RP2350_PLL_PWR_POSTDIVPD_BITS (1U << 3)
#define RP2350_PLL_PWR_VCOPD_BITS (1U << 5)
#define RP2350_PLL_PRIM_POSTDIV1_LSB 16U
#define RP2350_PLL_PRIM_POSTDIV2_LSB 12U
#define RP2350_PLL_SYS_REFDIV 1U
#define RP2350_PLL_SYS_FBDIV 125U
#define RP2350_PLL_SYS_POSTDIV1 5U
#define RP2350_PLL_SYS_POSTDIV2 2U
#define RP2350_PLL_USB_REFDIV 1U
#define RP2350_PLL_USB_FBDIV 100U
#define RP2350_PLL_USB_POSTDIV1 5U
#define RP2350_PLL_USB_POSTDIV2 5U
#define RP2350_USB_CLOCK_WAIT_LIMIT 2000000U
#define RP2350_SYS_CLK_HZ 150000000U
#define RP2350_USB_CLK_HZ 48000000U

#define RP2350_XOSC_CTRL_OFFSET 0x00U
#define RP2350_XOSC_STATUS_OFFSET 0x04U
#define RP2350_XOSC_STARTUP_OFFSET 0x0cU
#define RP2350_XOSC_CTRL_ENABLE_LSB 12U
#define RP2350_XOSC_CTRL_ENABLE_VALUE_ENABLE 0xfabU
#define RP2350_XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ 0xaa0U
#define RP2350_XOSC_STATUS_STABLE_BITS (1U << 31)
#define RP2350_XOSC_STARTUP_DELAY_12MHZ 47U

extern uint32_t SystemCoreClock;

static inline volatile uint32_t *rp2350_mmio32(uint32_t address) {
  return (volatile uint32_t *)address;
}

static inline bool rp2350_wait_for_bits(uint32_t address, uint32_t mask, uint32_t value) {
  for (uint32_t i = 0; i < RP2350_USB_CLOCK_WAIT_LIMIT; ++i) {
    if ((*rp2350_mmio32(address) & mask) == value) {
      return true;
    }
  }
  return false;
}

static inline void rp2350_write_masked(uint32_t address, uint32_t value, uint32_t mask) {
  volatile uint32_t *reg = rp2350_mmio32(address);
  *reg = (*reg & ~mask) | (value & mask);
}

static inline void rp2350_xosc_enable(void) {
  if (*rp2350_mmio32(RP2350_XOSC_BASE + RP2350_XOSC_STATUS_OFFSET) &
      RP2350_XOSC_STATUS_STABLE_BITS) {
    return;
  }

  *rp2350_mmio32(RP2350_XOSC_BASE + RP2350_XOSC_CTRL_OFFSET) =
      RP2350_XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;
  *rp2350_mmio32(RP2350_XOSC_BASE + RP2350_XOSC_STARTUP_OFFSET) = RP2350_XOSC_STARTUP_DELAY_12MHZ;
  *rp2350_mmio32(RP2350_XOSC_BASE + RP2350_XOSC_CTRL_OFFSET) =
      (RP2350_XOSC_CTRL_ENABLE_VALUE_ENABLE << RP2350_XOSC_CTRL_ENABLE_LSB) |
      RP2350_XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;
  (void)rp2350_wait_for_bits(RP2350_XOSC_BASE + RP2350_XOSC_STATUS_OFFSET,
                             RP2350_XOSC_STATUS_STABLE_BITS, RP2350_XOSC_STATUS_STABLE_BITS);
}

static inline bool rp2350_clock_configure_glitchless(uint32_t ctrl_offset, uint32_t div_offset,
                                                     uint32_t selected_offset, uint32_t src,
                                                     uint32_t auxsrc, uint32_t src_bits,
                                                     uint32_t auxsrc_bits) {
  const uint32_t ctrl = RP2350_CLOCKS_BASE + ctrl_offset;
  const uint32_t selected = RP2350_CLOCKS_BASE + selected_offset;

  rp2350_write_masked(ctrl, 0U, src_bits);
  if (!rp2350_wait_for_bits(selected, 1U, 1U)) {
    return false;
  }

  *rp2350_mmio32(RP2350_CLOCKS_BASE + div_offset) = RP2350_CLOCKS_DIV_1;
  rp2350_write_masked(ctrl, auxsrc << RP2350_CLOCKS_AUX_CTRL_AUXSRC_LSB, auxsrc_bits);
  rp2350_write_masked(ctrl, src, src_bits);

  return rp2350_wait_for_bits(selected, 1U << src, 1U << src);
}

static inline bool rp2350_clock_configure_aux(uint32_t ctrl_offset, uint32_t div_offset,
                                              uint32_t auxsrc) {
  const uint32_t ctrl = RP2350_CLOCKS_BASE + ctrl_offset;

  rp2350_write_masked(ctrl, 0U, RP2350_CLOCKS_AUX_CTRL_ENABLE_BITS);
  (void)rp2350_wait_for_bits(ctrl, RP2350_CLOCKS_AUX_CTRL_ENABLED_BITS, 0U);
  *rp2350_mmio32(RP2350_CLOCKS_BASE + div_offset) = RP2350_CLOCKS_DIV_1;
  rp2350_write_masked(ctrl, auxsrc << RP2350_CLOCKS_AUX_CTRL_AUXSRC_LSB,
                      RP2350_CLOCKS_AUX_CTRL_AUXSRC_MASK);
  rp2350_write_masked(ctrl, RP2350_CLOCKS_AUX_CTRL_ENABLE_BITS,
                      RP2350_CLOCKS_AUX_CTRL_ENABLE_BITS);

  return rp2350_wait_for_bits(ctrl, RP2350_CLOCKS_AUX_CTRL_ENABLED_BITS,
                              RP2350_CLOCKS_AUX_CTRL_ENABLED_BITS);
}

static inline bool rp2350_pll_init(uint32_t pll_base, uint32_t reset_bits, uint32_t refdiv,
                                   uint32_t fbdiv, uint32_t postdiv1, uint32_t postdiv2) {
  reset_block(reset_bits);
  unreset_block_wait(reset_bits);

  *rp2350_mmio32(pll_base + RP2350_PLL_CS_OFFSET) = refdiv;
  *rp2350_mmio32(pll_base + RP2350_PLL_FBDIV_INT_OFFSET) = fbdiv;
  *rp2350_mmio32(pll_base + RP2350_PLL_PWR_OFFSET) &=
      ~(RP2350_PLL_PWR_PD_BITS | RP2350_PLL_PWR_VCOPD_BITS);
  if (!rp2350_wait_for_bits(pll_base + RP2350_PLL_CS_OFFSET, RP2350_PLL_CS_LOCK_BITS,
                            RP2350_PLL_CS_LOCK_BITS)) {
    return false;
  }

  *rp2350_mmio32(pll_base + RP2350_PLL_PRIM_OFFSET) =
      (postdiv1 << RP2350_PLL_PRIM_POSTDIV1_LSB) |
      (postdiv2 << RP2350_PLL_PRIM_POSTDIV2_LSB);
  *rp2350_mmio32(pll_base + RP2350_PLL_PWR_OFFSET) &= ~RP2350_PLL_PWR_POSTDIVPD_BITS;
  return true;
}

static inline bool rp2350_clk_ref_enable_xosc(void) {
  rp2350_xosc_enable();
  return rp2350_clock_configure_glitchless(
      RP2350_CLOCKS_CLK_REF_CTRL_OFFSET, RP2350_CLOCKS_CLK_REF_DIV_OFFSET,
      RP2350_CLOCKS_CLK_REF_SELECTED_OFFSET, RP2350_CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0U,
      RP2350_CLOCKS_CLK_REF_CTRL_SRC_BITS, RP2350_CLOCKS_CLK_REF_CTRL_AUXSRC_BITS);
}

static inline bool rp2350_usb_clocks_init(void) {
  if (!rp2350_clk_ref_enable_xosc()) {
    return false;
  }

  if (!rp2350_pll_init(RP2350_PLL_USB_BASE, RESETS_RESET_PLL_USB_BITS, RP2350_PLL_USB_REFDIV,
                       RP2350_PLL_USB_FBDIV, RP2350_PLL_USB_POSTDIV1,
                       RP2350_PLL_USB_POSTDIV2)) {
    return false;
  }

  if (!rp2350_clock_configure_aux(RP2350_CLOCKS_CLK_USB_CTRL_OFFSET,
                                  RP2350_CLOCKS_CLK_USB_DIV_OFFSET,
                                  RP2350_CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB)) {
    return false;
  }

  if (!rp2350_clock_configure_aux(RP2350_CLOCKS_CLK_ADC_CTRL_OFFSET,
                                  RP2350_CLOCKS_CLK_ADC_DIV_OFFSET,
                                  RP2350_CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB)) {
    return false;
  }

  rp2350_timer_init_ticks();
  return true;
}

static inline bool rp2350_runtime_clocks_init(bool configure_clk_peri_from_sys) {
  *rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_SYS_RESUS_CTRL_OFFSET) = 0U;

  if (!rp2350_clk_ref_enable_xosc()) {
    return false;
  }

  if (!rp2350_clock_configure_glitchless(
          RP2350_CLOCKS_CLK_SYS_CTRL_OFFSET, RP2350_CLOCKS_CLK_SYS_DIV_OFFSET,
          RP2350_CLOCKS_CLK_SYS_SELECTED_OFFSET, RP2350_CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0U,
          RP2350_CLOCKS_CLK_SYS_CTRL_SRC_BITS, RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS)) {
    return false;
  }

  if (!rp2350_pll_init(RP2350_PLL_SYS_BASE, RESETS_RESET_PLL_SYS_BITS, RP2350_PLL_SYS_REFDIV,
                       RP2350_PLL_SYS_FBDIV, RP2350_PLL_SYS_POSTDIV1,
                       RP2350_PLL_SYS_POSTDIV2)) {
    return false;
  }

  if (!rp2350_pll_init(RP2350_PLL_USB_BASE, RESETS_RESET_PLL_USB_BITS, RP2350_PLL_USB_REFDIV,
                       RP2350_PLL_USB_FBDIV, RP2350_PLL_USB_POSTDIV1,
                       RP2350_PLL_USB_POSTDIV2)) {
    return false;
  }

  if (!rp2350_clock_configure_glitchless(
          RP2350_CLOCKS_CLK_SYS_CTRL_OFFSET, RP2350_CLOCKS_CLK_SYS_DIV_OFFSET,
          RP2350_CLOCKS_CLK_SYS_SELECTED_OFFSET,
          RP2350_CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
          RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
          RP2350_CLOCKS_CLK_SYS_CTRL_SRC_BITS, RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS)) {
    return false;
  }

  if (!rp2350_clock_configure_aux(RP2350_CLOCKS_CLK_USB_CTRL_OFFSET,
                                  RP2350_CLOCKS_CLK_USB_DIV_OFFSET,
                                  RP2350_CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB)) {
    return false;
  }

  if (!rp2350_clock_configure_aux(RP2350_CLOCKS_CLK_ADC_CTRL_OFFSET,
                                  RP2350_CLOCKS_CLK_ADC_DIV_OFFSET,
                                  RP2350_CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB)) {
    return false;
  }

  if (configure_clk_peri_from_sys &&
      !rp2350_clock_configure_aux(RP2350_CLOCKS_CLK_PERI_CTRL_OFFSET,
                                  RP2350_CLOCKS_CLK_PERI_DIV_OFFSET,
                                  RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS)) {
    return false;
  }

  if (configure_clk_peri_from_sys &&
      !rp2350_clock_configure_aux(RP2350_CLOCKS_CLK_HSTX_CTRL_OFFSET,
                                  RP2350_CLOCKS_CLK_HSTX_DIV_OFFSET,
                                  RP2350_CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS)) {
    return false;
  }

  SystemCoreClock = RP2350_SYS_CLK_HZ;
  rp2350_timer_init_ticks();
  return true;
}

static inline void rp2350_clk_peri_enable_xosc(void) {
  rp2350_xosc_enable();

  *rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_PERI_CTRL_OFFSET) = 0U;
  *rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_PERI_DIV_OFFSET) =
      RP2350_CLOCKS_CLK_PERI_DIV_1;
  *rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_PERI_CTRL_OFFSET) =
      RP2350_CLOCKS_CLK_PERI_CTRL_ENABLE_BITS |
      (RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC
       << RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB);

  while (!(*rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_PERI_CTRL_OFFSET) &
           RP2350_CLOCKS_CLK_PERI_CTRL_ENABLED_BITS)) {
  }
}

static inline uint32_t rp2350_clk_peri_ctrl(void) {
  return *rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_PERI_CTRL_OFFSET);
}

static inline uint32_t rp2350_clk_peri_div(void) {
  return *rp2350_mmio32(RP2350_CLOCKS_BASE + RP2350_CLOCKS_CLK_PERI_DIV_OFFSET);
}

static inline uint32_t rp2350_clk_peri_source_hz(void) {
  const uint32_t ctrl = rp2350_clk_peri_ctrl();
  if (!(ctrl & RP2350_CLOCKS_CLK_PERI_CTRL_ENABLE_BITS)) {
    return 0U;
  }

  const uint32_t auxsrc =
      (ctrl & RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_MASK) >> RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB;
  if (auxsrc == RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC) {
    return RP2350_XOSC_HZ;
  }
  if (auxsrc == RP2350_CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS) {
    return SystemCoreClock;
  }

  return 0U;
}
