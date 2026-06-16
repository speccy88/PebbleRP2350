/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_lcd.h"

#include "board/board.h"
#include "soc/rp2350/rp2350/hardware/clocks.h"
#include "hardware/timer.h"
#include "util/reverse.h"

#include <string.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define RESETS_BASE 0x40020000U
#define SPI0_BASE 0x40080000U
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
#define RESETS_RESET_SPI0_BITS (1U << 18)
#define RESETS_RESET_SPI1_BITS (1U << 19)

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
#define SPI_SSPCR0_DSS_8_BITS 7U
#define SPI_SSPCR0_SCR(value) ((value) << 8U)
#define SPI_SSPCR0_SCR_DIV3 2U
#define SPI_SSPCPSR_1MHZ_AT_12MHZ_CLK_PERI 4U

#define LCD_CMD_WRITE 0x80U
#define LCD_CMD_CLEAR 0x20U

static bool s_use_hardware_spi;
static const Rp2350MemoryLcdConfig *s_config;
static uint8_t s_capture_frame[FRUITJAM_LCD_CAPTURE_BYTES];
static uint32_t s_capture_sequence;
static uint32_t s_transfer_start_us;
static uint32_t s_transfer_count;
static uint32_t s_last_transfer_us;
static uint32_t s_max_transfer_us;
static uint16_t s_current_transfer_rows;
static uint16_t s_last_transfer_rows;

static void prv_transfer_begin(void) {
  s_transfer_start_us = time_us_32();
  s_current_transfer_rows = 0U;
}

static void prv_transfer_end(void) {
  const uint32_t elapsed_us = time_us_32() - s_transfer_start_us;

  s_last_transfer_us = elapsed_us;
  if (elapsed_us > s_max_transfer_us) {
    s_max_transfer_us = elapsed_us;
  }
  s_last_transfer_rows = s_current_transfer_rows;
  ++s_transfer_count;
}

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
  memcpy(capture_row, row, FRUITJAM_LCD_LINE_BYTES);
}

static const Rp2350MemoryLcdConfig *prv_config(void) {
  if (!s_config) {
    s_config = (const Rp2350MemoryLcdConfig *)DISPLAY;
  }
  return s_config;
}

static uint32_t prv_spi_base(void) {
  return (prv_config()->spi_index == 0U) ? SPI0_BASE : SPI1_BASE;
}

static uint32_t prv_spi_reset_bits(void) {
  return (prv_config()->spi_index == 0U) ? RESETS_RESET_SPI0_BITS : RESETS_RESET_SPI1_BITS;
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
  const uint32_t spi_base = prv_spi_base();
  while (REG32(spi_base + SPI_SSPSR_OFFSET) & SPI_SSPSR_RNE_BITS) {
    (void)REG32(spi_base + SPI_SSPDR_OFFSET);
  }
}

static void prv_spi_wait_idle(void) {
  const uint32_t spi_base = prv_spi_base();
  while (REG32(spi_base + SPI_SSPSR_OFFSET) & SPI_SSPSR_BSY_BITS) {
  }
  prv_spi_drain_rx();
  REG32(spi_base + SPI_SSPICR_OFFSET) = 0x1U;
}

static void prv_spi_write_bytes(const uint8_t *data, uint32_t length) {
  uint32_t offset = 0;
  const uint32_t spi_base = prv_spi_base();

  while (offset < length) {
    if (REG32(spi_base + SPI_SSPSR_OFFSET) & SPI_SSPSR_TNF_BITS) {
      REG32(spi_base + SPI_SSPDR_OFFSET) = data[offset++];
    }
    prv_spi_drain_rx();
  }
}

static void prv_write_byte(uint8_t value) {
  if (s_use_hardware_spi) {
    prv_spi_write_bytes(&value, 1U);
    return;
  }

  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1) {
    prv_gpio_put(prv_config()->mosi_gpio, (value & mask) != 0U);
    fruitjam_lcd_delay(8U);
    prv_gpio_put(prv_config()->sck_gpio, true);
    fruitjam_lcd_delay(8U);
    prv_gpio_put(prv_config()->sck_gpio, false);
    fruitjam_lcd_delay(8U);
  }
}

static void prv_write_bytes(const uint8_t *data, uint32_t length) {
  if (s_use_hardware_spi) {
    prv_spi_write_bytes(data, length);
    return;
  }

  for (uint32_t i = 0; i < length; ++i) {
    prv_write_byte(data[i]);
  }
}

static void prv_select(void) {
  prv_gpio_put(prv_config()->lcd_cs_gpio, true);
  fruitjam_lcd_delay(64U);
}

static void prv_deselect(void) {
  if (s_use_hardware_spi) {
    prv_spi_wait_idle();
  }

  fruitjam_lcd_delay(64U);
  prv_gpio_put(prv_config()->lcd_cs_gpio, false);
  fruitjam_lcd_delay(64U);
}

static void prv_hardware_spi_init(void) {
  rp2350_clk_peri_enable_xosc();

  const uint32_t spi_base = prv_spi_base();
  const uint32_t spi_reset_bits = prv_spi_reset_bits();

  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_SET_BITS) = spi_reset_bits;
  fruitjam_lcd_delay(64U);
  REG32(RESETS_BASE + RESETS_RESET_OFFSET + REG_ALIAS_CLR_BITS) = spi_reset_bits;
  while (!(REG32(RESETS_BASE + RESETS_RESET_DONE_OFFSET) & spi_reset_bits)) {
  }

  REG32(spi_base + SPI_SSPCR1_OFFSET) = 0U;
  REG32(spi_base + SPI_SSPCPSR_OFFSET) = SPI_SSPCPSR_1MHZ_AT_12MHZ_CLK_PERI;
  REG32(spi_base + SPI_SSPCR0_OFFSET) = SPI_SSPCR0_SCR(SPI_SSPCR0_SCR_DIV3) | SPI_SSPCR0_DSS_8_BITS;
  REG32(spi_base + SPI_SSPDMACR_OFFSET) = 0U;
  REG32(spi_base + SPI_SSPICR_OFFSET) = 0x3U;
  REG32(spi_base + SPI_SSPCR1_OFFSET) = SPI_SSPCR1_SSE_BITS;

  prv_gpio_set_function(prv_config()->miso_gpio, GPIO_FUNC_SPI);
  prv_gpio_set_function(prv_config()->sck_gpio, GPIO_FUNC_SPI);
  prv_gpio_set_function(prv_config()->mosi_gpio, GPIO_FUNC_SPI);
}

void fruitjam_lcd_init_pins(void) {
  s_use_hardware_spi = false;
  const Rp2350MemoryLcdConfig *config = prv_config();

  prv_gpio_output_init(config->lcd_cs_gpio, false);
  if (config->ram_cs_gpio != FRUITJAM_LCD_PIN_UNUSED) {
    prv_gpio_output_init(config->ram_cs_gpio, true);
  }
  if (config->inactive_cs_gpio != FRUITJAM_LCD_PIN_UNUSED) {
    prv_gpio_output_init(config->inactive_cs_gpio, true);
  }
  prv_gpio_output_init(config->sck_gpio, false);
  prv_gpio_output_init(config->mosi_gpio, false);
  prv_gpio_output_init(config->disp_gpio, false);
  prv_gpio_output_init(config->extcomin_gpio, false);
}

void fruitjam_lcd_use_hardware_spi(bool enabled) {
  if (enabled) {
    prv_hardware_spi_init();
    s_use_hardware_spi = true;
    return;
  }

  s_use_hardware_spi = false;
  prv_gpio_output_init(prv_config()->sck_gpio, false);
  prv_gpio_output_init(prv_config()->mosi_gpio, false);
}

void fruitjam_lcd_set_display_enabled(bool enabled) {
  prv_gpio_put(prv_config()->disp_gpio, enabled);
}

void fruitjam_lcd_set_vcom(bool state) {
  prv_gpio_put(prv_config()->extcomin_gpio, state);
}

void fruitjam_lcd_clear(void) {
  prv_transfer_begin();
  prv_select();
  prv_write_byte(LCD_CMD_CLEAR);
  prv_write_byte(0x00U);
  prv_deselect();
  prv_transfer_end();
  prv_capture_clear();
}

void fruitjam_lcd_begin_write(void) {
  prv_transfer_begin();
  prv_select();
  prv_write_byte(LCD_CMD_WRITE);
}

void fruitjam_lcd_write_row_msb(uint16_t y, const uint8_t *row) {
  uint8_t packet[1U + FRUITJAM_LCD_LINE_BYTES + 1U];

  prv_capture_row(y, row);

  packet[0] = (uint8_t)reverse_byte((uint8_t)(y + 1U));
  memcpy(&packet[1], row, FRUITJAM_LCD_LINE_BYTES);
  packet[1U + FRUITJAM_LCD_LINE_BYTES] = 0x00U;
  prv_write_bytes(packet, sizeof(packet));
  ++s_current_transfer_rows;
}

void fruitjam_lcd_end_write(void) {
  prv_write_byte(0x00U);
  prv_deselect();
  prv_transfer_end();
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

bool fruitjam_lcd_is_hardware_spi_enabled(void) {
  return s_use_hardware_spi;
}

uint32_t fruitjam_lcd_spi_index(void) {
  return prv_config()->spi_index;
}

uint32_t fruitjam_lcd_spi_cr0(void) {
  return REG32(prv_spi_base() + SPI_SSPCR0_OFFSET);
}

uint32_t fruitjam_lcd_spi_cpsr(void) {
  return REG32(prv_spi_base() + SPI_SSPCPSR_OFFSET);
}

uint32_t fruitjam_lcd_spi_status(void) {
  return REG32(prv_spi_base() + SPI_SSPSR_OFFSET);
}

uint32_t fruitjam_lcd_clk_peri_ctrl(void) {
  return rp2350_clk_peri_ctrl();
}

uint32_t fruitjam_lcd_clk_peri_div(void) {
  return rp2350_clk_peri_div();
}

uint32_t fruitjam_lcd_spi_hz(void) {
  const uint32_t cpsr = fruitjam_lcd_spi_cpsr() & 0xffU;
  const uint32_t scr = (fruitjam_lcd_spi_cr0() >> 8U) & 0xffU;
  const uint32_t peri_hz = rp2350_clk_peri_source_hz();

  if (peri_hz == 0U || cpsr == 0U) {
    return 0U;
  }

  return peri_hz / (cpsr * (scr + 1U));
}

uint32_t fruitjam_lcd_transfer_count(void) {
  return s_transfer_count;
}

uint32_t fruitjam_lcd_last_transfer_us(void) {
  return s_last_transfer_us;
}

uint32_t fruitjam_lcd_max_transfer_us(void) {
  return s_max_transfer_us;
}

uint16_t fruitjam_lcd_last_transfer_rows(void) {
  return s_last_transfer_rows;
}
