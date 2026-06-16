/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics_circle.h"
#include "applib/graphics/text.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/ui.h"
#include "applib/unobstructed_area_service.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/clock.h"
#include "pbl/services/i18n/i18n.h"
#include "util/time/time.h"

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  Window window;
#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
  Layer canvas_layer;
#else
  TextLayer text_date_layer;
  TextLayer text_time_layer;
  Layer line_layer;
#endif
  char time_text[6];
  char date_text[13];
} TicTocData;

#if defined(CONFIG_BOARD_FRUITJAM_RP2350)

#define FRUITJAM_LOGO_WIDTH 104
#define FRUITJAM_LOGO_HEIGHT 39
#define FRUITJAM_LOGO_STRIDE 13

static const uint8_t s_fruitjam_logo_bits[FRUITJAM_LOGO_HEIGHT][FRUITJAM_LOGO_STRIDE] = {
  {0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x03, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x03, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x03, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x7c, 0x7e, 0x00, 0x07, 0xff, 0x8f, 0xfe, 0x1f, 0x8f, 0x1f, 0x1f, 0xfe},
  {0x00, 0x7c, 0x7e, 0x00, 0x09, 0xff, 0xd3, 0xff, 0x17, 0xdf, 0xa7, 0xa7, 0xff},
  {0x07, 0xff, 0xe7, 0xc0, 0x0b, 0xff, 0x97, 0xff, 0x97, 0xdf, 0x2f, 0xaf, 0xff},
  {0x1f, 0xff, 0x00, 0xf0, 0x0f, 0xe0, 0x1f, 0xc7, 0x9f, 0xdf, 0xbf, 0x9f, 0xfe},
  {0x1f, 0xff, 0x00, 0xf0, 0x0f, 0xe0, 0x1f, 0xc7, 0x9f, 0xdf, 0x3f, 0x83, 0xf0},
  {0x3f, 0xff, 0x00, 0x3c, 0x0f, 0xff, 0x9f, 0xff, 0xbf, 0xdf, 0xbf, 0x83, 0xf0},
  {0x3f, 0xff, 0x00, 0x3c, 0x0f, 0xff, 0xdf, 0xff, 0x1f, 0xdf, 0xbf, 0x83, 0xf0},
  {0xff, 0xff, 0x00, 0x0e, 0x0f, 0xff, 0x9f, 0xfe, 0x1f, 0xdf, 0xbf, 0x83, 0xf0},
  {0xff, 0xff, 0x00, 0x0f, 0x0f, 0xe0, 0x1f, 0xff, 0x1f, 0xdf, 0x3f, 0x83, 0xf0},
  {0xff, 0xff, 0x00, 0x07, 0x0f, 0xe0, 0x1f, 0xff, 0xbf, 0xdf, 0xbf, 0x83, 0xf0},
  {0xff, 0xef, 0x18, 0x07, 0x0f, 0xe0, 0x1f, 0xdf, 0x9f, 0xff, 0x3f, 0x83, 0xf0},
  {0xff, 0xef, 0x18, 0x07, 0x0f, 0xe0, 0x1f, 0xcf, 0x9f, 0xff, 0xbf, 0x83, 0xf0},
  {0xff, 0xef, 0x18, 0x06, 0x0f, 0xe0, 0x1f, 0xcf, 0x9f, 0xff, 0x3f, 0x83, 0xf0},
  {0xff, 0xef, 0x18, 0x07, 0x07, 0xc0, 0x0f, 0x87, 0x07, 0xfc, 0x1f, 0x01, 0xe0},
  {0xff, 0xef, 0x18, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0xff, 0xef, 0x18, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0xff, 0xff, 0x00, 0x07, 0x00, 0x0f, 0x87, 0xfc, 0x1f, 0x80, 0x70, 0x00, 0x00},
  {0xff, 0xff, 0x00, 0x06, 0x00, 0x13, 0xcb, 0xfe, 0x17, 0xc0, 0xf8, 0x00, 0x00},
  {0xff, 0xff, 0x00, 0x0f, 0x00, 0x17, 0xd7, 0xff, 0x17, 0xe1, 0xf8, 0x00, 0x00},
  {0x3f, 0xff, 0x00, 0x0c, 0x00, 0x1f, 0xdf, 0xc7, 0x9f, 0xf3, 0xf8, 0x00, 0x00},
  {0x3f, 0xff, 0x00, 0x0c, 0x00, 0x1f, 0xdf, 0xc7, 0x9f, 0xff, 0xf8, 0x00, 0x00},
  {0x3f, 0xff, 0x00, 0x3c, 0x00, 0x1f, 0xdf, 0xff, 0x9f, 0xff, 0xf8, 0x00, 0x00},
  {0x3f, 0xff, 0x00, 0x3c, 0x07, 0x1f, 0xdf, 0xff, 0x9f, 0xff, 0xf8, 0x00, 0x00},
  {0x1f, 0xff, 0x00, 0xf0, 0x0f, 0x9f, 0xdf, 0xc7, 0x9f, 0xff, 0xf8, 0x00, 0x00},
  {0x1f, 0xff, 0x01, 0xf0, 0x0f, 0x9f, 0xdf, 0xc7, 0x9f, 0xde, 0xf8, 0x00, 0x00},
  {0x07, 0xff, 0x01, 0xc0, 0x0f, 0x9f, 0xdf, 0xc7, 0x9f, 0xcc, 0xf8, 0x00, 0x00},
  {0x01, 0xff, 0xe7, 0x00, 0x0f, 0xff, 0xdf, 0xc7, 0x9f, 0xc0, 0xf8, 0x00, 0x00},
  {0x01, 0xff, 0xe7, 0x00, 0x0f, 0xff, 0xdf, 0xc7, 0x9f, 0xc0, 0xf8, 0x00, 0x00},
  {0x00, 0x7c, 0x7e, 0x00, 0x07, 0xff, 0x9f, 0xc7, 0x9f, 0xc0, 0xf8, 0x00, 0x00},
  {0x00, 0x7c, 0x7e, 0x00, 0x03, 0xff, 0x0f, 0x83, 0x1f, 0x80, 0x70, 0x00, 0x00},
};

static void prv_draw_text(GContext *ctx, const char *text, GFont font, GColor color, GRect box,
                          GTextAlignment alignment) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeFill, alignment, NULL);
}

static void prv_draw_via(GContext *ctx, GPoint point, uint16_t radius) {
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_circle(ctx, point, radius);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, point, 1);
}

static void prv_draw_trace(GContext *ctx, GPoint start, GPoint end) {
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, start, end);
}

static void prv_draw_pad(GContext *ctx, GRect rect) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &rect);
  GRect hole = GRect(rect.origin.x + 2, rect.origin.y + 2, rect.size.w - 4, rect.size.h - 4);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &hole);
}

static void prv_draw_fruitjam_logo(GContext *ctx, GPoint origin) {
  graphics_context_set_fill_color(ctx, GColorBlack);

  for (int y = 0; y < FRUITJAM_LOGO_HEIGHT; ++y) {
    int run_start = -1;
    for (int x = 0; x <= FRUITJAM_LOGO_WIDTH; ++x) {
      const bool pixel_set =
          (x < FRUITJAM_LOGO_WIDTH) &&
          (s_fruitjam_logo_bits[y][x / 8] & (0x80 >> (x % 8)));
      if (pixel_set && run_start < 0) {
        run_start = x;
      } else if (!pixel_set && run_start >= 0) {
        graphics_fill_rect(ctx, &GRect(origin.x + run_start, origin.y + y,
                                       x - run_start, 1));
        run_start = -1;
      }
    }
  }
}

static void prv_draw_circuit_art(GContext *ctx) {
  graphics_context_set_antialiased(ctx, false);

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(2, 2, DISP_COLS - 4, DISP_ROWS - 4));
  graphics_draw_rect(ctx, &GRect(5, 5, DISP_COLS - 10, DISP_ROWS - 10));

  prv_draw_pad(ctx, GRect(9, 139, 10, 10));
  prv_draw_pad(ctx, GRect(125, 139, 10, 10));
  prv_draw_pad(ctx, GRect(9, 39, 8, 8));
  prv_draw_pad(ctx, GRect(127, 39, 8, 8));

  prv_draw_trace(ctx, GPoint(20, 43), GPoint(38, 43));
  prv_draw_trace(ctx, GPoint(38, 43), GPoint(38, 56));
  prv_draw_trace(ctx, GPoint(38, 56), GPoint(50, 56));
  prv_draw_via(ctx, GPoint(50, 56), 3);

  prv_draw_trace(ctx, GPoint(124, 43), GPoint(106, 43));
  prv_draw_trace(ctx, GPoint(106, 43), GPoint(106, 57));
  prv_draw_trace(ctx, GPoint(106, 57), GPoint(95, 57));
  prv_draw_via(ctx, GPoint(95, 57), 3);

  prv_draw_trace(ctx, GPoint(15, 144), GPoint(34, 144));
  prv_draw_trace(ctx, GPoint(34, 144), GPoint(34, 126));
  prv_draw_trace(ctx, GPoint(34, 126), GPoint(48, 126));
  prv_draw_via(ctx, GPoint(48, 126), 3);

  prv_draw_trace(ctx, GPoint(130, 144), GPoint(111, 144));
  prv_draw_trace(ctx, GPoint(111, 144), GPoint(111, 126));
  prv_draw_trace(ctx, GPoint(111, 126), GPoint(96, 126));
  prv_draw_via(ctx, GPoint(96, 126), 3);

  prv_draw_trace(ctx, GPoint(7, 116), GPoint(25, 116));
  prv_draw_trace(ctx, GPoint(25, 116), GPoint(25, 106));
  prv_draw_via(ctx, GPoint(25, 106), 2);

  prv_draw_trace(ctx, GPoint(137, 116), GPoint(119, 116));
  prv_draw_trace(ctx, GPoint(119, 116), GPoint(119, 106));
  prv_draw_via(ctx, GPoint(119, 106), 2);

  prv_draw_trace(ctx, GPoint(91, 19), GPoint(125, 19));
  prv_draw_trace(ctx, GPoint(125, 19), GPoint(125, 31));
  prv_draw_via(ctx, GPoint(125, 31), 3);

  for (int x = 59; x <= 84; x += 5) {
    prv_draw_trace(ctx, GPoint(x, 33), GPoint(x + 3, 33));
    prv_draw_trace(ctx, GPoint(x, 133), GPoint(x + 3, 133));
  }
}

static void prv_canvas_layer_update_proc(Layer *layer, GContext *ctx) {
  TicTocData *data = app_state_get_user_data();
  const GRect *bounds = &layer->bounds;

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds);

  prv_draw_circuit_art(ctx);
  prv_draw_fruitjam_logo(ctx, GPoint(19, 6));

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, &GRect(13, 52, 118, 47), 4);
  graphics_draw_round_rect(ctx, &GRect(16, 55, 112, 41), 3);

  const GFont time_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  prv_draw_text(ctx, data->time_text, time_font, GColorBlack, GRect(0, 50, DISP_COLS, 50),
                GTextAlignmentCenter);

  prv_draw_text(ctx, data->date_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GColorBlack,
                GRect(0, 101, DISP_COLS, 22), GTextAlignmentCenter);

  prv_draw_text(ctx, "RP2350  DS1307", fonts_get_system_font(FONT_KEY_GOTHIC_14), GColorBlack,
                GRect(0, 147, DISP_COLS, 16), GTextAlignmentCenter);
}

static void prv_update_layer_positions(void) {
}

static void prv_unobstructed_area_change_handler(AnimationProgress progress, void *context) {
  TicTocData *data = app_state_get_user_data();
  layer_mark_dirty(&data->canvas_layer);
}

static void prv_minute_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  TicTocData *data = app_state_get_user_data();

  strftime(data->time_text, sizeof(data->time_text),
           clock_is_24h_style() ? "%R" : "%I:%M", tick_time);
  if (!clock_is_24h_style() && (data->time_text[0] == '0')) {
    memmove(data->time_text, data->time_text + 1, strlen(data->time_text));
  }

  strftime(data->date_text, sizeof(data->date_text), "%b %e", tick_time);

  layer_mark_dirty(&data->canvas_layer);
}

#else

static void prv_line_layer_update_callback(Layer *me, GContext* ctx) {
  GRect bounds;
  layer_get_bounds(me, &bounds);
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(me, &unobstructed_bounds);

  // Calculate how much we need to shift up
  int16_t obstruction = bounds.size.h - unobstructed_bounds.size.h;
  int16_t extra_shift = obstruction > 0 ? 10 : 0;
  int16_t line_y = 97 - obstruction + extra_shift;

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(8, line_y), GPoint(131, line_y));
  graphics_draw_line(ctx, GPoint(8, line_y + 1), GPoint(131, line_y + 1));
}

static void prv_update_layer_positions(void) {
  TicTocData *data = app_state_get_user_data();
  Layer *window_layer = window_get_root_layer(&data->window);

  GRect bounds;
  layer_get_bounds(window_layer, &bounds);
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(window_layer, &unobstructed_bounds);

  // Calculate how much we need to shift up (maintains spacing between elements)
  int16_t obstruction = bounds.size.h - unobstructed_bounds.size.h;
  // Add extra shift when obstructed to reduce bottom padding
  int16_t extra_shift = obstruction > 0 ? 10 : 0;

  GRect date_frame;
  layer_get_frame(&data->text_date_layer.layer, &date_frame);
  date_frame.origin.y = 68 - obstruction + extra_shift;
  layer_set_frame(&data->text_date_layer.layer, &date_frame);

  GRect time_frame;
  layer_get_frame(&data->text_time_layer.layer, &time_frame);
  time_frame.origin.y = 92 - obstruction + extra_shift;
  layer_set_frame(&data->text_time_layer.layer, &time_frame);

  layer_mark_dirty(&data->line_layer);
}

static void prv_unobstructed_area_change_handler(AnimationProgress progress, void *context) {
  prv_update_layer_positions();
}

static void prv_minute_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  TicTocData *data = app_state_get_user_data();

  strftime(data->date_text, sizeof(data->date_text), i18n_get("%B %e", data), tick_time);
  text_layer_set_text(&data->text_date_layer, data->date_text);

  strftime(data->time_text, sizeof(data->time_text),
      clock_is_24h_style() ? "%R" : "%I:%M", tick_time);

  // Handle lack of non-padded hour format string for twelve hour clock.
  char *start_time_text = data->time_text;
  if (!clock_is_24h_style() && (data->time_text[0] == '0')) {
    start_time_text++;
  }

  text_layer_set_text(&data->text_time_layer, start_time_text);
}

#endif

static void prv_deinit(void) {
  TicTocData *data = app_state_get_user_data();
  app_unobstructed_area_service_unsubscribe();
  tick_timer_service_unsubscribe();
#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
  layer_deinit(&data->canvas_layer);
#endif
  i18n_free_all(data);
  app_free(data);
}

static void prv_init(void) {
  TicTocData *data = app_zalloc_check(sizeof(TicTocData));
  app_state_set_user_data(data);
  setlocale(LC_ALL, "");

  window_init(&data->window, WINDOW_NAME("TicToc"));
#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
  window_set_background_color(&data->window, GColorWhite);

  layer_init(&data->canvas_layer, &data->window.layer.bounds);
  layer_set_update_proc(&data->canvas_layer, prv_canvas_layer_update_proc);
  layer_add_child(&data->window.layer, &data->canvas_layer);
#else
  window_set_background_color(&data->window, GColorBlack);

  text_layer_init(&data->text_date_layer, &GRect(8, 68, DISP_COLS - 8, DISP_ROWS - 68));
  text_layer_set_text_color(&data->text_date_layer, GColorWhite);
  text_layer_set_background_color(&data->text_date_layer, GColorClear);
  text_layer_set_font(&data->text_date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  layer_add_child(&data->window.layer, &data->text_date_layer.layer);

  text_layer_init(&data->text_time_layer, &GRect(7, 92, DISP_COLS - 7, DISP_ROWS - 92));
  text_layer_set_text_color(&data->text_time_layer, GColorWhite);
  text_layer_set_background_color(&data->text_time_layer, GColorClear);
  text_layer_set_font(&data->text_time_layer,
                      fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  layer_add_child(&data->window.layer, &data->text_time_layer.layer);

  layer_init(&data->line_layer, &data->window.layer.frame);
  data->line_layer.update_proc = &prv_line_layer_update_callback;
  layer_add_child(&data->window.layer, &data->line_layer);
#endif

  tick_timer_service_subscribe(MINUTE_UNIT, prv_minute_tick_handler);

  struct tm time_struct;
  rtc_get_time_tm(&time_struct);
  prv_minute_tick_handler(&time_struct, MINUTE_UNIT);

  app_window_stack_push(&data->window, true);

  // Subscribe to unobstructed area changes
  UnobstructedAreaHandlers unobstructed_handlers = {
    .change = prv_unobstructed_area_change_handler
  };
  app_unobstructed_area_service_subscribe(unobstructed_handlers, NULL);

  // Set initial positions based on unobstructed area
  prv_update_layer_positions();
}

void tictoc_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
