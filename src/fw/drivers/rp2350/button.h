/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button_id.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FruitJamButtonDebugSnapshot {
  uint32_t raw_physical_state;
  uint32_t debounced_physical_state;
  uint32_t emitted_state;
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
