/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define BT_VENDOR_ID 0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

enum {
  PICO2W_PIN_DEBUG_TX = 0,
  PICO2W_PIN_DEBUG_RX = 1,
  PICO2W_PIN_I2C_SDA = 4,
  PICO2W_PIN_I2C_SCL = 5,
  PICO2W_PIN_BUTTON_BACK = 6,
  PICO2W_PIN_BUTTON_UP = 7,
  PICO2W_PIN_BUTTON_SELECT = 8,
  PICO2W_PIN_LCD_MISO = 16,
  PICO2W_PIN_LCD_CS = 17,
  PICO2W_PIN_LCD_SCK = 18,
  PICO2W_PIN_LCD_MOSI = 19,
  PICO2W_PIN_LCD_DISP = 20,
  PICO2W_PIN_LCD_EXTCOMIN = 21,
};

extern UARTDevice *const DBG_UART;
extern DisplayDevice *const DISPLAY;
extern const BoardConfigActuator BOARD_CONFIG_VIBE;
extern const BoardConfigPower BOARD_CONFIG_POWER;
extern const BoardConfig BOARD_CONFIG;
extern const BoardConfigButton BOARD_CONFIG_BUTTON;

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
    .accel_config =
        {
            .default_motion_sensitivity = 85U,
        },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
    .mag_config = {{0}},
};
