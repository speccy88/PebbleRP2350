/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_psram.h"

#include "board/board.h"
#include "board/boards/board_fruitjam_rp2350.h"
#include "util/attributes.h"

#include <cmsis_core.h>
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define XIP_CTRL_BASE 0x400c8000U
#define XIP_QMI_BASE 0x400d0000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define GPIO_FUNC_XIP_CS1 9U
#define GPIO_PAD_NORMAL 0x52U

#define XIP_CTRL_OFFSET 0x00U
#define XIP_CTRL_WRITABLE_M1_BITS (1U << 11)

#define QMI_DIRECT_CSR_OFFSET 0x00U
#define QMI_DIRECT_TX_OFFSET 0x04U
#define QMI_DIRECT_RX_OFFSET 0x08U
#define QMI_M1_TIMING_OFFSET 0x20U
#define QMI_M1_RFMT_OFFSET 0x24U
#define QMI_M1_RCMD_OFFSET 0x28U
#define QMI_M1_WFMT_OFFSET 0x2cU
#define QMI_M1_WCMD_OFFSET 0x30U

#define QMI_DIRECT_CSR_CLKDIV_LSB 22U
#define QMI_DIRECT_CSR_TXEMPTY_BITS (1U << 11)
#define QMI_DIRECT_CSR_ASSERT_CS1N_BITS (1U << 3)
#define QMI_DIRECT_CSR_BUSY_BITS (1U << 1)
#define QMI_DIRECT_CSR_EN_BITS (1U << 0)

#define QMI_DIRECT_TX_OE_BITS (1U << 19)
#define QMI_DIRECT_TX_IWIDTH_LSB 16U
#define QMI_DIRECT_TX_IWIDTH_VALUE_Q 2U

#define QMI_M1_TIMING_COOLDOWN_LSB 30U
#define QMI_M1_TIMING_PAGEBREAK_LSB 28U
#define QMI_M1_TIMING_PAGEBREAK_VALUE_1024 2U
#define QMI_M1_TIMING_SELECT_HOLD_LSB 23U
#define QMI_M1_TIMING_MAX_SELECT_LSB 17U
#define QMI_M1_TIMING_MIN_DESELECT_LSB 12U
#define QMI_M1_TIMING_RXDELAY_LSB 8U
#define QMI_M1_TIMING_CLKDIV_LSB 0U

#define QMI_M1_XFMT_DUMMY_LEN_LSB 16U
#define QMI_M1_XFMT_DUMMY_LEN_VALUE_NONE 0U
#define QMI_M1_XFMT_DUMMY_LEN_VALUE_24 6U
#define QMI_M1_XFMT_PREFIX_LEN_LSB 12U
#define QMI_M1_XFMT_PREFIX_LEN_VALUE_8 1U
#define QMI_M1_XFMT_DATA_WIDTH_LSB 8U
#define QMI_M1_XFMT_DUMMY_WIDTH_LSB 6U
#define QMI_M1_XFMT_SUFFIX_WIDTH_LSB 4U
#define QMI_M1_XFMT_ADDR_WIDTH_LSB 2U
#define QMI_M1_XFMT_PREFIX_WIDTH_LSB 0U
#define QMI_M1_XFMT_WIDTH_VALUE_Q 2U

#define QMI_M1_XCMD_PREFIX_LSB 0U

#define PSRAM_DIRECT_CLKDIV 30U
#define PSRAM_TARGET_HZ 63000000U
#define PSRAM_READ_ID 0x9fU
#define PSRAM_QUAD_READ 0xebU
#define PSRAM_QUAD_WRITE 0x38U
#define PSRAM_KGD 0x5dU

#define RAMFUNC NOINLINE SECTION(".ramfunc")

extern uint32_t SystemCoreClock;

static size_t s_psram_size;

static void prv_gpio_set_function(uint8_t pin, uint32_t function) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = function;
}

static void RAMFUNC prv_wait_qmi_idle(void) {
  while (REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) & QMI_DIRECT_CSR_BUSY_BITS) {
  }
}

static void RAMFUNC prv_psram_direct_begin(void) {
  REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) =
      (PSRAM_DIRECT_CLKDIV << QMI_DIRECT_CSR_CLKDIV_LSB) | QMI_DIRECT_CSR_EN_BITS;
  prv_wait_qmi_idle();
}

static void RAMFUNC prv_psram_direct_end(void) {
  REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) &=
      ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
}

static uint32_t RAMFUNC prv_psram_clkdiv(void) {
  uint32_t clkdiv = (SystemCoreClock + (PSRAM_TARGET_HZ - 1U)) / PSRAM_TARGET_HZ;

  if (clkdiv < 2U) {
    clkdiv = 2U;
  } else if (clkdiv > 255U) {
    clkdiv = 255U;
  }

  return clkdiv;
}

static void RAMFUNC prv_psram_write_direct(uint32_t value) {
  REG32(XIP_QMI_BASE + QMI_DIRECT_TX_OFFSET) = value;
  prv_wait_qmi_idle();
}

static void RAMFUNC prv_psram_enter_quad_mode(void) {
  prv_psram_direct_begin();

  REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
  prv_psram_write_direct(QMI_DIRECT_TX_OE_BITS |
                         (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB) |
                         0xf5U);
  (void)REG32(XIP_QMI_BASE + QMI_DIRECT_RX_OFFSET);
  REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

  prv_psram_direct_end();
}

static uint8_t RAMFUNC prv_psram_read_id(uint8_t *eid) {
  uint8_t kgd = 0U;

  *eid = 0U;
  prv_psram_direct_begin();

  REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
  for (size_t i = 0; i < 12U; ++i) {
    REG32(XIP_QMI_BASE + QMI_DIRECT_TX_OFFSET) = (i == 0U) ? PSRAM_READ_ID : 0xffU;
    while (!(REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) &
             QMI_DIRECT_CSR_TXEMPTY_BITS)) {
    }
    prv_wait_qmi_idle();

    const uint8_t value = (uint8_t)REG32(XIP_QMI_BASE + QMI_DIRECT_RX_OFFSET);
    if (i == 5U) {
      kgd = value;
    } else if (i == 6U) {
      *eid = value;
    }
  }

  prv_psram_direct_end();
  return kgd;
}

static void RAMFUNC prv_psram_reset_and_enable_quad(void) {
  prv_psram_direct_begin();
  for (size_t i = 0; i < 4U; ++i) {
    uint8_t cmd;

    switch (i) {
      case 0:
        cmd = 0x66U;
        break;
      case 1:
        cmd = 0x99U;
        break;
      case 2:
        cmd = 0x35U;
        break;
      default:
        cmd = 0xc0U;
        break;
    }

    REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    prv_psram_write_direct(cmd);
    REG32(XIP_QMI_BASE + QMI_DIRECT_CSR_OFFSET) &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

    for (volatile uint32_t j = 0; j < 20U; ++j) {
      __asm__ volatile("nop");
    }
    (void)REG32(XIP_QMI_BASE + QMI_DIRECT_RX_OFFSET);
  }
  prv_psram_direct_end();
}

static void RAMFUNC prv_psram_configure_xip(void) {
  const uint32_t psram_clkdiv = prv_psram_clkdiv();
  const uint32_t q_width =
      QMI_M1_XFMT_WIDTH_VALUE_Q << QMI_M1_XFMT_PREFIX_WIDTH_LSB |
      QMI_M1_XFMT_WIDTH_VALUE_Q << QMI_M1_XFMT_ADDR_WIDTH_LSB |
      QMI_M1_XFMT_WIDTH_VALUE_Q << QMI_M1_XFMT_SUFFIX_WIDTH_LSB |
      QMI_M1_XFMT_WIDTH_VALUE_Q << QMI_M1_XFMT_DUMMY_WIDTH_LSB |
      QMI_M1_XFMT_WIDTH_VALUE_Q << QMI_M1_XFMT_DATA_WIDTH_LSB |
      QMI_M1_XFMT_PREFIX_LEN_VALUE_8 << QMI_M1_XFMT_PREFIX_LEN_LSB;

  REG32(XIP_QMI_BASE + QMI_M1_TIMING_OFFSET) =
      QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
      3U << QMI_M1_TIMING_SELECT_HOLD_LSB |
      1U << QMI_M1_TIMING_COOLDOWN_LSB |
      2U << QMI_M1_TIMING_RXDELAY_LSB |
      29U << QMI_M1_TIMING_MAX_SELECT_LSB |
      12U << QMI_M1_TIMING_MIN_DESELECT_LSB |
      psram_clkdiv << QMI_M1_TIMING_CLKDIV_LSB;

  REG32(XIP_QMI_BASE + QMI_M1_RFMT_OFFSET) =
      q_width | QMI_M1_XFMT_DUMMY_LEN_VALUE_24 << QMI_M1_XFMT_DUMMY_LEN_LSB;
  REG32(XIP_QMI_BASE + QMI_M1_RCMD_OFFSET) = PSRAM_QUAD_READ << QMI_M1_XCMD_PREFIX_LSB;

  REG32(XIP_QMI_BASE + QMI_M1_WFMT_OFFSET) =
      q_width | QMI_M1_XFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_XFMT_DUMMY_LEN_LSB;
  REG32(XIP_QMI_BASE + QMI_M1_WCMD_OFFSET) = PSRAM_QUAD_WRITE << QMI_M1_XCMD_PREFIX_LSB;
}

static size_t RAMFUNC prv_psram_size_from_eid(uint8_t eid) {
  const uint8_t size_id = eid >> 5U;

  if (eid == 0x26U || size_id == 2U) {
    return 8U * 1024U * 1024U;
  }
  if (size_id == 1U) {
    return 4U * 1024U * 1024U;
  }
  if (size_id == 0U) {
    return 2U * 1024U * 1024U;
  }

  return 1024U * 1024U;
}

static bool RAMFUNC prv_psram_probe_window(size_t psram_size) {
  volatile uint32_t *psram = (volatile uint32_t *)(uintptr_t)FRUITJAM_PSRAM_NOCACHE_BASE;
  const size_t last_word = (psram_size / sizeof(uint32_t)) - 1U;

  psram[0] = 0x12345678U;
  if (psram[0] != 0x12345678U) {
    return false;
  }

  psram[last_word] = 0xa5a55a5aU;
  if (psram[last_word] != 0xa5a55a5aU) {
    return false;
  }

  return true;
}

static size_t RAMFUNC prv_psram_init(void) {
  uint8_t eid;

  prv_psram_enter_quad_mode();
  if (prv_psram_read_id(&eid) != PSRAM_KGD) {
    return 0U;
  }

  prv_psram_reset_and_enable_quad();
  prv_psram_configure_xip();
  REG32(XIP_CTRL_BASE + XIP_CTRL_OFFSET) |= XIP_CTRL_WRITABLE_M1_BITS;

  const size_t psram_size = prv_psram_size_from_eid(eid);
  if (!prv_psram_probe_window(psram_size)) {
    return 0U;
  }

  return psram_size;
}

void fruitjam_psram_init(void) {
  if (s_psram_size) {
    return;
  }

  prv_gpio_set_function(FRUITJAM_PIN_PSRAM_CS, GPIO_FUNC_XIP_CS1);

  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  __DSB();
  __ISB();

  s_psram_size = prv_psram_init();

  __DSB();
  __ISB();
  __set_PRIMASK(primask);
}

size_t fruitjam_psram_get_size(void) {
  return s_psram_size;
}

bool fruitjam_psram_is_available(void) {
  return s_psram_size >= FRUITJAM_PSRAM_EXPECTED_SIZE;
}
