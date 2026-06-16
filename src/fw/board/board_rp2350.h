/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"
#include "soc/rp2350/rp2350/rp2350_external_rtc.h"

#include <cmsis_core.h>
#include <stdbool.h>
#include <stdint.h>

#include "drivers/button_id.h"

#define IRQ_PRIORITY_INVALID (1 << __NVIC_PRIO_BITS)

enum {
#define IRQ_DEF(num, irq) IS_VALID_IRQ__##irq,
#include "irq_rp2350.def"
#undef IRQ_DEF
};

#define IRQ_MAP(irq, handler, device) \
  void irq##_IRQHandler(void) {       \
    handler(device);                  \
  }                                   \
  _Static_assert(IS_VALID_IRQ__##irq || true, "(See comment below)")

typedef void GPIO_TypeDef;

#define GPIO_Port_NULL NULL
#define GPIO_Pin_NULL 0xffU
#define RP2350_GPIO_RESOURCE_EXISTS ((void *)1)

typedef enum {
  GPIO_OType_PP,
  GPIO_OType_OD,
} GPIOOType_TypeDef;

typedef enum {
  GPIO_PuPd_NOPULL,
  GPIO_PuPd_UP,
  GPIO_PuPd_DOWN,
} GPIOPuPd_TypeDef;

typedef enum { GPIO_Speed_2MHz, GPIO_Speed_50MHz, GPIO_Speed_200MHz } GPIOSpeed_TypeDef;

typedef struct {
  void *const peripheral;
  const uint32_t gpio_pin;
  GPIOPuPd_TypeDef pull;
} ExtiConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
} InputConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
  bool active_high;
} OutputConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
} AfConfig;

typedef struct {
  uint8_t gpio_pin;
  uint8_t function;
} Pinmux;

typedef struct {
  uint16_t value;
  uint16_t resolution;
  int enabled;
  uint16_t channel;
} PwmState;

typedef struct {
  Pinmux pwm_pin;
  PwmState *state;
} PwmConfig;

typedef struct {
  uint8_t timer_index;
} TimerConfig;

typedef enum {
  ActuatorOptions_Ctl = 1 << 0,
  ActuatorOptions_Pwm = 1 << 1,
  ActuatorOptions_HBridge = 1 << 3,
} ActuatorOptions;

typedef struct {
  const ActuatorOptions options;
  const OutputConfig ctl;
  const PwmConfig pwm;
} BoardConfigActuator;

typedef struct {
  uint8_t backlight_on_percent;
  uint32_t ambient_light_dark_threshold;
  uint32_t ambient_k_delta_threshold;
  ExtiConfig dbgserial_int;
  InputConfig dbgserial_int_gpio;
} BoardConfig;

typedef struct {
  const char *name;
  void *const port;
  uint8_t pin;
  GPIOPuPd_TypeDef pull;
  bool active_high;
} ButtonConfig;

typedef struct {
  ButtonConfig buttons[NUM_BUTTONS];
  void *timer;
  int timer_irqn;
} BoardConfigButton;

typedef struct {
  ExtiConfig pmic_int;
  const uint8_t low_power_threshold;
  const uint16_t battery_capacity_hours;
  //! Board is powered from a fixed supply and has no battery policy.
  const bool fixed_power;
} BoardConfigPower;

typedef enum {
  AccelThresholdLow,
  AccelThresholdHigh,
  AccelThreshold_Num,
} AccelThreshold;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
  uint32_t shake_thresholds[AccelThreshold_Num];
  uint32_t double_tap_threshold;
  uint8_t tap_shock;
  uint8_t tap_quiet;
  uint8_t tap_dur;
  uint8_t default_motion_sensitivity;
} AccelConfig;

typedef struct {
  const AccelConfig accel_config;
  const InputConfig accel_int_gpios[2];
  const ExtiConfig accel_ints[2];
} BoardConfigAccel;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
} MagConfig;

typedef struct {
  const MagConfig mag_config;
} BoardConfigMag;

typedef enum { SpiPeriphClockRp2350 } SpiPeriphClock;

typedef const struct UARTDevice UARTDevice;
typedef const struct DMARequest DMARequest;
typedef const struct SPIBus SPIBus;
typedef const struct SPISlavePort SPISlavePort;
typedef const struct I2CBus I2CBus;
typedef const struct I2CSlavePort I2CSlavePort;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct DisplayDevice DisplayDevice;
typedef const struct AudioDevice AudioDevice;

extern const Rp2350ExternalRtcConfig *const BOARD_CONFIG_EXTERNAL_RTC;

void board_early_init(void);
void board_init(void);

#include <stdio.h>
#define sniprintf snprintf

#include "board_definitions.h"
