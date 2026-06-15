/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __CM33_REV 0x0100U
#define __NVIC_PRIO_BITS 4U
#define __Vendor_SysTickConfig 0U
#define __MPU_PRESENT 1U
#define __VTOR_PRESENT 1U
#define __FPU_PRESENT 1U
#define __FPU_DP 0U
#define __DSP_PRESENT 1U
#define __SAUREGION_PRESENT 1U

typedef enum IRQn {
  NonMaskableInt_IRQn = -14,
  HardFault_IRQn = -13,
  MemoryManagement_IRQn = -12,
  BusFault_IRQn = -11,
  UsageFault_IRQn = -10,
  SecureFault_IRQn = -9,
  SVCall_IRQn = -5,
  DebugMonitor_IRQn = -4,
  PendSV_IRQn = -2,
  SysTick_IRQn = -1,

  TIMER0_IRQ_0_IRQn = 0,
  TIMER0_IRQ_1_IRQn = 1,
  TIMER0_IRQ_2_IRQn = 2,
  TIMER0_IRQ_3_IRQn = 3,
  TIMER1_IRQ_0_IRQn = 4,
  TIMER1_IRQ_1_IRQn = 5,
  TIMER1_IRQ_2_IRQn = 6,
  TIMER1_IRQ_3_IRQn = 7,
  PWM_IRQ_WRAP_0_IRQn = 8,
  PWM_IRQ_WRAP_1_IRQn = 9,
  DMA_IRQ_0_IRQn = 10,
  DMA_IRQ_1_IRQn = 11,
  DMA_IRQ_2_IRQn = 12,
  DMA_IRQ_3_IRQn = 13,
  USBCTRL_IRQ_IRQn = 14,
  PIO0_IRQ_0_IRQn = 15,
  PIO0_IRQ_1_IRQn = 16,
  PIO1_IRQ_0_IRQn = 17,
  PIO1_IRQ_1_IRQn = 18,
  PIO2_IRQ_0_IRQn = 19,
  PIO2_IRQ_1_IRQn = 20,
  IO_IRQ_BANK0_IRQn = 21,
  IO_IRQ_BANK0_NS_IRQn = 22,
  IO_IRQ_QSPI_IRQn = 23,
  IO_IRQ_QSPI_NS_IRQn = 24,
  SIO_IRQ_FIFO_IRQn = 25,
  SIO_IRQ_BELL_IRQn = 26,
  SIO_IRQ_FIFO_NS_IRQn = 27,
  SIO_IRQ_BELL_NS_IRQn = 28,
  SIO_IRQ_MTIMECMP_IRQn = 29,
  CLOCKS_IRQ_IRQn = 30,
  SPI0_IRQ_IRQn = 31,
  SPI1_IRQ_IRQn = 32,
  UART0_IRQ_IRQn = 33,
  UART1_IRQ_IRQn = 34,
  ADC_IRQ_FIFO_IRQn = 35,
  I2C0_IRQ_IRQn = 36,
  I2C1_IRQ_IRQn = 37,
  OTP_IRQ_IRQn = 38,
  TRNG_IRQ_IRQn = 39,
  PLL_SYS_IRQ_IRQn = 42,
  PLL_USB_IRQ_IRQn = 43,
  POWMAN_IRQ_POW_IRQn = 44,
  POWMAN_IRQ_TIMER_IRQn = 45,
} IRQn_Type;

#include "core_cm33.h"

#ifndef MPU_RASR_TEX_Pos
#define MPU_RASR_TEX_Pos MPU_RLAR_AttrIndx_Pos
#define MPU_RASR_S_Msk 0
#define MPU_RASR_C_Msk 0
#define MPU_RASR_B_Msk 0
#define RASR RLAR
#endif

#ifndef SCB_CCR_STKALIGN_Msk
#define SCB_CCR_STKALIGN_Msk (1UL << 9)
#endif

#ifndef SRAM_BASE
#define SRAM_BASE 0x20000000UL
#endif

extern uint32_t SystemCoreClock;

#ifdef __cplusplus
}
#endif
