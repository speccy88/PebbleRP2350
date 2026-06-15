/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FRUITJAM_LCD_WIDTH 144U
#define FRUITJAM_LCD_HEIGHT 168U
#define FRUITJAM_LCD_LINE_BYTES (FRUITJAM_LCD_WIDTH / 8U)
#define FRUITJAM_LCD_CAPTURE_BYTES (FRUITJAM_LCD_HEIGHT * FRUITJAM_LCD_LINE_BYTES)

typedef uint8_t (*FruitJamLCDPatternByteFn)(uint16_t y, uint16_t byte_x, void *context);

void fruitjam_lcd_delay(uint32_t count);
void fruitjam_lcd_init_pins(void);
void fruitjam_lcd_use_spi1(bool enabled);
void fruitjam_lcd_set_display_enabled(bool enabled);
void fruitjam_lcd_set_vcom(bool state);
void fruitjam_lcd_clear(void);
void fruitjam_lcd_begin_write(void);
void fruitjam_lcd_write_row_msb(uint16_t y, const uint8_t *row);
void fruitjam_lcd_end_write(void);
void fruitjam_lcd_write_generated_frame(FruitJamLCDPatternByteFn get_byte, void *context);
const uint8_t *fruitjam_lcd_capture_data(void);
uint32_t fruitjam_lcd_capture_sequence(void);
