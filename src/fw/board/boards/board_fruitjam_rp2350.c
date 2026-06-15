/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/uart/rp2350.h"

#define UART0_BASE 0x40070000U
#define GPIO_FUNC_UART 2U
#define RESETS_RESET_UART0_BITS (1U << 26)

static UARTDeviceState s_dbg_uart_state;

static struct UARTDevice s_dbg_uart = {
    .state = &s_dbg_uart_state,
    .base_addr = UART0_BASE,
    .tx_gpio = FRUITJAM_PIN_DEBUG_TX,
    .rx_gpio = FRUITJAM_PIN_DEBUG_RX,
    .tx_function = GPIO_FUNC_UART,
    .rx_function = GPIO_FUNC_UART,
    .reset_bits = RESETS_RESET_UART0_BITS,
    .irqn = UART0_IRQ_IRQn,
    .irq_priority = 5,
};

UARTDevice *const DBG_UART = (UARTDevice *)&s_dbg_uart;

IRQ_MAP(UART0_IRQ, uart_irq_handler, DBG_UART);

typedef struct FruitJamDisplayDevice {
  uint8_t spi_index;
  uint8_t sck_gpio;
  uint8_t mosi_gpio;
  uint8_t miso_gpio;
  uint8_t lcd_cs_gpio;
  uint8_t ram_cs_gpio;
  uint8_t disp_gpio;
  uint8_t extcomin_gpio;
} FruitJamDisplayDevice;

static const FruitJamDisplayDevice s_display = {
    .spi_index = 1,
    .sck_gpio = FRUITJAM_PIN_LCD_SCK,
    .mosi_gpio = FRUITJAM_PIN_LCD_MOSI,
    .miso_gpio = FRUITJAM_PIN_LCD_MISO,
    .lcd_cs_gpio = FRUITJAM_PIN_LCD_CS,
    .ram_cs_gpio = FRUITJAM_PIN_LCD_RAM_CS,
    .disp_gpio = FRUITJAM_PIN_LCD_DISP,
    .extcomin_gpio = FRUITJAM_PIN_LCD_EXTCOMIN,
};

DisplayDevice *const DISPLAY = (DisplayDevice *)&s_display;

const BoardConfigActuator BOARD_CONFIG_VIBE = {
    .options = 0,
};

const BoardConfigPower BOARD_CONFIG_POWER = {
    .low_power_threshold = 2U,
    .battery_capacity_hours = 168U,
    .fixed_power = true,
};

const BoardConfig BOARD_CONFIG = {
    .backlight_on_percent = 100,
    .ambient_light_dark_threshold = 150,
    .ambient_k_delta_threshold = 25,
};

const BoardConfigButton BOARD_CONFIG_BUTTON = {
    .buttons =
        {
            [BUTTON_ID_BACK] = {"Back", RP2350_GPIO_RESOURCE_EXISTS, FRUITJAM_PIN_BUTTON_BACK,
                                GPIO_PuPd_UP, false},
            [BUTTON_ID_UP] = {"Up", RP2350_GPIO_RESOURCE_EXISTS, FRUITJAM_PIN_BUTTON_UP,
                              GPIO_PuPd_UP, false},
            [BUTTON_ID_SELECT] = {"Select", RP2350_GPIO_RESOURCE_EXISTS, FRUITJAM_PIN_BUTTON_SELECT,
                                  GPIO_PuPd_UP, false},
            [BUTTON_ID_DOWN] = {"Down", GPIO_Port_NULL, GPIO_Pin_NULL, GPIO_PuPd_NOPULL, false},
        },
    .timer = NULL,
    .timer_irqn = TIMER0_IRQ_0_IRQn,
};

void board_early_init(void) {}

void board_init(void) {}
