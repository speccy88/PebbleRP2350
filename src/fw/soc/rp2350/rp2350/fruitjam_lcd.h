/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FRUITJAM_LCD_WIDTH 144U
#define FRUITJAM_LCD_HEIGHT 168U
#define FRUITJAM_LCD_LINE_BYTES (FRUITJAM_LCD_WIDTH / 8U)
#define FRUITJAM_LCD_CAPTURE_BYTES (FRUITJAM_LCD_HEIGHT * FRUITJAM_LCD_LINE_BYTES)
#define FRUITJAM_LCD_PIN_UNUSED 0xffU

typedef struct {
  uint8_t spi_index;
  uint8_t sck_gpio;
  uint8_t mosi_gpio;
  uint8_t miso_gpio;
  uint8_t lcd_cs_gpio;
  uint8_t ram_cs_gpio;
  uint8_t disp_gpio;
  uint8_t extcomin_gpio;
  uint8_t inactive_cs_gpio;
} Rp2350MemoryLcdConfig;

typedef Rp2350MemoryLcdConfig FruitJamLCDConfig;

typedef uint8_t (*FruitJamLCDPatternByteFn)(uint16_t y, uint16_t byte_x, void *context);

void fruitjam_lcd_delay(uint32_t count);
void fruitjam_lcd_init_pins(void);
void fruitjam_lcd_use_hardware_spi(bool enabled);
void fruitjam_lcd_set_display_enabled(bool enabled);
void fruitjam_lcd_set_vcom(bool state);
void fruitjam_lcd_clear(void);
void fruitjam_lcd_begin_write(void);
void fruitjam_lcd_write_row_msb(uint16_t y, const uint8_t *row);
void fruitjam_lcd_end_write(void);
void fruitjam_lcd_write_generated_frame(FruitJamLCDPatternByteFn get_byte, void *context);
const uint8_t *fruitjam_lcd_capture_data(void);
uint32_t fruitjam_lcd_capture_sequence(void);
bool fruitjam_lcd_is_hardware_spi_enabled(void);
uint32_t fruitjam_lcd_spi_index(void);
uint32_t fruitjam_lcd_spi_cr0(void);
uint32_t fruitjam_lcd_spi_cpsr(void);
uint32_t fruitjam_lcd_spi_status(void);
uint32_t fruitjam_lcd_clk_peri_ctrl(void);
uint32_t fruitjam_lcd_clk_peri_div(void);
uint32_t fruitjam_lcd_spi_hz(void);
uint32_t fruitjam_lcd_transfer_count(void);
uint32_t fruitjam_lcd_last_transfer_us(void);
uint32_t fruitjam_lcd_max_transfer_us(void);
uint16_t fruitjam_lcd_last_transfer_rows(void);
