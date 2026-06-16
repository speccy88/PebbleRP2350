/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/uart/rp2350.h"
#include "soc/rp2350/rp2350/fruitjam_lcd.h"

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

static const Rp2350MemoryLcdConfig s_display = {
    .spi_index = 1,
    .sck_gpio = FRUITJAM_PIN_LCD_SCK,
    .mosi_gpio = FRUITJAM_PIN_LCD_MOSI,
    .miso_gpio = FRUITJAM_PIN_LCD_MISO,
    .lcd_cs_gpio = FRUITJAM_PIN_LCD_CS,
    .ram_cs_gpio = FRUITJAM_PIN_LCD_RAM_CS,
    .disp_gpio = FRUITJAM_PIN_LCD_DISP,
    .extcomin_gpio = FRUITJAM_PIN_LCD_EXTCOMIN,
    .inactive_cs_gpio = FRUITJAM_PIN_ESP_CS,
};

DisplayDevice *const DISPLAY = (DisplayDevice *)&s_display;

#if defined(CONFIG_RP2350_EXTERNAL_RTC_DS1307)
static const Rp2350ExternalRtcConfig s_external_rtc = {
    .type = Rp2350ExternalRtcTypeDs1307,
    .i2c_address = CONFIG_RP2350_EXTERNAL_RTC_I2C_ADDRESS,
    .sda_gpio = CONFIG_RP2350_EXTERNAL_RTC_SDA_GPIO,
    .scl_gpio = CONFIG_RP2350_EXTERNAL_RTC_SCL_GPIO,
};

const Rp2350ExternalRtcConfig *const BOARD_CONFIG_EXTERNAL_RTC = &s_external_rtc;
#else
const Rp2350ExternalRtcConfig *const BOARD_CONFIG_EXTERNAL_RTC = NULL;
#endif

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
