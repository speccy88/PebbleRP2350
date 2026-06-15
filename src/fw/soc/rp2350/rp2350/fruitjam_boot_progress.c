/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_boot_progress.h"

#include "board/splash.h"
#include "soc/rp2350/rp2350/fruitjam_lcd.h"
#include "system/reboot_reason.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define PROGRESS_BAR_WIDTH 96U
#define PROGRESS_BAR_HEIGHT 4U
#define PROGRESS_INDICATOR_WIDTH 12U
#define STAGE_TEXT_SCALE 2U
#define STAGE_TEXT_GLYPH_WIDTH 3U
#define STAGE_TEXT_GLYPH_HEIGHT 5U
#define STAGE_TEXT_ADVANCE 4U
#define STAGE_TEXT_Y 132U
#define NORMAL_STAGE_HOLD_CYCLES 6000000U
#define ERROR_STAGE_HOLD_CYCLES 90000000U
#define LAST_LABEL_SIZE 16U

static bool s_vcom_state;
static volatile FruitJamBootProgressStage s_last_stage = FruitJamBootProgressStageEarly;
static volatile uint32_t s_progress_sequence;
static char s_last_label[LAST_LABEL_SIZE] = "00 EARLY";

typedef struct FruitJamBootProgressFrame {
  FruitJamBootProgressStage stage;
  const char *label;
} FruitJamBootProgressFrame;

static bool prv_stage_is_error(FruitJamBootProgressStage stage) {
  return stage >= FruitJamBootProgressStageCount;
}

static const char *prv_stage_label(FruitJamBootProgressStage stage) {
  if (stage == FruitJamBootProgressStageResourceError) {
    return "ERR RES";
  }

  switch (stage) {
    case FruitJamBootProgressStageEarly:
      return "00 EARLY";
    case FruitJamBootProgressStagePfsStart:
      return "01 PFS";
    case FruitJamBootProgressStagePfsDone:
      return "02 PFS OK";
    case FruitJamBootProgressStageDriversStart:
      return "03 DRV";
    case FruitJamBootProgressStageDriversDone:
      return "04 DRV OK";
    case FruitJamBootProgressStageResourcesStart:
      return "05 RES";
    case FruitJamBootProgressStageResourcesDone:
      return "06 RES OK";
    case FruitJamBootProgressStageDisplayInit:
      return "07 LCD";
    case FruitJamBootProgressStageDriverReady:
      return "08 DISP";
    case FruitJamBootProgressStageCompositorStart:
      return "09 COMP";
    case FruitJamBootProgressStageCompositorDone:
      return "10 COMP OK";
    case FruitJamBootProgressStageBluetoothInitStart:
      return "11 BT INIT";
    case FruitJamBootProgressStageBluetoothInitDone:
      return "12 BTI OK";
    case FruitJamBootProgressStageServicesStart:
      return "13 SVC";
    case FruitJamBootProgressStageServicesDone:
      return "14 SVC OK";
    case FruitJamBootProgressStageBluetoothStart:
      return "15 BT START";
    case FruitJamBootProgressStageBluetoothUnavailable:
      return "16 BT NO HCI";
    case FruitJamBootProgressStageBluetoothDone:
      return "17 BT OK";
    case FruitJamBootProgressStageLauncherReady:
      return "18 LAUNCH";
    case FruitJamBootProgressStageCount:
      break;
    case FruitJamBootProgressStageFaultReset:
      return "ERR RESET";
    case FruitJamBootProgressStageFaultAssert:
      return "ERR ASSERT";
    case FruitJamBootProgressStageFaultHard:
      return "ERR HARD";
    case FruitJamBootProgressStageFaultStack:
      return "ERR STACK";
    case FruitJamBootProgressStageFaultOom:
      return "ERR OOM";
    case FruitJamBootProgressStageFaultWatchdog:
      return "ERR WATCH";
    case FruitJamBootProgressStageFaultCoreDump:
      return "ERR CORE";
  }

  return "??";
}

static const char *prv_frame_label(const FruitJamBootProgressFrame *frame) {
  return frame->label ? frame->label : prv_stage_label(frame->stage);
}

static void prv_remember_progress(FruitJamBootProgressStage stage, const char *label) {
  const char *source = label ? label : prv_stage_label(stage);
  uint32_t i = 0;

  s_last_stage = stage;
  while (source[i] != '\0' && i < LAST_LABEL_SIZE - 1U) {
    s_last_label[i] = source[i];
    ++i;
  }
  s_last_label[i] = '\0';
  ++s_progress_sequence;
}

static FruitJamBootProgressStage prv_stage_from_reboot_reason(uint8_t reason_code) {
  switch ((RebootReasonCode)reason_code) {
    case RebootReasonCode_Assert:
      return FruitJamBootProgressStageFaultAssert;
    case RebootReasonCode_StackOverflow:
      return FruitJamBootProgressStageFaultStack;
    case RebootReasonCode_HardFault:
    case RebootReasonCode_AppHardFault:
    case RebootReasonCode_WorkerHardFault:
      return FruitJamBootProgressStageFaultHard;
    case RebootReasonCode_OutOfMemory:
      return FruitJamBootProgressStageFaultOom;
    case RebootReasonCode_Watchdog:
      return FruitJamBootProgressStageFaultWatchdog;
    case RebootReasonCode_CoreDump:
    case RebootReasonCode_CoreDumpEntryFailed:
      return FruitJamBootProgressStageFaultCoreDump;
    default:
      return FruitJamBootProgressStageFaultReset;
  }
}

static char prv_hex_digit(uint8_t value) {
  value &= 0x0fU;
  return (value < 10U) ? (char)('0' + value) : (char)('A' + value - 10U);
}

static void prv_format_reason_code_label(char label[7], uint8_t reason_code) {
  label[0] = 'R';
  label[1] = 'S';
  label[2] = 'T';
  label[3] = ' ';
  label[4] = prv_hex_digit(reason_code >> 4U);
  label[5] = prv_hex_digit(reason_code);
  label[6] = '\0';
}

static void prv_format_u32_label(char label[12], char prefix0, char prefix1, uint32_t value) {
  label[0] = prefix0;
  label[1] = prefix1;
  label[2] = ' ';

  for (uint32_t i = 0; i < 8U; ++i) {
    const uint32_t shift = 28U - i * 4U;
    label[3U + i] = prv_hex_digit((uint8_t)(value >> shift));
  }

  label[11] = '\0';
}

static uint16_t prv_glyph_pattern(char c) {
  switch (c) {
    case '0':
      return 0x7b6fU;
    case '1':
      return 0x5d27U;
    case '2':
      return 0x73e7U;
    case '3':
      return 0x73cfU;
    case '4':
      return 0x5bc9U;
    case '5':
      return 0x79cfU;
    case '6':
      return 0x79efU;
    case '7':
      return 0x72a4U;
    case '8':
      return 0x7befU;
    case '9':
      return 0x7bcfU;
    case 'A':
      return 0x2f6dU;
    case 'B':
      return 0x6badU;
    case 'C':
      return 0x7247U;
    case 'D':
      return 0x6b6dU;
    case 'E':
      return 0x72c7U;
    case 'F':
      return 0x72c4U;
    case 'G':
      return 0x7257U;
    case 'H':
      return 0x5bedU;
    case 'I':
      return 0x7497U;
    case 'J':
      return 0x2497U;
    case 'K':
      return 0x5b6dU;
    case 'L':
      return 0x4927U;
    case 'M':
      return 0x5ffdU;
    case 'N':
      return 0x5ffdU;
    case 'O':
      return 0x7b6fU;
    case 'P':
      return 0x6bc8U;
    case 'Q':
      return 0x7b7fU;
    case 'R':
      return 0x6bedU;
    case 'S':
      return 0x79cfU;
    case 'T':
      return 0x7492U;
    case 'U':
      return 0x5b6fU;
    case 'V':
      return 0x5b6aU;
    case 'W':
      return 0x5bffU;
    case 'X':
      return 0x5aadU;
    case 'Y':
      return 0x5a92U;
    case 'Z':
      return 0x72a7U;
    case '?':
      return 0x72c2U;
    default:
      return 0U;
  }
}

static uint16_t prv_label_width(const char *label) {
  uint16_t length = 0U;

  while (label[length] != '\0') {
    ++length;
  }

  if (length == 0U) {
    return 0U;
  }

  return (uint16_t)((length * STAGE_TEXT_ADVANCE - 1U) * STAGE_TEXT_SCALE);
}

static bool prv_logo_pixel(uint16_t x, uint16_t y) {
  const uint16_t logo_x0 = (FRUITJAM_LCD_WIDTH - splash_width) / 2U;
  const uint16_t logo_y0 = (FRUITJAM_LCD_HEIGHT - splash_height) / 2U;

  if (x < logo_x0 || y < logo_y0 ||
      x >= (logo_x0 + splash_width) ||
      y >= (logo_y0 + splash_height)) {
    return false;
  }

  const uint16_t logo_x = (uint16_t)(x - logo_x0);
  const uint16_t logo_y = (uint16_t)(y - logo_y0);
  const uint16_t logo_row_bytes = splash_width / 8U;
  return (splash_bits[logo_y * logo_row_bytes + logo_x / 8U] &
          (uint8_t)(1U << (logo_x & 7U))) != 0U;
}

static uint16_t prv_indicator_offset(FruitJamBootProgressStage stage) {
  if (stage >= FruitJamBootProgressStageCount) {
    return 0U;
  }

  const uint32_t max_offset = PROGRESS_BAR_WIDTH - PROGRESS_INDICATOR_WIDTH;
  const uint32_t denominator = FruitJamBootProgressStageCount - 1U;
  return (uint16_t)(((uint32_t)stage * max_offset) / denominator);
}

static bool prv_progress_pixel(uint16_t x, uint16_t y, FruitJamBootProgressStage stage) {
  const uint16_t bar_x0 = (FRUITJAM_LCD_WIDTH - PROGRESS_BAR_WIDTH) / 2U;
  const uint16_t bar_y0 =
      (FRUITJAM_LCD_HEIGHT - splash_height) / 2U + splash_height + 20U -
      (PROGRESS_BAR_HEIGHT / 2U);

  if (x < bar_x0 || y < bar_y0 ||
      x >= (bar_x0 + PROGRESS_BAR_WIDTH) ||
      y >= (bar_y0 + PROGRESS_BAR_HEIGHT)) {
    return false;
  }

  if (prv_stage_is_error(stage)) {
    return true;
  }

  const uint16_t indicator_x0 = bar_x0 + prv_indicator_offset(stage);
  if (x >= indicator_x0 && x < (indicator_x0 + PROGRESS_INDICATOR_WIDTH)) {
    return true;
  }

  return ((x ^ y) & 1U) == 0U;
}

static bool prv_stage_text_pixel(uint16_t x, uint16_t y,
                                 const FruitJamBootProgressFrame *frame) {
  const char *label = prv_frame_label(frame);
  const uint16_t label_width = prv_label_width(label);

  if (label_width == 0U || y < STAGE_TEXT_Y ||
      y >= STAGE_TEXT_Y + STAGE_TEXT_GLYPH_HEIGHT * STAGE_TEXT_SCALE) {
    return false;
  }

  const uint16_t text_x0 = (FRUITJAM_LCD_WIDTH - label_width) / 2U;
  if (x < text_x0 || x >= text_x0 + label_width) {
    return false;
  }

  const uint16_t local_x = (uint16_t)(x - text_x0);
  const uint16_t char_index = local_x / (STAGE_TEXT_ADVANCE * STAGE_TEXT_SCALE);
  const uint16_t char_x =
      (local_x % (STAGE_TEXT_ADVANCE * STAGE_TEXT_SCALE)) / STAGE_TEXT_SCALE;
  const uint16_t char_y = (uint16_t)((y - STAGE_TEXT_Y) / STAGE_TEXT_SCALE);

  if (char_x >= STAGE_TEXT_GLYPH_WIDTH) {
    return false;
  }

  const uint16_t pattern = prv_glyph_pattern(label[char_index]);
  const uint8_t row_bits = (uint8_t)((pattern >> ((STAGE_TEXT_GLYPH_HEIGHT - 1U - char_y) *
                                                  STAGE_TEXT_GLYPH_WIDTH)) &
                                     0x7U);
  return (row_bits & (uint8_t)(1U << (STAGE_TEXT_GLYPH_WIDTH - 1U - char_x))) != 0U;
}

static uint8_t prv_progress_byte(uint16_t y, uint16_t byte_x, void *context) {
  const FruitJamBootProgressFrame *frame = context;
  uint8_t value = 0xffU;

  for (uint8_t bit = 0; bit < 8U; ++bit) {
    const uint16_t x = (uint16_t)(byte_x * 8U + bit);
    if (prv_logo_pixel(x, y) || prv_progress_pixel(x, y, frame->stage) ||
        prv_stage_text_pixel(x, y, frame)) {
      value &= (uint8_t)~(0x80U >> bit);
    }
  }

  return value;
}

static void prv_write_frame_with_label(FruitJamBootProgressStage stage, const char *label) {
  FruitJamBootProgressFrame frame = {
    .stage = stage,
    .label = label,
  };
  prv_remember_progress(stage, label);
  fruitjam_lcd_write_generated_frame(prv_progress_byte, &frame);
  s_vcom_state = !s_vcom_state;
  fruitjam_lcd_set_vcom(s_vcom_state);
  fruitjam_lcd_delay(prv_stage_is_error(stage) ? ERROR_STAGE_HOLD_CYCLES :
                                                 NORMAL_STAGE_HOLD_CYCLES);
}

static void prv_show_with_label(FruitJamBootProgressStage stage, const char *label) {
  fruitjam_lcd_init_pins();
  fruitjam_lcd_delay(120000U);
  fruitjam_lcd_set_display_enabled(true);
  fruitjam_lcd_delay(6000U);
  prv_write_frame_with_label(stage, label);
}

void fruitjam_boot_progress_write_frame(FruitJamBootProgressStage stage) {
  prv_write_frame_with_label(stage, NULL);
}

void fruitjam_boot_progress_write_label(FruitJamBootProgressStage stage, const char *label) {
  prv_write_frame_with_label(stage, label);
}

void fruitjam_boot_progress_mark(FruitJamBootProgressStage stage) {
  prv_remember_progress(stage, NULL);
}

void fruitjam_boot_progress_mark_label(FruitJamBootProgressStage stage, const char *label) {
  prv_remember_progress(stage, label);
}

void fruitjam_boot_progress_show(FruitJamBootProgressStage stage) {
  prv_show_with_label(stage, NULL);
}

void fruitjam_boot_progress_show_label(FruitJamBootProgressStage stage, const char *label) {
  prv_show_with_label(stage, label);
}

void fruitjam_boot_progress_show_reboot_reason(uint8_t reason_code, uint32_t raw1,
                                               uint32_t raw2, uint32_t raw3) {
  const FruitJamBootProgressStage stage = prv_stage_from_reboot_reason(reason_code);
  char label[12];

  prv_format_reason_code_label(label, reason_code);
  prv_show_with_label(stage, label);
  prv_write_frame_with_label(stage, NULL);

  if ((RebootReasonCode)reason_code == RebootReasonCode_Watchdog) {
    if (raw1 != 0U) {
      prv_format_u32_label(label, 'P', 'C', raw1);
      prv_write_frame_with_label(stage, label);
    }
    if (raw2 != 0U) {
      prv_format_u32_label(label, 'L', 'R', raw2);
      prv_write_frame_with_label(stage, label);
    }
    if (raw3 != 0U) {
      prv_format_u32_label(label, 'C', 'B', raw3);
      prv_write_frame_with_label(stage, label);
    }
    return;
  }

  if (raw1 != 0U) {
    prv_format_u32_label(label, 'E', 'X', raw1);
    prv_write_frame_with_label(stage, label);
  }
}

FruitJamBootProgressStage fruitjam_boot_progress_last_stage(void) {
  return s_last_stage;
}

const char *fruitjam_boot_progress_last_label(void) {
  return s_last_label;
}

uint32_t fruitjam_boot_progress_sequence(void) {
  return s_progress_sequence;
}

const char *fruitjam_boot_progress_label(FruitJamBootProgressStage stage) {
  return prv_stage_label(stage);
}
