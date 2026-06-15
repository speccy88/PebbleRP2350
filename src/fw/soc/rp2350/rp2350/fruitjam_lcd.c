/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_lcd.h"

#include "board/board.h"
#include "board/boards/board_fruitjam_rp2350.h"
#include "util/reverse.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define RESETS_BASE 0x40020000U
#define CLOCKS_BASE 0x40010000U
#define SPI1_BASE 0x40088000U
#define SIO_BASE 0xd0000000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define REG_ALIAS_SET_BITS 0x2000U
#define REG_ALIAS_CLR_BITS 0x3000U

#define GPIO_FUNC_SPI 1U
#define GPIO_FUNC_SIO 5U
#define GPIO_PAD_NORMAL 0x52U

#define RESETS_RESET_OFFSET 0x00U
#define RESETS_RESET_DONE_OFFSET 0x08U
#define RESETS_RESET_SPI1_BITS (1U << 19)

#define CLOCKS_CLK_PERI_CTRL_OFFSET 0x48U
#define CLOCKS_CLK_PERI_DIV_OFFSET 0x4cU
#define CLOCKS_CLK_PERI_CTRL_ENABLE_BITS (1U << 11)
#define CLOCKS_CLK_PERI_CTRL_ENABLED_BITS (1U << 28)
#define CLOCKS_CLK_PERI_DIV_1 0x00010000U

#define SIO_GPIO_OUT_SET_OFFSET 0x18U
#define SIO_GPIO_HI_OUT_SET_OFFSET 0x1cU
#define SIO_GPIO_OUT_CLR_OFFSET 0x20U
#define SIO_GPIO_HI_OUT_CLR_OFFSET 0x24U
#define SIO_GPIO_OE_SET_OFFSET 0x38U
#define SIO_GPIO_HI_OE_SET_OFFSET 0x3cU

#define SPI_SSPCR0_OFFSET 0x00U
#define SPI_SSPCR1_OFFSET 0x04U
#define SPI_SSPDR_OFFSET 0x08U
#define SPI_SSPSR_OFFSET 0x0cU
#define SPI_SSPCPSR_OFFSET 0x10U
#define SPI_SSPICR_OFFSET 0x20U
#define SPI_SSPDMACR_OFFSET 0x24U

#define SPI_SSPCR1_SSE_BITS (1U << 1)
#define SPI_SSPSR_BSY_BITS (1U << 4)
#define SPI_SSPSR_RNE_BITS (1U << 2)
#define SPI_SSPSR_TNF_BITS (1U << 1)

#define LCD_CMD_WRITE 0x80U
#define LCD_CMD_CLEAR 0x20U

static bool s_use_spi1;
static uint8_t s_capture_frame[FRUITJAM_LCD_CAPTURE_BYTES];
static uint32_t s_capture_sequence;

static void prv_capture_clear(void) {
  for (uint32_t i = 0; i < sizeof(s_capture_frame); ++i) {
    s_capture_frame[i] = 0xffU;
  }
  ++s_capture_sequence;
}

static void prv_capture_row(uint16_t y, const uint8_t *row) {
  if (y >= FRUITJAM_LCD_HEIGHT) {
    return;
  }

  uint8_t *capture_row = &s_capture_frame[(uint32_t)y * FRUITJAM_LCD_LINE_BYTES];
  for (uint32_t i = 0; i < FRUITJAM_LCD_LINE_BYTES; ++i) {
    capture_row[i] = row[i];
  }
}

void fruitjam_lcd_delay(uint32_t count) {
  for (volatile uint32_t i = 0; i < count; ++i) {
    __asm__ volatile("nop");
  }
}

static void prv_gpio_put(uint8_t pin, bool value) {
  if (pin < 32U) {
    REG32(SIO_BASE + (value ? SIO_GPIO_OUT_SET_OFFSET : SIO_GPIO_OUT_CLR_OFFSET)) = 1U << pin;
  } else {
    REG32(SIO_BASE + (value ? SIO_GPIO_HI_OUT_SET_OFFSET : SIO_GPIO_HI_OUT_CLR_OFFSET)) =
        1U << (pin - 32U);
  }
}

static void prv_gpio_output_enable(uint8_t pin) {
  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_SET_OFFSET) = 1U << pin;
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_SET_OFFSET) = 1U << (pin - 32U);
  }
}

static void prv_gpio_output_init(uint8_t pin, bool value) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = GPIO_FUNC_SIO;
  prv_gpio_put(pin, value);
  prv_gpio_output_enable(pin);
}

static void prv_gpio_set_function(uint8_t pin, uint32_t function) {
  REG32(PADS_BANK0_GPIO(pin)) = GPIO_PAD_NORMAL;
  REG32(IO_BANK0_GPIO_CTRL(pin)) = function;
}

static void prv_spi_drain_rx(void) {
  while (REG32(SPI1_BASE + SPI_SSPSR_OFFSET) & SPI_SSPSR_RNE_BITS) {
    (void)REG32(SPI1_BASE + SPI_SSPDR_OFFSET);
  }
}

static void prv_spi_wait_idle(void) {
  while (REG32(SPI1_BASE + SPI_SSPSR_OFFSET) & SPI_SSPSR_BSY_BITS) {
  }
  prv_spi_drain_rx();
  REG32(SPI1_BASE + SPI_SSPICR_OFFSET) = 0x1U;
}

static void prv_spi_write_byte(uint8_t value) {
  while (!(REG32(SPI1_BASE + SPI_SSPSR_OFFSET) & SPI_SSPSR_TNF_BITS)) {
  }
  REG32(SPI1_BASE + SPI_SSPDR_OFFSET) = value;
  while (REG32(SPI1_BASE + SPI_SSPSR_OFFSET) & SPI_SSPSR_BSY_BITS) {
  }
  prv_spi_drain_rx();
}

static void prv_write_byte(uint8_t value) {
  if (s_use_spi1) {
    prv_spi_write_byte(value);
    return;
  }

  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1) {
    prv_gpio_put(FRUITJAM_PIN_LCD_MOSI, (value & mask) != 0U);
    fruitjam_lcd_delay(8U);
    prv_gpio_put(FRUITJAM_PIN_LCD_SCK, true);
    fruitjam_lcd_delay(8U);
    prv_gpio_put(FRUITJAM_PIN_LCD_SCK, false);
    fruitjam_lcd_delay(8U);
  }
}

static void prv_select(void) {
  prv_gpio_put(FRUITJAM_PIN_LCD_CS, true);
  fruitjam_lcd_delay(64U);
}

static void prv_deselect(void) {
  if (s_use_spi1) {
    prv_spi_wait_idle();
  }

  fruitjam_lcd_delay(64U);
  prv_gpio_put(FRUITJAM_PIN_LCD_CS, false);
  fruitjam_lcd_delay(64U);
}

static void prv_enable_clk_peri_from_clk_sys(void) {
  REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_CTRL_OFFSET) = 0U;
  REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_DIV_OFFSET) = CLOCKS_CLK_PERI_DIV_1;
  REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_CTRL_OFFSET) = CLOCKS_CLK_PERI_CTRL_ENABLE_BITS;

  while (!(REG32(CLOCKS_BASE + CLOCKS_CLK_PERI_CTRL_OFFSET) &
           CLOCKS_CLK_PERI_CTRL_ENABLED_BITS)) {
  }
}

static void prv_spi1_init(void) {
  prv_enable_clk_peri_from_clk_sys();

  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_SET_BITS) = RESETS_RESET_SPI1_BITS;
  fruitjam_lcd_delay(64U);
  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_CLR_BITS) = RESETS_RESET_SPI1_BITS;
  while (!(REG32(RESETS_BASE + RESETS_RESET_DONE_OFFSET) & RESETS_RESET_SPI1_BITS)) {
  }

  REG32(SPI1_BASE + SPI_SSPCR1_OFFSET) = 0U;
  REG32(SPI1_BASE + SPI_SSPCPSR_OFFSET) = 100U;
  REG32(SPI1_BASE + SPI_SSPCR0_OFFSET) = (2U << 8) | 7U;
  REG32(SPI1_BASE + SPI_SSPDMACR_OFFSET) = 0U;
  REG32(SPI1_BASE + SPI_SSPICR_OFFSET) = 0x3U;
  REG32(SPI1_BASE + SPI_SSPCR1_OFFSET) = SPI_SSPCR1_SSE_BITS;

  prv_gpio_set_function(FRUITJAM_PIN_LCD_MISO, GPIO_FUNC_SPI);
  prv_gpio_set_function(FRUITJAM_PIN_LCD_SCK, GPIO_FUNC_SPI);
  prv_gpio_set_function(FRUITJAM_PIN_LCD_MOSI, GPIO_FUNC_SPI);
}

void fruitjam_lcd_init_pins(void) {
  s_use_spi1 = false;

  prv_gpio_output_init(FRUITJAM_PIN_LCD_CS, false);
  prv_gpio_output_init(FRUITJAM_PIN_LCD_RAM_CS, true);
  prv_gpio_output_init(FRUITJAM_PIN_ESP_CS, true);
  prv_gpio_output_init(FRUITJAM_PIN_LCD_SCK, false);
  prv_gpio_output_init(FRUITJAM_PIN_LCD_MOSI, false);
  prv_gpio_output_init(FRUITJAM_PIN_LCD_DISP, false);
  prv_gpio_output_init(FRUITJAM_PIN_LCD_EXTCOMIN, false);
}

void fruitjam_lcd_use_spi1(bool enabled) {
  if (enabled) {
    prv_spi1_init();
    s_use_spi1 = true;
    return;
  }

  s_use_spi1 = false;
  prv_gpio_output_init(FRUITJAM_PIN_LCD_SCK, false);
  prv_gpio_output_init(FRUITJAM_PIN_LCD_MOSI, false);
}

void fruitjam_lcd_set_display_enabled(bool enabled) {
  prv_gpio_put(FRUITJAM_PIN_LCD_DISP, enabled);
}

void fruitjam_lcd_set_vcom(bool state) {
  prv_gpio_put(FRUITJAM_PIN_LCD_EXTCOMIN, state);
}

void fruitjam_lcd_clear(void) {
  prv_select();
  prv_write_byte(LCD_CMD_CLEAR);
  prv_write_byte(0x00U);
  prv_deselect();
  prv_capture_clear();
}

void fruitjam_lcd_begin_write(void) {
  prv_select();
  prv_write_byte(LCD_CMD_WRITE);
}

void fruitjam_lcd_write_row_msb(uint16_t y, const uint8_t *row) {
  prv_capture_row(y, row);

  prv_write_byte((uint8_t)reverse_byte((uint8_t)(y + 1U)));
  for (uint32_t i = 0; i < FRUITJAM_LCD_LINE_BYTES; ++i) {
    prv_write_byte(row[i]);
  }
  prv_write_byte(0x00U);
}

void fruitjam_lcd_end_write(void) {
  prv_write_byte(0x00U);
  prv_deselect();
  ++s_capture_sequence;
}

void fruitjam_lcd_write_generated_frame(FruitJamLCDPatternByteFn get_byte, void *context) {
  uint8_t row[FRUITJAM_LCD_LINE_BYTES];

  fruitjam_lcd_begin_write();

  for (uint16_t y = 0; y < FRUITJAM_LCD_HEIGHT; ++y) {
    for (uint16_t byte_x = 0; byte_x < FRUITJAM_LCD_LINE_BYTES; ++byte_x) {
      row[byte_x] = get_byte(y, byte_x, context);
    }
    fruitjam_lcd_write_row_msb(y, row);
  }

  fruitjam_lcd_end_write();
}

const uint8_t *fruitjam_lcd_capture_data(void) {
  return s_capture_frame;
}

uint32_t fruitjam_lcd_capture_sequence(void) {
  return s_capture_sequence;
}
