/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"

#include "board/board.h"
#include "system/passert.h"

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define SIO_BASE 0xd0000000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define GPIO_FUNC_SIO 5U
#define GPIO_FUNC_NULL 0x1fU

#define GPIO_PIN_COUNT 48U

#define PADS_BANK0_GPIO_OD_BITS (1U << 7)
#define PADS_BANK0_GPIO_IE_BITS (1U << 6)
#define PADS_BANK0_GPIO_DRIVE_4MA_BITS (1U << 4)
#define PADS_BANK0_GPIO_PUE_BITS (1U << 3)
#define PADS_BANK0_GPIO_PDE_BITS (1U << 2)
#define PADS_BANK0_GPIO_SCHMITT_BITS (1U << 1)

#define GPIO_PAD_NORMAL \
  (PADS_BANK0_GPIO_IE_BITS | PADS_BANK0_GPIO_DRIVE_4MA_BITS | PADS_BANK0_GPIO_SCHMITT_BITS)

#define SIO_GPIO_IN_OFFSET 0x04U
#define SIO_GPIO_HI_IN_OFFSET 0x08U
#define SIO_GPIO_OUT_SET_OFFSET 0x18U
#define SIO_GPIO_HI_OUT_SET_OFFSET 0x1cU
#define SIO_GPIO_OUT_CLR_OFFSET 0x20U
#define SIO_GPIO_HI_OUT_CLR_OFFSET 0x24U
#define SIO_GPIO_OE_SET_OFFSET 0x38U
#define SIO_GPIO_HI_OE_SET_OFFSET 0x3cU
#define SIO_GPIO_OE_CLR_OFFSET 0x40U
#define SIO_GPIO_HI_OE_CLR_OFFSET 0x44U

static bool prv_pin_config_is_valid(const void *gpio, uint8_t pin) {
  return gpio != GPIO_Port_NULL && pin != GPIO_Pin_NULL && pin < GPIO_PIN_COUNT;
}

static uint32_t prv_pin_bit(uint8_t pin) {
  return 1UL << (pin & 31U);
}

static uint32_t prv_gpio_pad_config(GPIOPuPd_TypeDef pupd, GPIOOType_TypeDef otype) {
  uint32_t config = GPIO_PAD_NORMAL;

  if (pupd == GPIO_PuPd_UP) {
    config |= PADS_BANK0_GPIO_PUE_BITS;
  } else if (pupd == GPIO_PuPd_DOWN) {
    config |= PADS_BANK0_GPIO_PDE_BITS;
  } else if (pupd != GPIO_PuPd_NOPULL) {
    WTF;
  }

  if (otype == GPIO_OType_OD) {
    config |= PADS_BANK0_GPIO_OD_BITS;
  } else if (otype != GPIO_OType_PP) {
    WTF;
  }

  return config;
}

static void prv_gpio_set_function(uint8_t pin, uint8_t function) {
  REG32(IO_BANK0_GPIO_CTRL(pin)) = function;
}

static void prv_gpio_set_pad(uint8_t pin, uint32_t pad_config) {
  REG32(PADS_BANK0_GPIO(pin)) = pad_config;
}

static void prv_gpio_put(uint8_t pin, bool value) {
  if (pin < 32U) {
    REG32(SIO_BASE + (value ? SIO_GPIO_OUT_SET_OFFSET : SIO_GPIO_OUT_CLR_OFFSET)) =
        prv_pin_bit(pin);
  } else {
    REG32(SIO_BASE + (value ? SIO_GPIO_HI_OUT_SET_OFFSET : SIO_GPIO_HI_OUT_CLR_OFFSET)) =
        prv_pin_bit(pin);
  }
}

static bool prv_gpio_get(uint8_t pin) {
  if (pin < 32U) {
    return (REG32(SIO_BASE + SIO_GPIO_IN_OFFSET) & prv_pin_bit(pin)) != 0U;
  }
  return (REG32(SIO_BASE + SIO_GPIO_HI_IN_OFFSET) & prv_pin_bit(pin)) != 0U;
}

static void prv_gpio_output_enable(uint8_t pin) {
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_SET_OFFSET) = prv_pin_bit(pin);
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_SET_OFFSET) = prv_pin_bit(pin);
  }
}

static void prv_gpio_output_disable(uint8_t pin) {
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_CLR_OFFSET) = prv_pin_bit(pin);
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_CLR_OFFSET) = prv_pin_bit(pin);
  }
}

void gpio_use(GPIO_TypeDef *GPIOx) {
  (void)GPIOx;
}

void gpio_release(GPIO_TypeDef *GPIOx) {
  (void)GPIOx;
}

void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed) {
  (void)speed;

  if (pin_config == NULL ||
      !prv_pin_config_is_valid(pin_config->gpio, pin_config->gpio_pin)) {
    return;
  }

  prv_gpio_output_disable(pin_config->gpio_pin);
  prv_gpio_put(pin_config->gpio_pin, !pin_config->active_high);
  prv_gpio_set_pad(pin_config->gpio_pin, prv_gpio_pad_config(GPIO_PuPd_NOPULL, otype));
  prv_gpio_set_function(pin_config->gpio_pin, GPIO_FUNC_SIO);
  prv_gpio_output_enable(pin_config->gpio_pin);
}

void gpio_output_set(const OutputConfig *pin_config, bool asserted) {
  if (pin_config == NULL ||
      !prv_pin_config_is_valid(pin_config->gpio, pin_config->gpio_pin)) {
    return;
  }

  const bool drive_high = pin_config->active_high ? asserted : !asserted;
  prv_gpio_put(pin_config->gpio_pin, drive_high);
}

void gpio_af_init(const AfConfig *af_config, GPIOOType_TypeDef otype,
                  GPIOSpeed_TypeDef speed, GPIOPuPd_TypeDef pupd) {
  (void)speed;

  if (af_config == NULL || !prv_pin_config_is_valid(af_config->gpio, af_config->gpio_pin)) {
    return;
  }

  prv_gpio_set_pad(af_config->gpio_pin, prv_gpio_pad_config(pupd, otype));
}

void gpio_af_configure_low_power(const AfConfig *af_config) {
  if (af_config == NULL || !prv_pin_config_is_valid(af_config->gpio, af_config->gpio_pin)) {
    return;
  }

  prv_gpio_output_disable(af_config->gpio_pin);
  prv_gpio_set_pad(af_config->gpio_pin, 0);
  prv_gpio_set_function(af_config->gpio_pin, GPIO_FUNC_NULL);
}

void gpio_af_configure_fixed_output(const AfConfig *af_config, bool asserted) {
  if (af_config == NULL || !prv_pin_config_is_valid(af_config->gpio, af_config->gpio_pin)) {
    return;
  }

  prv_gpio_output_disable(af_config->gpio_pin);
  prv_gpio_put(af_config->gpio_pin, asserted);
  prv_gpio_set_pad(af_config->gpio_pin, prv_gpio_pad_config(GPIO_PuPd_NOPULL, GPIO_OType_PP));
  prv_gpio_set_function(af_config->gpio_pin, GPIO_FUNC_SIO);
  prv_gpio_output_enable(af_config->gpio_pin);
}

void gpio_init_all(void) {
}

void gpio_input_init(const InputConfig *input_cfg) {
  gpio_input_init_pull_up_down(input_cfg, GPIO_PuPd_NOPULL);
}

void gpio_input_init_pull_up_down(const InputConfig *input_cfg, GPIOPuPd_TypeDef pupd) {
  if (input_cfg == NULL || !prv_pin_config_is_valid(input_cfg->gpio, input_cfg->gpio_pin)) {
    return;
  }

  prv_gpio_output_disable(input_cfg->gpio_pin);
  prv_gpio_set_pad(input_cfg->gpio_pin, prv_gpio_pad_config(pupd, GPIO_OType_PP));
  prv_gpio_set_function(input_cfg->gpio_pin, GPIO_FUNC_SIO);
}

bool gpio_input_read(const InputConfig *input_cfg) {
  if (input_cfg == NULL || !prv_pin_config_is_valid(input_cfg->gpio, input_cfg->gpio_pin)) {
    return false;
  }

  return prv_gpio_get(input_cfg->gpio_pin);
}

void gpio_analog_init(const InputConfig *input_cfg) {
  if (input_cfg == NULL || !prv_pin_config_is_valid(input_cfg->gpio, input_cfg->gpio_pin)) {
    return;
  }

  prv_gpio_output_disable(input_cfg->gpio_pin);
  prv_gpio_set_pad(input_cfg->gpio_pin, 0);
  prv_gpio_set_function(input_cfg->gpio_pin, GPIO_FUNC_NULL);
}
