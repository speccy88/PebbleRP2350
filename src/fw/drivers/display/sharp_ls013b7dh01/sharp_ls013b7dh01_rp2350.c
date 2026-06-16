/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sharp_ls013b7dh01.h"

#include "soc/rp2350/rp2350/fruitjam_boot_progress.h"
#include "soc/rp2350/rp2350/fruitjam_lcd.h"
#include "system/passert.h"
#include "util/reverse.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define BOOT_FRAMEBUFFER_BYTES_PER_ROW (DISPLAY_FRAMEBUFFER_BYTES / PBL_DISPLAY_HEIGHT)

#define VCOM_TASK_STACK_SIZE 128U
#define VCOM_TASK_PRIORITY 1U
#define VCOM_TOGGLE_MS 500U
#define BRINGUP_LOOP_FRAME_MS 1800U

_Static_assert(DISP_LINE_BYTES == FRUITJAM_LCD_LINE_BYTES,
               "Fruit Jam LCD helper must match Sharp display row width");

static bool s_rotated_180;
static bool s_updating;
static bool s_initialized;
static bool s_vcom_state;
static TaskHandle_t s_vcom_task;
#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_BRINGUP_LOOP) || \
    defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN)
static uint8_t s_diag_frame[DISPLAY_FRAMEBUFFER_BYTES];
#endif
static uint8_t s_wire_row[DISP_LINE_BYTES];

typedef enum {
  DiagPattern_DriverInit,
  DiagPattern_SchedulerLoop,
#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN)
  DiagPattern_DriverSpi,
#endif
} DiagPattern;

static void prv_vcom_task(void *data) {
  (void)data;

  while (true) {
    s_vcom_state = !s_vcom_state;
    fruitjam_lcd_set_vcom(s_vcom_state);
    vTaskDelay(pdMS_TO_TICKS(VCOM_TOGGLE_MS));
  }
}

#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN)
static void prv_diag_set_pixel(uint16_t x, uint16_t y, bool black) {
  uint8_t *byte = &s_diag_frame[(uint32_t)y * BOOT_FRAMEBUFFER_BYTES_PER_ROW + (x / 8U)];
  const uint8_t mask = (uint8_t)(1U << (x & 7U));

  if (black) {
    *byte &= (uint8_t)~mask;
  } else {
    *byte |= mask;
  }
}
#endif

static void prv_format_wire_row(const uint8_t *data, bool rotated) {
  if (rotated) {
    for (uint32_t i = 0; i < DISP_LINE_BYTES; ++i) {
      s_wire_row[i] = (uint8_t)reverse_byte(data[DISP_LINE_BYTES - 1U - i]);
    }
    return;
  }

  for (uint32_t i = 0; i < DISP_LINE_BYTES; ++i) {
    s_wire_row[i] = (uint8_t)reverse_byte(data[i]);
  }
}

static void prv_write_pebble_row(uint16_t y, const uint8_t *data) {
  const uint16_t wire_y = s_rotated_180 ? (PBL_DISPLAY_HEIGHT - 1U - y) : y;

  prv_format_wire_row(data, s_rotated_180);
  fruitjam_lcd_write_row_msb(wire_y, s_wire_row);
}

#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN)
static bool prv_spi_diag_pixel(uint16_t x, uint16_t y) {
  const bool vertical_bar = ((x / 10U) & 1U) != 0U;
  const bool diagonal =
      (uint32_t)y * PBL_DISPLAY_WIDTH <= ((uint32_t)x + 1U) * PBL_DISPLAY_HEIGHT &&
      ((uint32_t)y + 1U) * PBL_DISPLAY_WIDTH >= (uint32_t)x * PBL_DISPLAY_HEIGHT;
  const bool opposite_diagonal =
      (uint32_t)y * PBL_DISPLAY_WIDTH <= ((uint32_t)(PBL_DISPLAY_WIDTH - x)) * PBL_DISPLAY_HEIGHT &&
      ((uint32_t)y + 1U) * PBL_DISPLAY_WIDTH >=
          ((uint32_t)(PBL_DISPLAY_WIDTH - 1U - x)) * PBL_DISPLAY_HEIGHT;

  return vertical_bar || diagonal || opposite_diagonal;
}

static void prv_show_spi_pattern(void) {
  memset(s_diag_frame, 0xff, sizeof(s_diag_frame));

  for (uint16_t y = 0; y < PBL_DISPLAY_HEIGHT; ++y) {
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH; ++x) {
      if (prv_spi_diag_pixel(x, y)) {
        prv_diag_set_pixel(x, y, true);
      }
    }
  }

  display_update_boot_frame(s_diag_frame);
}
#endif

static void prv_show_init_pattern(DiagPattern pattern) {
#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN)
  if (pattern == DiagPattern_DriverSpi) {
    prv_show_spi_pattern();
    return;
  }
#endif

  const FruitJamBootProgressStage stage = (pattern == DiagPattern_SchedulerLoop)
                                              ? FruitJamBootProgressStageDriverReady
                                              : FruitJamBootProgressStageDisplayInit;
  fruitjam_boot_progress_write_frame(stage);
}

#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_BRINGUP_LOOP)
static void prv_show_white_frame(void) {
  memset(s_diag_frame, 0xff, sizeof(s_diag_frame));
  display_update_boot_frame(s_diag_frame);
}

static void prv_run_bringup_loop(void) {
  while (true) {
    prv_show_white_frame();
    vTaskDelay(pdMS_TO_TICKS(BRINGUP_LOOP_FRAME_MS));
    prv_show_init_pattern(DiagPattern_SchedulerLoop);
    vTaskDelay(pdMS_TO_TICKS(BRINGUP_LOOP_FRAME_MS));
  }
}
#endif

void display_init(void) {
  if (s_initialized) {
    return;
  }

  fruitjam_lcd_init_pins();
  fruitjam_lcd_set_display_enabled(true);
  fruitjam_lcd_set_vcom(false);
#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_HARDWARE_SPI)
  fruitjam_lcd_use_hardware_spi(true);
#else
  fruitjam_lcd_use_hardware_spi(false);
#endif
  display_clear();
  prv_show_init_pattern(DiagPattern_DriverInit);

  if (!s_vcom_task) {
    xTaskCreate(prv_vcom_task, "SharpVCOM", VCOM_TASK_STACK_SIZE, NULL, VCOM_TASK_PRIORITY,
                &s_vcom_task);
  }

#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_SPI_DIAGNOSTIC_PATTERN)
  display_clear();
  prv_show_init_pattern(DiagPattern_DriverSpi);
#endif

  s_initialized = true;

#if defined(CONFIG_DISPLAY_SHARP_LS013B7DH01_RP2350_BRINGUP_LOOP)
  prv_run_bringup_loop();
#endif
}

void display_clear(void) {
  PBL_ASSERTN(!s_updating);

  fruitjam_lcd_clear();
}

void display_set_enabled(bool enabled) {
  fruitjam_lcd_set_display_enabled(enabled);
}

void display_set_rotated(bool rotated) {
  s_rotated_180 = rotated;
}

bool display_update_in_progress(void) {
  return s_updating;
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  DisplayRow row;

  PBL_ASSERTN(nrcb != NULL);
  PBL_ASSERTN(!s_updating);

  s_updating = true;

  fruitjam_lcd_begin_write();

  while (nrcb(&row)) {
    if (row.address >= PBL_DISPLAY_HEIGHT) {
      continue;
    }

    prv_write_pebble_row(row.address, row.data);
  }

  fruitjam_lcd_end_write();

  s_updating = false;

  if (uccb) {
    uccb();
  }
}

void display_update_boot_frame(uint8_t *framebuffer) {
  PBL_ASSERTN(framebuffer != NULL);
  PBL_ASSERTN(!s_updating);

  s_updating = true;

  fruitjam_lcd_begin_write();

  for (uint16_t y = 0; y < PBL_DISPLAY_HEIGHT; ++y) {
    const uint8_t *row = &framebuffer[(uint32_t)y * BOOT_FRAMEBUFFER_BYTES_PER_ROW];
    prv_write_pebble_row(y, row);
  }

  fruitjam_lcd_end_write();

  s_updating = false;
}
