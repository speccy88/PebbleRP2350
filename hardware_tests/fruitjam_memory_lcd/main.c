#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/regs/qmi.h"
#include "hardware/regs/xip.h"
#include "hardware/spi.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/sync.h"
#include "pico/bootrom.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/time.h"

enum {
  LCD_WIDTH = 144,
  LCD_HEIGHT = 168,
  LCD_BYTES_PER_LINE = LCD_WIDTH / 8,
  LCD_FRAME_BYTES = LCD_BYTES_PER_LINE * LCD_HEIGHT,
};

enum {
  PIN_MISO = 28,
  PIN_SCK = 30,
  PIN_MOSI = 31,
  PIN_LCD_CS = 6,
  PIN_RAM_CS = 41,
  PIN_DISP = 7,
  PIN_EIN = 10,
  PIN_ESP_CS = 46,
  PIN_LED = 29,
  PIN_BUTTON_BACK = 0,
  PIN_BUTTON_UP = 4,
  PIN_BUTTON_SELECT = 5,
  PIN_PSRAM_CS = 47,
};

enum {
  BUTTON_BACK = 1u << 0,
  BUTTON_UP = 1u << 1,
  BUTTON_SELECT = 1u << 2,
  BUTTON_ALL = BUTTON_BACK | BUTTON_UP | BUTTON_SELECT,
};

enum {
  PSRAM_BASE = 0x11000000u,
  PSRAM_NOCACHE_BASE = 0x15000000u,
};

typedef struct {
  uint32_t last_mask;
  uint32_t all_hold_started_ms;
  bool all_holding;
} input_state_t;

static spi_inst_t *const lcd_spi = spi1;
static const uint32_t spi_baud_hz = 500 * 1000;

static uint8_t frame[LCD_FRAME_BYTES];
static volatile bool vcom_state;

static uint8_t reverse_byte(uint8_t value) {
  value = ((value & 0xf0u) >> 4) | ((value & 0x0fu) << 4);
  value = ((value & 0xccu) >> 2) | ((value & 0x33u) << 2);
  value = ((value & 0xaau) >> 1) | ((value & 0x55u) << 1);
  return value;
}

static void gpio_output_init(uint pin, bool value) {
  gpio_init(pin);
  gpio_put(pin, value);
  gpio_set_dir(pin, GPIO_OUT);
}

static void gpio_input_pullup_init(uint pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
}

static bool vcom_timer_callback(struct repeating_timer *timer) {
  (void)timer;
  vcom_state = !vcom_state;
  gpio_put(PIN_EIN, vcom_state);
  return true;
}

static void lcd_select(void) {
  gpio_put(PIN_LCD_CS, 1);
  busy_wait_us_32(3);
}

static void lcd_deselect(void) {
  busy_wait_us_32(1);
  gpio_put(PIN_LCD_CS, 0);
  busy_wait_us_32(1);
}

static void memory_lcd_clear(void) {
  static const uint8_t cmd_all_clear = 0x20;
  static const uint8_t padding = 0x00;

  lcd_select();
  spi_write_blocking(lcd_spi, &cmd_all_clear, 1);
  spi_write_blocking(lcd_spi, &padding, 1);
  lcd_deselect();
}

static void memory_lcd_write_frame(const uint8_t *pixels) {
  static const uint8_t cmd_write = 0x80;
  static const uint8_t padding = 0x00;

  lcd_select();
  spi_write_blocking(lcd_spi, &cmd_write, 1);

  for (uint y = 0; y < LCD_HEIGHT; ++y) {
    uint8_t address = reverse_byte((uint8_t)(y + 1));
    spi_write_blocking(lcd_spi, &address, 1);
    spi_write_blocking(lcd_spi, pixels + (y * LCD_BYTES_PER_LINE),
                       LCD_BYTES_PER_LINE);
    spi_write_blocking(lcd_spi, &padding, 1);
  }

  spi_write_blocking(lcd_spi, &padding, 1);
  lcd_deselect();
}

static void framebuffer_clear(bool black) {
  memset(frame, black ? 0x00 : 0xff, sizeof(frame));
}

static void set_pixel(int x, int y, bool black) {
  if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) {
    return;
  }

  uint8_t *byte = &frame[(y * LCD_BYTES_PER_LINE) + (x / 8)];
  const uint8_t mask = (uint8_t)(0x80u >> (x % 8));
  if (black) {
    *byte &= (uint8_t)~mask;
  } else {
    *byte |= mask;
  }
}

static void fill_rect(int x, int y, int w, int h, bool black) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      set_pixel(xx, yy, black);
    }
  }
}

static void draw_border(void) {
  for (int i = 0; i < 2; ++i) {
    for (int x = i; x < LCD_WIDTH - i; ++x) {
      set_pixel(x, i, true);
      set_pixel(x, LCD_HEIGHT - 1 - i, true);
    }
    for (int y = i; y < LCD_HEIGHT - i; ++y) {
      set_pixel(i, y, true);
      set_pixel(LCD_WIDTH - 1 - i, y, true);
    }
  }
}

static const char *glyph_rows(char c) {
  switch (c) {
    case '0':
      return "01110"
             "10001"
             "10011"
             "10101"
             "11001"
             "10001"
             "01110";
    case '1':
      return "00100"
             "01100"
             "00100"
             "00100"
             "00100"
             "00100"
             "01110";
    case '2':
      return "01110"
             "10001"
             "00001"
             "00010"
             "00100"
             "01000"
             "11111";
    case '3':
      return "11110"
             "00001"
             "00001"
             "01110"
             "00001"
             "00001"
             "11110";
    case '4':
      return "00010"
             "00110"
             "01010"
             "10010"
             "11111"
             "00010"
             "00010";
    case '5':
      return "11111"
             "10000"
             "10000"
             "11110"
             "00001"
             "00001"
             "11110";
    case '6':
      return "01110"
             "10000"
             "10000"
             "11110"
             "10001"
             "10001"
             "01110";
    case '7':
      return "11111"
             "00001"
             "00010"
             "00100"
             "01000"
             "01000"
             "01000";
    case '8':
      return "01110"
             "10001"
             "10001"
             "01110"
             "10001"
             "10001"
             "01110";
    case '9':
      return "01110"
             "10001"
             "10001"
             "01111"
             "00001"
             "00001"
             "01110";
    case 'A':
      return "01110"
             "10001"
             "10001"
             "11111"
             "10001"
             "10001"
             "10001";
    case 'B':
      return "11110"
             "10001"
             "10001"
             "11110"
             "10001"
             "10001"
             "11110";
    case 'C':
      return "01111"
             "10000"
             "10000"
             "10000"
             "10000"
             "10000"
             "01111";
    case 'D':
      return "11110"
             "10001"
             "10001"
             "10001"
             "10001"
             "10001"
             "11110";
    case 'E':
      return "11111"
             "10000"
             "10000"
             "11110"
             "10000"
             "10000"
             "11111";
    case 'F':
      return "11111"
             "10000"
             "10000"
             "11110"
             "10000"
             "10000"
             "10000";
    case 'G':
      return "01111"
             "10000"
             "10000"
             "10011"
             "10001"
             "10001"
             "01111";
    case 'H':
      return "10001"
             "10001"
             "10001"
             "11111"
             "10001"
             "10001"
             "10001";
    case 'I':
      return "11111"
             "00100"
             "00100"
             "00100"
             "00100"
             "00100"
             "11111";
    case 'J':
      return "00111"
             "00010"
             "00010"
             "00010"
             "00010"
             "10010"
             "01100";
    case 'K':
      return "10001"
             "10010"
             "10100"
             "11000"
             "10100"
             "10010"
             "10001";
    case 'L':
      return "10000"
             "10000"
             "10000"
             "10000"
             "10000"
             "10000"
             "11111";
    case 'M':
      return "10001"
             "11011"
             "10101"
             "10101"
             "10001"
             "10001"
             "10001";
    case 'N':
      return "10001"
             "11001"
             "10101"
             "10011"
             "10001"
             "10001"
             "10001";
    case 'O':
      return "01110"
             "10001"
             "10001"
             "10001"
             "10001"
             "10001"
             "01110";
    case 'P':
      return "11110"
             "10001"
             "10001"
             "11110"
             "10000"
             "10000"
             "10000";
    case 'R':
      return "11110"
             "10001"
             "10001"
             "11110"
             "10100"
             "10010"
             "10001";
    case 'S':
      return "01111"
             "10000"
             "10000"
             "01110"
             "00001"
             "00001"
             "11110";
    case 'T':
      return "11111"
             "00100"
             "00100"
             "00100"
             "00100"
             "00100"
             "00100";
    case 'U':
      return "10001"
             "10001"
             "10001"
             "10001"
             "10001"
             "10001"
             "01110";
    case 'W':
      return "10001"
             "10001"
             "10001"
             "10101"
             "10101"
             "10101"
             "01010";
    default:
      return NULL;
  }
}

static void draw_char_scaled(int x, int y, char c, int scale, bool black) {
  if (c == ' ') {
    return;
  }

  const char *rows = glyph_rows(c);
  if (!rows) {
    return;
  }

  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if (rows[(row * 5) + col] == '1') {
        fill_rect(x + (col * scale), y + (row * scale), scale, scale, black);
      }
    }
  }
}

static void draw_text_scaled(int x, int y, const char *text, int scale, bool black) {
  while (*text) {
    draw_char_scaled(x, y, *text, scale, black);
    x += 6 * scale;
    ++text;
  }
}

static int text_width(const char *text, int scale) {
  return (int)strlen(text) * 6 * scale;
}

static int centered_text_x(const char *text, int scale) {
  return (LCD_WIDTH - text_width(text, scale)) / 2;
}

static void draw_checkerboard(void) {
  for (int y = 8; y < LCD_HEIGHT - 8; ++y) {
    for (int x = 8; x < LCD_WIDTH - 8; ++x) {
      if ((((x / 8) + (y / 8)) & 1) == 0) {
        set_pixel(x, y, true);
      }
    }
  }
}

static void draw_diagonal(void) {
  for (int y = 0; y < LCD_HEIGHT; ++y) {
    int x = (y * (LCD_WIDTH - 1)) / (LCD_HEIGHT - 1);
    set_pixel(x, y, true);
    set_pixel(x + 1, y, true);
    set_pixel(x, y + 1, true);
  }
}

static const char *input_label(uint32_t mask) {
  if (mask == BUTTON_ALL) {
    return "UF2 HOLD";
  }
  if ((mask & (BUTTON_UP | BUTTON_SELECT)) == (BUTTON_UP | BUTTON_SELECT)) {
    return "DOWN";
  }
  if (mask == BUTTON_BACK) {
    return "BACK";
  }
  if (mask == BUTTON_UP) {
    return "UP";
  }
  if (mask == BUTTON_SELECT) {
    return "SELECT";
  }
  if (mask != 0) {
    return "BTNS";
  }
  return "IDLE";
}

static void draw_psram_status(size_t psram_size) {
  const char *status = "NO PSRAM";
  if (psram_size >= 8u * 1024u * 1024u) {
    status = "PSRAM 8M OK";
  } else if (psram_size >= 4u * 1024u * 1024u) {
    status = "PSRAM 4M OK";
  } else if (psram_size >= 2u * 1024u * 1024u) {
    status = "PSRAM 2M OK";
  }

  draw_text_scaled(centered_text_x(status, 1), 132, status, 1, true);
}

static void build_test_pattern(uint32_t button_mask, size_t psram_size) {
  const char *label = input_label(button_mask);

  framebuffer_clear(false);
  draw_checkerboard();
  draw_diagonal();

  fill_rect(12, 56, 120, 47, false);
  draw_text_scaled(18, 62, "FRUIT JAM", 2, true);
  draw_text_scaled(30, 80, "MEM LCD", 2, true);

  fill_rect(4, 108, 136, 56, false);
  draw_text_scaled(centered_text_x(label, 2), 112, label, 2, true);
  draw_psram_status(psram_size);
  draw_text_scaled(9, 148, "B1 BACK B2 UP B3 SEL", 1, true);

  draw_border();
}

static void build_bootsel_screen(void) {
  framebuffer_clear(false);
  draw_border();
  draw_text_scaled(centered_text_x("UF2 MODE", 2), 54, "UF2 MODE", 2, true);
  draw_text_scaled(centered_text_x("REBOOTING", 2), 76, "REBOOTING", 2, true);
  draw_text_scaled(centered_text_x("HOLD ALL", 1), 110, "HOLD ALL", 1, true);
}

static void setup_buttons(void) {
  gpio_input_pullup_init(PIN_BUTTON_BACK);
  gpio_input_pullup_init(PIN_BUTTON_UP);
  gpio_input_pullup_init(PIN_BUTTON_SELECT);
}

static uint32_t read_button_mask(void) {
  uint32_t mask = 0;

  if (!gpio_get(PIN_BUTTON_BACK)) {
    mask |= BUTTON_BACK;
  }
  if (!gpio_get(PIN_BUTTON_UP)) {
    mask |= BUTTON_UP;
  }
  if (!gpio_get(PIN_BUTTON_SELECT)) {
    mask |= BUTTON_SELECT;
  }

  return mask;
}

static bool poll_buttons(input_state_t *state, uint32_t *mask_out,
                         bool *request_bootsel) {
  const uint32_t mask = read_button_mask();
  const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
  bool changed = mask != state->last_mask;

  if (changed) {
    printf("Buttons mask=0x%lx -> %s\n", (unsigned long)mask, input_label(mask));
    state->last_mask = mask;
  }

  if (mask == BUTTON_ALL) {
    if (!state->all_holding) {
      state->all_holding = true;
      state->all_hold_started_ms = now_ms;
    } else if ((uint32_t)(now_ms - state->all_hold_started_ms) >= 2000u) {
      *request_bootsel = true;
    }
  } else {
    state->all_holding = false;
  }

  *mask_out = mask;
  return changed;
}

// Adapted from the local wili8jam Fruit Jam PSRAM probe.
static size_t __no_inline_not_in_flash_func(setup_psram)(void) {
  gpio_set_function(PIN_PSRAM_CS, GPIO_FUNC_XIP_CS1);

  uint32_t save_irq_status = save_and_disable_interrupts();

  qmi_hw->direct_csr =
      30u << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
  while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
  }

  qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
  qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
                      QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB |
                      0xf5u;
  while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
  }
  (void)qmi_hw->direct_rx;
  qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

  qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
  uint8_t kgd = 0;
  uint8_t eid = 0;
  for (size_t i = 0; i < 12; i++) {
    qmi_hw->direct_tx = (i == 0) ? 0x9fu : 0xffu;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0) {
    }
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }
    if (i == 5) {
      kgd = (uint8_t)qmi_hw->direct_rx;
    } else if (i == 6) {
      eid = (uint8_t)qmi_hw->direct_rx;
    } else {
      (void)qmi_hw->direct_rx;
    }
  }
  qmi_hw->direct_csr &=
      ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

  if (kgd != 0x5d) {
    restore_interrupts(save_irq_status);
    return 0;
  }

  qmi_hw->direct_csr =
      30u << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
  while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
  }

  for (uint8_t cmd_i = 0; cmd_i < 4; cmd_i++) {
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    switch (cmd_i) {
      case 0:
        qmi_hw->direct_tx = 0x66u;
        break;
      case 1:
        qmi_hw->direct_tx = 0x99u;
        break;
      case 2:
        qmi_hw->direct_tx = 0x35u;
        break;
      default:
        qmi_hw->direct_tx = 0xc0u;
        break;
    }
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }
    qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    for (size_t j = 0; j < 20; j++) {
      __asm volatile("nop");
    }
    (void)qmi_hw->direct_rx;
  }
  qmi_hw->direct_csr &=
      ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

  uint32_t psram_clkdiv = (clock_get_hz(clk_sys) + 62999999u) / 63000000u;
  if (psram_clkdiv < 2) {
    psram_clkdiv = 2;
  }

  qmi_hw->m[1].timing =
      QMI_M0_TIMING_PAGEBREAK_VALUE_1024 << QMI_M0_TIMING_PAGEBREAK_LSB |
      3u << QMI_M0_TIMING_SELECT_HOLD_LSB |
      1u << QMI_M0_TIMING_COOLDOWN_LSB |
      2u << QMI_M0_TIMING_RXDELAY_LSB |
      29u << QMI_M0_TIMING_MAX_SELECT_LSB |
      12u << QMI_M0_TIMING_MIN_DESELECT_LSB |
      psram_clkdiv << QMI_M0_TIMING_CLKDIV_LSB;

  qmi_hw->m[1].rfmt =
      QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB |
      QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_RFMT_ADDR_WIDTH_LSB |
      QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB |
      QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_RFMT_DUMMY_WIDTH_LSB |
      QMI_M0_RFMT_DUMMY_LEN_VALUE_24 << QMI_M0_RFMT_DUMMY_LEN_LSB |
      QMI_M0_RFMT_DATA_WIDTH_VALUE_Q << QMI_M0_RFMT_DATA_WIDTH_LSB |
      QMI_M0_RFMT_PREFIX_LEN_VALUE_8 << QMI_M0_RFMT_PREFIX_LEN_LSB |
      QMI_M0_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M0_RFMT_SUFFIX_LEN_LSB;

  qmi_hw->m[1].rcmd = 0xebu << QMI_M0_RCMD_PREFIX_LSB |
                      0u << QMI_M0_RCMD_SUFFIX_LSB;

  qmi_hw->m[1].wfmt =
      QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB |
      QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_WFMT_ADDR_WIDTH_LSB |
      QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB |
      QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_WFMT_DUMMY_WIDTH_LSB |
      QMI_M0_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M0_WFMT_DUMMY_LEN_LSB |
      QMI_M0_WFMT_DATA_WIDTH_VALUE_Q << QMI_M0_WFMT_DATA_WIDTH_LSB |
      QMI_M0_WFMT_PREFIX_LEN_VALUE_8 << QMI_M0_WFMT_PREFIX_LEN_LSB |
      QMI_M0_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M0_WFMT_SUFFIX_LEN_LSB;

  qmi_hw->m[1].wcmd = 0x38u << QMI_M0_WCMD_PREFIX_LSB |
                      0u << QMI_M0_WCMD_SUFFIX_LSB;

  restore_interrupts(save_irq_status);

  size_t psram_size = 1024u * 1024u;
  uint8_t size_id = eid >> 5;
  if (eid == 0x26 || size_id == 2) {
    psram_size *= 8u;
  } else if (size_id == 0) {
    psram_size *= 2u;
  } else if (size_id == 1) {
    psram_size *= 4u;
  }

  xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;

  volatile uint32_t *psram_nocache =
      (volatile uint32_t *)(uintptr_t)PSRAM_NOCACHE_BASE;
  psram_nocache[0] = 0x12345678u;
  if (psram_nocache[0] != 0x12345678u) {
    return 0;
  }

  if (psram_size >= sizeof(uint32_t)) {
    size_t last_word = (psram_size / sizeof(uint32_t)) - 1u;
    psram_nocache[last_word] = 0xa5a55a5au;
    if (psram_nocache[last_word] != 0xa5a55a5au) {
      return 0;
    }
  }

  return psram_size;
}

static void hardware_init(void) {
  gpio_output_init(PIN_LCD_CS, false);
  gpio_output_init(PIN_RAM_CS, true);
  gpio_output_init(PIN_DISP, false);
  gpio_output_init(PIN_EIN, false);
  gpio_output_init(PIN_ESP_CS, true);
  gpio_output_init(PIN_LED, true);
  setup_buttons();

  uint actual_baud = spi_init(lcd_spi, spi_baud_hz);
  spi_set_format(lcd_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

  sleep_us(800);
  gpio_put(PIN_DISP, true);
  sleep_us(40);
  gpio_put(PIN_EIN, false);
  sleep_us(40);

  printf("SPI requested %lu Hz, actual %u Hz\n", spi_baud_hz, actual_baud);
  printf("LCD_CS=%u RAM_CS=%u DISP=%u EIN=%u\n", PIN_LCD_CS, PIN_RAM_CS,
         PIN_DISP, PIN_EIN);
  printf("Buttons: back=%u up=%u select=%u\n", PIN_BUTTON_BACK, PIN_BUTTON_UP,
         PIN_BUTTON_SELECT);
}

static void run_screen_for_ms(bool show_pattern, uint32_t duration_ms,
                              input_state_t *input, size_t psram_size) {
  absolute_time_t deadline = make_timeout_time_ms(duration_ms);
  uint32_t button_mask = read_button_mask();
  bool request_bootsel = false;

  if (show_pattern) {
    build_test_pattern(button_mask, psram_size);
    memory_lcd_write_frame(frame);
    printf("Showing test pattern\n");
  } else {
    framebuffer_clear(false);
    memory_lcd_clear();
    printf("Showing all white\n");
  }

  while (!time_reached(deadline)) {
    if (poll_buttons(input, &button_mask, &request_bootsel) && show_pattern) {
      build_test_pattern(button_mask, psram_size);
      memory_lcd_write_frame(frame);
    }

    if (request_bootsel) {
      printf("All buttons held; entering USB bootloader\n");
      build_bootsel_screen();
      memory_lcd_write_frame(frame);
      sleep_ms(250);
      reset_usb_boot(0, 0);
    }

    gpio_put(PIN_LED, (to_ms_since_boot(get_absolute_time()) / 500u) & 1u);
    sleep_ms(20);
  }
}

int main(void) {
  stdio_init_all();
  sleep_ms(1500);

  printf("Fruit Jam Waveshare 1.3inch Memory LCD bring-up\n");
  hardware_init();

  size_t psram_size = setup_psram();
  printf("PSRAM detected: %lu bytes\n", (unsigned long)psram_size);

  struct repeating_timer vcom_timer;
  add_repeating_timer_ms(500, vcom_timer_callback, NULL, &vcom_timer);

  memory_lcd_clear();
  printf("Display cleared\n");

  input_state_t input = {
      .last_mask = read_button_mask(),
  };

  while (true) {
    run_screen_for_ms(true, 3200, &input, psram_size);
    run_screen_for_ms(false, 2200, &input, psram_size);
  }
}
