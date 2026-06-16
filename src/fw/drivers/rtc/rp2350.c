/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/rtc.h"

#include "board/board.h"
#include "soc/rp2350/rp2350/rp2350_external_rtc.h"

#include "soc/rp2350/rp2350/hardware/timer.h"
#include "system/rtc_registers.h"

#include <string.h>

#define RP2350_BACKUP_REGISTER_COUNT 16
#define RP2350_WATCHDOG_BASE 0x400d8000UL
#define RP2350_WATCHDOG_SCRATCH0_OFFSET 0x0cUL
#define RP2350_WATCHDOG_SCRATCH_STRIDE 4UL
#define TZ_BACKUP_BASE 11

static time_t s_time_base;
static RtcTicks s_time_tick_base;
static RtcTicks s_alarm_set_time;
static bool s_alarm_initialized;
static uint32_t s_backup[RP2350_BACKUP_REGISTER_COUNT];
static uint32_t s_last_time_us;
static uint64_t s_time_us_high;

void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value);

static volatile uint32_t *prv_scratch_register(uint32_t scratch_id) {
  return (volatile uint32_t *)(RP2350_WATCHDOG_BASE + RP2350_WATCHDOG_SCRATCH0_OFFSET +
                               (scratch_id * RP2350_WATCHDOG_SCRATCH_STRIDE));
}

static int prv_backup_id_to_scratch(uint32_t reg_id) {
  switch (reg_id) {
    case RTC_BKP_BOOTBIT_DR:
      return 0;
    case REBOOT_REASON_REGISTER_1:
      return 1;
    case REBOOT_REASON_STUCK_TASK_PC:
      return 2;
    case REBOOT_REASON_STUCK_TASK_LR:
      return 3;
    case REBOOT_REASON_STUCK_TASK_CALLBACK:
      return 4;
    case SLOT_OF_LAST_LAUNCHED_APP:
      return 5;
    case RTC_BKP_FLASH_ERASE_PROGRESS:
      return 6;
    case CURRENT_TIME_REGISTER:
      return 7;
    default:
      return -1;
  }
}

static uint64_t prv_time_us_64(void) {
  const uint32_t now = time_us_32();
  if (now < s_last_time_us) {
    s_time_us_high += (1ULL << 32);
  }
  s_last_time_us = now;

  return s_time_us_high + now;
}

static RtcTicks prv_us_to_ticks(uint64_t us) {
  return (us * RTC_TICKS_HZ) / 1000000ULL;
}

static bool prv_external_rtc_init_and_read(time_t *out_time) {
  if (!BOARD_CONFIG_EXTERNAL_RTC || !rp2350_external_rtc_init(BOARD_CONFIG_EXTERNAL_RTC)) {
    return false;
  }
  return rp2350_external_rtc_read_time(out_time);
}

static void prv_external_rtc_write_time(time_t time) {
  if (!BOARD_CONFIG_EXTERNAL_RTC) {
    return;
  }
  (void)rp2350_external_rtc_write_time(time);
}

static bool prv_external_rtc_read_timezone(TimezoneInfo *tzinfo) {
  if (!BOARD_CONFIG_EXTERNAL_RTC) {
    return false;
  }
  return rp2350_external_rtc_read_timezone(tzinfo);
}

static void prv_external_rtc_write_timezone(const TimezoneInfo *tzinfo) {
  if (!BOARD_CONFIG_EXTERNAL_RTC) {
    return;
  }
  (void)rp2350_external_rtc_write_timezone(tzinfo);
}

static void prv_external_rtc_clear_timezone(void) {
  if (!BOARD_CONFIG_EXTERNAL_RTC) {
    return;
  }
  (void)rp2350_external_rtc_clear_timezone();
}

static void prv_store_timezone_registers(const TimezoneInfo *tzinfo) {
  const uint32_t *raw = (const uint32_t *)tzinfo;
  _Static_assert(sizeof(TimezoneInfo) <= 5 * sizeof(uint32_t),
                 "RTC Set Timezone invalid data size");

  RTC_WriteBackupRegister(TZ_BACKUP_BASE + 0, raw[0]);
  RTC_WriteBackupRegister(TZ_BACKUP_BASE + 1, raw[1]);
  RTC_WriteBackupRegister(TZ_BACKUP_BASE + 2, raw[2]);
  RTC_WriteBackupRegister(TZ_BACKUP_BASE + 3, raw[3]);
  RTC_WriteBackupRegister(TZ_BACKUP_BASE + 4, raw[4]);
}

void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value) {
  const int scratch_id = prv_backup_id_to_scratch(reg_id);
  if (scratch_id >= 0) {
    *prv_scratch_register((uint32_t)scratch_id) = value;
    return;
  }

  if (reg_id < RP2350_BACKUP_REGISTER_COUNT) {
    s_backup[reg_id] = value;
  }
}

uint32_t RTC_ReadBackupRegister(uint32_t reg_id) {
  const int scratch_id = prv_backup_id_to_scratch(reg_id);
  if (scratch_id >= 0) {
    return *prv_scratch_register((uint32_t)scratch_id);
  }

  if (reg_id < RP2350_BACKUP_REGISTER_COUNT) {
    return s_backup[reg_id];
  }
  return 0;
}

void rtc_init(void) {
  rp2350_timer_init_ticks();

  s_last_time_us = time_us_32();
  s_time_us_high = 0;
  s_time_tick_base = rtc_get_ticks();

  time_t external_time;
  if (prv_external_rtc_init_and_read(&external_time)) {
    s_time_base = external_time;
    s_time_tick_base = rtc_get_ticks();
  }

  TimezoneInfo tzinfo;
  if (prv_external_rtc_read_timezone(&tzinfo)) {
    prv_store_timezone_registers(&tzinfo);
  }
}

void rtc_calibrate_frequency(uint32_t frequency) {
  (void)frequency;
}

void rtc_init_timers(void) {
}

void rtc_set_time(time_t time) {
  s_time_base = time;
  s_time_tick_base = rtc_get_ticks();
  prv_external_rtc_write_time(time);
}

time_t rtc_get_time(void) {
  return s_time_base + (time_t)((rtc_get_ticks() - s_time_tick_base) / RTC_TICKS_HZ);
}

void rtc_set_time_tm(struct tm *time_tm) {
  rtc_set_time(mktime(time_tm));
}

void rtc_get_time_tm(struct tm *time_tm) {
  time_t t = rtc_get_time();
  localtime_r(&t, time_tm);
}

void rtc_get_time_ms(time_t *out_seconds, uint16_t *out_ms) {
  const RtcTicks ticks_since_time_base = rtc_get_ticks() - s_time_tick_base;
  *out_seconds = s_time_base + (time_t)(ticks_since_time_base / RTC_TICKS_HZ);
  *out_ms = (uint16_t)(((ticks_since_time_base % RTC_TICKS_HZ) * 1000) / RTC_TICKS_HZ);
}

bool rtc_sanitize_struct_tm(struct tm *t) {
  if (t->tm_year < 100) {
    t->tm_year = 100;
    return true;
  }
  if (t->tm_year > 137) {
    t->tm_year = 137;
    return true;
  }
  return false;
}

bool rtc_sanitize_time_t(time_t *t) {
  struct tm time_struct;
  gmtime_r(t, &time_struct);
  bool result = rtc_sanitize_struct_tm(&time_struct);
  *t = mktime(&time_struct);
  return result;
}

RtcTicks rtc_get_ticks(void) {
  return prv_us_to_ticks(prv_time_us_64());
}

void rtc_alarm_init(void) {
  s_alarm_initialized = true;
}

void rtc_alarm_set(RtcTicks num_ticks) {
  (void)num_ticks;
  s_alarm_set_time = rtc_get_ticks();
}

RtcTicks rtc_alarm_get_elapsed_ticks(void) {
  return rtc_get_ticks() - s_alarm_set_time;
}

bool rtc_alarm_is_initialized(void) {
  return s_alarm_initialized;
}

void rtc_set_timezone(TimezoneInfo *tzinfo) {
  prv_store_timezone_registers(tzinfo);
  prv_external_rtc_write_timezone(tzinfo);
}

void rtc_get_timezone(TimezoneInfo *tzinfo) {
  uint32_t *raw = (uint32_t *)tzinfo;
  raw[0] = RTC_ReadBackupRegister(TZ_BACKUP_BASE + 0);
  raw[1] = RTC_ReadBackupRegister(TZ_BACKUP_BASE + 1);
  raw[2] = RTC_ReadBackupRegister(TZ_BACKUP_BASE + 2);
  raw[3] = RTC_ReadBackupRegister(TZ_BACKUP_BASE + 3);
  raw[4] = RTC_ReadBackupRegister(TZ_BACKUP_BASE + 4);
}

uint16_t rtc_get_timezone_id(void) {
  return (RTC_ReadBackupRegister(TZ_BACKUP_BASE + 1) >> 16) & 0xffff;
}

bool rtc_is_timezone_set(void) {
  return RTC_ReadBackupRegister(TZ_BACKUP_BASE + 0) != 0;
}

const char *rtc_get_time_string(char *buffer) {
  return time_t_to_string(buffer, rtc_get_time());
}

const char *time_t_to_string(char *buffer, time_t t) {
  struct tm time;
  localtime_r(&t, &time);
  strftime(buffer, TIME_STRING_BUFFER_SIZE, "%c", &time);
  return buffer;
}

void rtc_timezone_clear(void) {
  for (int i = 0; i < 5; i++) {
    RTC_WriteBackupRegister(TZ_BACKUP_BASE + i, 0);
  }
  prv_external_rtc_clear_timezone();
}
