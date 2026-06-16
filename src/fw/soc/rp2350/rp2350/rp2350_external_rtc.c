/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/rp2350_external_rtc.h"

#include "hardware/timer.h"

#include <string.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define SIO_BASE 0xd0000000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define GPIO_FUNC_SIO 5U
#define GPIO_PAD_NORMAL 0x52U
#define GPIO_PAD_INPUT_PULLUP 0x4aU

#define SIO_GPIO_IN_OFFSET 0x04U
#define SIO_GPIO_HI_IN_OFFSET 0x08U
#define SIO_GPIO_OUT_CLR_OFFSET 0x20U
#define SIO_GPIO_HI_OUT_CLR_OFFSET 0x24U
#define SIO_GPIO_OE_SET_OFFSET 0x38U
#define SIO_GPIO_HI_OE_SET_OFFSET 0x3cU
#define SIO_GPIO_OE_CLR_OFFSET 0x40U
#define SIO_GPIO_HI_OE_CLR_OFFSET 0x44U

#define I2C_HALF_PERIOD_US 25U
#define I2C_STRETCH_TIMEOUT_US 100U
#define DS1307_NVRAM_BEGIN 0x08U
#define DS1307_NVRAM_SIZE 56U
#define DS1307_TIMEZONE_MAGIC_0 'P'
#define DS1307_TIMEZONE_MAGIC_1 'T'
#define DS1307_TIMEZONE_MAGIC_2 'Z'
#define DS1307_TIMEZONE_VERSION 1U
#define DS1307_TIMEZONE_HEADER_SIZE 5U
#define DS1307_TIMEZONE_RECORD_SIZE \
  (DS1307_TIMEZONE_HEADER_SIZE + sizeof(TimezoneInfo) + sizeof(uint8_t))

typedef enum {
  Rp2350ExternalRtcErrorNone = 0,
  Rp2350ExternalRtcErrorNotInitialized = -1,
  Rp2350ExternalRtcErrorSclStuckLow = -2,
  Rp2350ExternalRtcErrorAddressNack = -3,
  Rp2350ExternalRtcErrorRegisterNack = -4,
  Rp2350ExternalRtcErrorInvalidTime = -5,
  Rp2350ExternalRtcErrorWriteNack = -6,
  Rp2350ExternalRtcErrorUnsupportedType = -7,
  Rp2350ExternalRtcErrorInvalidConfig = -8,
  Rp2350ExternalRtcErrorInvalidTimezone = -9,
} Rp2350ExternalRtcError;

_Static_assert(DS1307_TIMEZONE_RECORD_SIZE <= DS1307_NVRAM_SIZE,
               "DS1307 timezone record must fit in NVRAM");

static Rp2350ExternalRtcConfig s_config;
static bool s_initialized;
static bool s_present;
static bool s_time_valid;
static int32_t s_last_error = Rp2350ExternalRtcErrorNotInitialized;
static uint32_t s_read_success_count;
static uint32_t s_read_failure_count;
static uint32_t s_write_success_count;
static uint32_t s_write_failure_count;
static uint32_t s_timezone_read_success_count;
static uint32_t s_timezone_read_failure_count;
static uint32_t s_timezone_write_success_count;
static uint32_t s_timezone_write_failure_count;

static void prv_delay_us(uint32_t us) {
  const uint32_t start = time_us_32();
  while ((uint32_t)(time_us_32() - start) < us) {
  }
}

static bool prv_gpio_get(uint8_t pin) {
  if (pin < 32U) {
    return (REG32(SIO_BASE + SIO_GPIO_IN_OFFSET) & (1U << pin)) != 0U;
  }

  return (REG32(SIO_BASE + SIO_GPIO_HI_IN_OFFSET) & (1U << (pin - 32U))) != 0U;
}

static void prv_gpio_output_low(uint8_t pin) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = GPIO_FUNC_SIO;
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OUT_CLR_OFFSET) = 1U << pin;
    REG32(SIO_BASE + SIO_GPIO_OE_SET_OFFSET) = 1U << pin;
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OUT_CLR_OFFSET) = 1U << (pin - 32U);
    REG32(SIO_BASE + SIO_GPIO_HI_OE_SET_OFFSET) = 1U << (pin - 32U);
  }
}

static void prv_gpio_release(uint8_t pin) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_INPUT_PULLUP;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = GPIO_FUNC_SIO;
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_CLR_OFFSET) = 1U << pin;
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_CLR_OFFSET) = 1U << (pin - 32U);
  }
}

static void prv_sda_low(void) {
  prv_gpio_output_low(s_config.sda_gpio);
}

static void prv_sda_release(void) {
  prv_gpio_release(s_config.sda_gpio);
}

static void prv_scl_low(void) {
  prv_gpio_output_low(s_config.scl_gpio);
}

static bool prv_scl_release_wait(void) {
  prv_gpio_release(s_config.scl_gpio);
  const uint32_t start = time_us_32();
  while (!prv_gpio_get(s_config.scl_gpio)) {
    if ((uint32_t)(time_us_32() - start) >= I2C_STRETCH_TIMEOUT_US) {
      s_last_error = Rp2350ExternalRtcErrorSclStuckLow;
      return false;
    }
  }

  return true;
}

static bool prv_i2c_start(void) {
  prv_sda_release();
  if (!prv_scl_release_wait()) {
    return false;
  }
  prv_delay_us(I2C_HALF_PERIOD_US);
  prv_sda_low();
  prv_delay_us(I2C_HALF_PERIOD_US);
  prv_scl_low();
  return true;
}

static void prv_i2c_stop(void) {
  prv_sda_low();
  prv_delay_us(I2C_HALF_PERIOD_US);
  (void)prv_scl_release_wait();
  prv_delay_us(I2C_HALF_PERIOD_US);
  prv_sda_release();
  prv_delay_us(I2C_HALF_PERIOD_US);
}

static bool prv_i2c_write_bit(bool high) {
  if (high) {
    prv_sda_release();
  } else {
    prv_sda_low();
  }

  prv_delay_us(I2C_HALF_PERIOD_US);
  if (!prv_scl_release_wait()) {
    return false;
  }
  prv_delay_us(I2C_HALF_PERIOD_US);
  prv_scl_low();
  return true;
}

static bool prv_i2c_read_bit(bool *out_high) {
  prv_sda_release();
  prv_delay_us(I2C_HALF_PERIOD_US);
  if (!prv_scl_release_wait()) {
    return false;
  }
  prv_delay_us(I2C_HALF_PERIOD_US);
  *out_high = prv_gpio_get(s_config.sda_gpio);
  prv_scl_low();
  return true;
}

static bool prv_i2c_write_byte(uint8_t byte) {
  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
    if (!prv_i2c_write_bit((byte & mask) != 0U)) {
      return false;
    }
  }

  bool ack_high = true;
  if (!prv_i2c_read_bit(&ack_high)) {
    return false;
  }
  return !ack_high;
}

static bool prv_i2c_read_byte(uint8_t *out_byte, bool ack) {
  uint8_t value = 0;
  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
    bool bit_high = false;
    if (!prv_i2c_read_bit(&bit_high)) {
      return false;
    }
    if (bit_high) {
      value |= mask;
    }
  }

  if (!prv_i2c_write_bit(!ack)) {
    return false;
  }
  *out_byte = value;
  return true;
}

static void prv_recover_bus(void) {
  prv_sda_release();
  for (uint8_t i = 0; i < 9U; ++i) {
    prv_scl_low();
    prv_delay_us(I2C_HALF_PERIOD_US);
    (void)prv_scl_release_wait();
    prv_delay_us(I2C_HALF_PERIOD_US);
  }
  prv_i2c_stop();
}

static bool prv_is_ds1307_compatible(void) {
  if (s_config.type == Rp2350ExternalRtcTypeDs1307) {
    return true;
  }

  s_last_error = Rp2350ExternalRtcErrorUnsupportedType;
  return false;
}

static bool prv_read_registers(uint8_t register_address, uint8_t *data, uint32_t length) {
  if (!s_initialized) {
    s_last_error = Rp2350ExternalRtcErrorNotInitialized;
    return false;
  }

  bool ok = false;
  if (!prv_i2c_start()) {
    goto done;
  }
  if (!prv_i2c_write_byte((uint8_t)(s_config.i2c_address << 1U))) {
    s_last_error = Rp2350ExternalRtcErrorAddressNack;
    goto done;
  }
  if (!prv_i2c_write_byte(register_address)) {
    s_last_error = Rp2350ExternalRtcErrorRegisterNack;
    goto done;
  }
  if (!prv_i2c_start()) {
    goto done;
  }
  if (!prv_i2c_write_byte((uint8_t)((s_config.i2c_address << 1U) | 1U))) {
    s_last_error = Rp2350ExternalRtcErrorAddressNack;
    goto done;
  }

  for (uint32_t i = 0; i < length; ++i) {
    if (!prv_i2c_read_byte(&data[i], i + 1U != length)) {
      goto done;
    }
  }
  ok = true;

done:
  prv_i2c_stop();
  return ok;
}

static bool prv_write_registers(uint8_t register_address, const uint8_t *data, uint32_t length) {
  if (!s_initialized) {
    s_last_error = Rp2350ExternalRtcErrorNotInitialized;
    return false;
  }

  bool ok = false;
  if (!prv_i2c_start()) {
    goto done;
  }
  if (!prv_i2c_write_byte((uint8_t)(s_config.i2c_address << 1U))) {
    s_last_error = Rp2350ExternalRtcErrorAddressNack;
    goto done;
  }
  if (!prv_i2c_write_byte(register_address)) {
    s_last_error = Rp2350ExternalRtcErrorRegisterNack;
    goto done;
  }
  for (uint32_t i = 0; i < length; ++i) {
    if (!prv_i2c_write_byte(data[i])) {
      s_last_error = Rp2350ExternalRtcErrorWriteNack;
      goto done;
    }
  }
  ok = true;

done:
  prv_i2c_stop();
  return ok;
}

static uint8_t prv_bcd_to_bin(uint8_t value) {
  return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0fU));
}

static uint8_t prv_bin_to_bcd(uint8_t value) {
  return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static bool prv_bcd_is_valid(uint8_t value, uint8_t min, uint8_t max) {
  if ((value & 0x0fU) > 9U || ((value >> 4U) & 0x0fU) > 9U) {
    return false;
  }

  const uint8_t decoded = prv_bcd_to_bin(value);
  return decoded >= min && decoded <= max;
}

static uint8_t prv_checksum(const uint8_t *data, uint32_t length) {
  uint8_t checksum = 0xa5U;
  for (uint32_t i = 0; i < length; ++i) {
    checksum = (uint8_t)((checksum << 1U) | (checksum >> 7U));
    checksum ^= data[i];
  }
  return checksum;
}

static bool prv_decode_time(const uint8_t registers[7], time_t *out_time) {
  if ((registers[0] & 0x80U) != 0U) {
    return false;
  }
  if (!prv_bcd_is_valid(registers[0] & 0x7fU, 0U, 59U) ||
      !prv_bcd_is_valid(registers[1] & 0x7fU, 0U, 59U) ||
      !prv_bcd_is_valid(registers[4] & 0x3fU, 1U, 31U) ||
      !prv_bcd_is_valid(registers[5] & 0x1fU, 1U, 12U) ||
      !prv_bcd_is_valid(registers[6], 0U, 99U)) {
    return false;
  }

  uint8_t hour = 0;
  if ((registers[2] & 0x40U) != 0U) {
    if (!prv_bcd_is_valid(registers[2] & 0x1fU, 1U, 12U)) {
      return false;
    }
    hour = prv_bcd_to_bin(registers[2] & 0x1fU);
    if ((registers[2] & 0x20U) != 0U && hour < 12U) {
      hour += 12U;
    } else if ((registers[2] & 0x20U) == 0U && hour == 12U) {
      hour = 0U;
    }
  } else {
    if (!prv_bcd_is_valid(registers[2] & 0x3fU, 0U, 23U)) {
      return false;
    }
    hour = prv_bcd_to_bin(registers[2] & 0x3fU);
  }

  const uint8_t year = prv_bcd_to_bin(registers[6]);
  if (year > 37U) {
    return false;
  }

  struct tm time_tm = {
      .tm_sec = prv_bcd_to_bin(registers[0] & 0x7fU),
      .tm_min = prv_bcd_to_bin(registers[1] & 0x7fU),
      .tm_hour = hour,
      .tm_mday = prv_bcd_to_bin(registers[4] & 0x3fU),
      .tm_mon = prv_bcd_to_bin(registers[5] & 0x1fU) - 1,
      .tm_year = 100 + year,
      .tm_isdst = 0,
  };

  *out_time = mktime(&time_tm);
  return *out_time != (time_t)-1;
}

bool rp2350_external_rtc_init(const Rp2350ExternalRtcConfig *config) {
  if (!config || config->type == Rp2350ExternalRtcTypeNone || config->i2c_address < 0x08U ||
      config->i2c_address >= 0x78U) {
    s_initialized = false;
    s_present = false;
    s_time_valid = false;
    s_last_error = Rp2350ExternalRtcErrorInvalidConfig;
    return false;
  }

  s_config = *config;
  s_initialized = true;
  s_present = false;
  s_time_valid = false;
  s_last_error = Rp2350ExternalRtcErrorNone;

  prv_gpio_release(s_config.sda_gpio);
  prv_gpio_release(s_config.scl_gpio);
  prv_recover_bus();
  return true;
}

bool rp2350_external_rtc_read_timezone(TimezoneInfo *out_timezone) {
  if (!out_timezone || !prv_is_ds1307_compatible()) {
    ++s_timezone_read_failure_count;
    return false;
  }

  uint8_t record[DS1307_TIMEZONE_RECORD_SIZE];
  if (!prv_read_registers(DS1307_NVRAM_BEGIN, record, sizeof(record))) {
    ++s_timezone_read_failure_count;
    return false;
  }

  s_present = true;
  const bool header_ok = record[0] == DS1307_TIMEZONE_MAGIC_0 &&
                         record[1] == DS1307_TIMEZONE_MAGIC_1 &&
                         record[2] == DS1307_TIMEZONE_MAGIC_2 &&
                         record[3] == DS1307_TIMEZONE_VERSION &&
                         record[4] == sizeof(TimezoneInfo);
  const uint8_t expected_checksum =
      prv_checksum(record, (uint32_t)(sizeof(record) - sizeof(uint8_t)));
  if (!header_ok || record[sizeof(record) - 1U] != expected_checksum) {
    s_last_error = Rp2350ExternalRtcErrorInvalidTimezone;
    ++s_timezone_read_failure_count;
    return false;
  }

  memcpy(out_timezone, &record[DS1307_TIMEZONE_HEADER_SIZE], sizeof(*out_timezone));
  s_last_error = Rp2350ExternalRtcErrorNone;
  ++s_timezone_read_success_count;
  return true;
}

bool rp2350_external_rtc_write_timezone(const TimezoneInfo *timezone) {
  if (!timezone || !prv_is_ds1307_compatible()) {
    ++s_timezone_write_failure_count;
    return false;
  }

  uint8_t record[DS1307_TIMEZONE_RECORD_SIZE] = {
      DS1307_TIMEZONE_MAGIC_0,
      DS1307_TIMEZONE_MAGIC_1,
      DS1307_TIMEZONE_MAGIC_2,
      DS1307_TIMEZONE_VERSION,
      sizeof(TimezoneInfo),
  };
  memcpy(&record[DS1307_TIMEZONE_HEADER_SIZE], timezone, sizeof(*timezone));
  record[sizeof(record) - 1U] =
      prv_checksum(record, (uint32_t)(sizeof(record) - sizeof(uint8_t)));

  if (!prv_write_registers(DS1307_NVRAM_BEGIN, record, sizeof(record))) {
    ++s_timezone_write_failure_count;
    return false;
  }

  s_present = true;
  s_last_error = Rp2350ExternalRtcErrorNone;
  ++s_timezone_write_success_count;
  return true;
}

bool rp2350_external_rtc_clear_timezone(void) {
  if (!prv_is_ds1307_compatible()) {
    ++s_timezone_write_failure_count;
    return false;
  }

  const uint8_t empty[DS1307_TIMEZONE_RECORD_SIZE] = {0};
  if (!prv_write_registers(DS1307_NVRAM_BEGIN, empty, sizeof(empty))) {
    ++s_timezone_write_failure_count;
    return false;
  }

  s_present = true;
  s_last_error = Rp2350ExternalRtcErrorNone;
  ++s_timezone_write_success_count;
  return true;
}

uint32_t rp2350_external_rtc_scan(uint8_t *addresses, uint32_t address_capacity) {
  if (!s_initialized) {
    s_last_error = Rp2350ExternalRtcErrorNotInitialized;
    return 0;
  }

  uint32_t found_count = 0;
  bool bus_error = false;
  prv_gpio_release(s_config.sda_gpio);
  prv_gpio_release(s_config.scl_gpio);
  prv_recover_bus();

  for (uint8_t address = 0x08U; address < 0x78U; ++address) {
    bool ack = false;
    if (prv_i2c_start()) {
      ack = prv_i2c_write_byte((uint8_t)(address << 1U));
    } else {
      bus_error = true;
    }
    prv_i2c_stop();

    if (ack) {
      if (found_count < address_capacity) {
        addresses[found_count] = address;
      }
      ++found_count;
    }
  }

  if (!bus_error) {
    s_last_error = Rp2350ExternalRtcErrorNone;
  }
  return found_count;
}

bool rp2350_external_rtc_read_time(time_t *out_time) {
  if (!prv_is_ds1307_compatible()) {
    s_present = false;
    s_time_valid = false;
    ++s_read_failure_count;
    return false;
  }

  uint8_t registers[7];
  if (!prv_read_registers(0x00U, registers, sizeof(registers))) {
    s_present = false;
    s_time_valid = false;
    ++s_read_failure_count;
    return false;
  }

  s_present = true;
  if (!prv_decode_time(registers, out_time)) {
    s_time_valid = false;
    s_last_error = Rp2350ExternalRtcErrorInvalidTime;
    ++s_read_failure_count;
    return false;
  }

  s_time_valid = true;
  s_last_error = Rp2350ExternalRtcErrorNone;
  ++s_read_success_count;
  return true;
}

bool rp2350_external_rtc_write_time(time_t time) {
  if (!prv_is_ds1307_compatible()) {
    s_present = false;
    s_time_valid = false;
    ++s_write_failure_count;
    return false;
  }

  struct tm time_tm;
  gmtime_r(&time, &time_tm);
  if (time_tm.tm_year < 100 || time_tm.tm_year > 137) {
    s_last_error = Rp2350ExternalRtcErrorInvalidTime;
    ++s_write_failure_count;
    return false;
  }

  const uint8_t registers[] = {
      prv_bin_to_bcd((uint8_t)time_tm.tm_sec),
      prv_bin_to_bcd((uint8_t)time_tm.tm_min),
      prv_bin_to_bcd((uint8_t)time_tm.tm_hour),
      prv_bin_to_bcd((uint8_t)(time_tm.tm_wday + 1)),
      prv_bin_to_bcd((uint8_t)time_tm.tm_mday),
      prv_bin_to_bcd((uint8_t)(time_tm.tm_mon + 1)),
      prv_bin_to_bcd((uint8_t)(time_tm.tm_year - 100)),
  };

  if (!prv_write_registers(0x00U, registers, sizeof(registers))) {
    s_present = false;
    s_time_valid = false;
    ++s_write_failure_count;
    return false;
  }

  s_present = true;
  s_time_valid = true;
  s_last_error = Rp2350ExternalRtcErrorNone;
  ++s_write_success_count;
  return true;
}

bool rp2350_external_rtc_is_present(void) {
  return s_present;
}

bool rp2350_external_rtc_has_valid_time(void) {
  return s_time_valid;
}

int32_t rp2350_external_rtc_last_error(void) {
  return s_last_error;
}

uint32_t rp2350_external_rtc_read_success_count(void) {
  return s_read_success_count;
}

uint32_t rp2350_external_rtc_read_failure_count(void) {
  return s_read_failure_count;
}

uint32_t rp2350_external_rtc_write_success_count(void) {
  return s_write_success_count;
}

uint32_t rp2350_external_rtc_write_failure_count(void) {
  return s_write_failure_count;
}

uint32_t rp2350_external_rtc_timezone_read_success_count(void) {
  return s_timezone_read_success_count;
}

uint32_t rp2350_external_rtc_timezone_read_failure_count(void) {
  return s_timezone_read_failure_count;
}

uint32_t rp2350_external_rtc_timezone_write_success_count(void) {
  return s_timezone_write_success_count;
}

uint32_t rp2350_external_rtc_timezone_write_failure_count(void) {
  return s_timezone_write_failure_count;
}
