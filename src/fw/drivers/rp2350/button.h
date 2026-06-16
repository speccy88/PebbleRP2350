/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button_id.h"

#include <stdbool.h>
#include <stdint.h>

#define RP2350_BUTTON_POLL_PERIOD_MS 2U
#define RP2350_BUTTON_DEBOUNCE_SAMPLES 20U
#define RP2350_BUTTON_BOOTSEL_HOLD_MS 2000U
#define RP2350_BUTTON_DOWN_COMBO_GRACE_MS 90U
#define RP2350_BUTTON_DOWN_COMBO_MASK ((1U << BUTTON_ID_UP) | (1U << BUTTON_ID_SELECT))

typedef struct FruitJamButtonDebugSnapshot {
  uint32_t pin_level_state;
  uint32_t raw_physical_state;
  uint32_t debounced_physical_state;
  uint32_t emitted_state;
  uint32_t injected_state;
  uint32_t suppress_until_release_mask;
  ButtonId pending_button;
  uint32_t pending_samples;
  uint32_t bootsel_hold_samples;
  uint32_t event_count;
  ButtonId last_event_button;
  bool last_event_down;
  bool down_combo_active;
  bool rotated_180;
} FruitJamButtonDebugSnapshot;

void button_debug_get_snapshot(FruitJamButtonDebugSnapshot *snapshot);
bool button_debug_emit_event(ButtonId id, bool is_pressed);
bool button_debug_tap(ButtonId id);
