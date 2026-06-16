/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"

#include <stdint.h>
#include <stdio.h>

#define STATUS_PERIOD_MS 1000U

static uint32_t s_last_status_ms;
static uint32_t s_tick_count;

static void prv_cdc_write_str(const char *str) {
  if (!tud_cdc_connected()) {
    return;
  }

  while (*str) {
    const uint32_t available = tud_cdc_write_available();
    if (available == 0) {
      tud_task();
      continue;
    }

    const uint32_t written = tud_cdc_write(str, 1);
    if (written == 1) {
      ++str;
    }
  }
  tud_cdc_write_flush();
}

static void prv_cdc_service_rx(void) {
  while (tud_cdc_available()) {
    char ch;
    tud_cdc_read(&ch, 1);
    tud_cdc_write(&ch, 1);
    if (ch == '\r') {
      tud_cdc_write("\n", 1);
    }
  }
  tud_cdc_write_flush();
}

int main(void) {
  board_init();
  tusb_init();

  s_last_status_ms = to_ms_since_boot(get_absolute_time());

  while (true) {
    tud_task();
    prv_cdc_service_rx();

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if ((now_ms - s_last_status_ms) >= STATUS_PERIOD_MS) {
      s_last_status_ms = now_ms;
      ++s_tick_count;

      char line[96];
      snprintf(line, sizeof(line), "pico descriptor probe tick %lu mounted=%u cdc=%u\r\n",
               (unsigned long)s_tick_count, tud_mounted() ? 1U : 0U,
               tud_cdc_connected() ? 1U : 0U);
      prv_cdc_write_str(line);
    }

    sleep_ms(1);
  }
}
