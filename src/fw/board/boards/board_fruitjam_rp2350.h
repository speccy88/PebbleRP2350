/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define BT_VENDOR_ID 0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

enum {
  FRUITJAM_PIN_BUTTON_BACK = 0,
  FRUITJAM_PIN_ESP_BUSY = 3,
  FRUITJAM_PIN_BUTTON_UP = 4,
  FRUITJAM_PIN_BUTTON_SELECT = 5,
  FRUITJAM_PIN_LCD_CS = 6,
  FRUITJAM_PIN_LCD_DISP = 7,
  FRUITJAM_PIN_ESP_TX = 8,
  FRUITJAM_PIN_ESP_RX = 9,
  FRUITJAM_PIN_LCD_EXTCOMIN = 10,
  FRUITJAM_PIN_ESP_RESET = 22,
  FRUITJAM_PIN_ESP_IRQ = 23,
  FRUITJAM_PIN_LCD_MISO = 28,
  FRUITJAM_PIN_LCD_SCK = 30,
  FRUITJAM_PIN_LCD_MOSI = 31,
  FRUITJAM_PIN_LCD_RAM_CS = 41,
  FRUITJAM_PIN_DEBUG_TX = 44,
  FRUITJAM_PIN_DEBUG_RX = 45,
  FRUITJAM_PIN_ESP_CS = 46,
  FRUITJAM_PIN_PSRAM_CS = 47,
};

extern UARTDevice *const DBG_UART;
extern DisplayDevice *const DISPLAY;
extern const BoardConfigActuator BOARD_CONFIG_VIBE;
extern const BoardConfigPower BOARD_CONFIG_POWER;
extern const BoardConfig BOARD_CONFIG;
extern const BoardConfigButton BOARD_CONFIG_BUTTON;

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
    .accel_config = {
        .default_motion_sensitivity = 85U,
    },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
    .mag_config = {{0}},
};
