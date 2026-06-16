/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/time/time.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  Rp2350ExternalRtcTypeNone = 0,
  Rp2350ExternalRtcTypeDs1307,
} Rp2350ExternalRtcType;

#define RP2350_EXTERNAL_RTC_ADDRESS_DS1307 0x68U

typedef struct {
  Rp2350ExternalRtcType type;
  uint8_t i2c_address;
  uint8_t sda_gpio;
  uint8_t scl_gpio;
} Rp2350ExternalRtcConfig;

bool rp2350_external_rtc_init(const Rp2350ExternalRtcConfig *config);
uint32_t rp2350_external_rtc_scan(uint8_t *addresses, uint32_t address_capacity);
bool rp2350_external_rtc_read_time(time_t *out_time);
bool rp2350_external_rtc_write_time(time_t time);
bool rp2350_external_rtc_read_timezone(TimezoneInfo *out_timezone);
bool rp2350_external_rtc_write_timezone(const TimezoneInfo *timezone);
bool rp2350_external_rtc_clear_timezone(void);
bool rp2350_external_rtc_is_present(void);
bool rp2350_external_rtc_has_valid_time(void);
int32_t rp2350_external_rtc_last_error(void);
uint32_t rp2350_external_rtc_read_success_count(void);
uint32_t rp2350_external_rtc_read_failure_count(void);
uint32_t rp2350_external_rtc_write_success_count(void);
uint32_t rp2350_external_rtc_write_failure_count(void);
uint32_t rp2350_external_rtc_timezone_read_success_count(void);
uint32_t rp2350_external_rtc_timezone_read_failure_count(void);
uint32_t rp2350_external_rtc_timezone_write_success_count(void);
uint32_t rp2350_external_rtc_timezone_write_failure_count(void);
