/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/button.h"
#include "drivers/debounced_button.h"
#include "drivers/rp2350/button.h"

#include "board/board.h"
#include "console/prompt.h"
#include "kernel/events.h"
#include "soc/rp2350/rp2350/fruitjam_bootsel.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define IO_BANK0_BASE 0x40028000U
#define PADS_BANK0_BASE 0x40038000U
#define SIO_BASE 0xd0000000U

#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + ((pin) * 8U) + 4U)
#define PADS_BANK0_GPIO(pin) (PADS_BANK0_BASE + 4U + ((pin) * 4U))

#define GPIO_FUNC_SIO 5U
#define GPIO_PAD_IE (1U << 6)
#define GPIO_PAD_DRIVE_4MA (1U << 4)
#define GPIO_PAD_PUE (1U << 3)
#define GPIO_PAD_PDE (1U << 2)
#define GPIO_PAD_SCHMITT (1U << 1)

#define SIO_GPIO_IN_OFFSET 0x04U
#define SIO_GPIO_HI_IN_OFFSET 0x08U
#define SIO_GPIO_OE_CLR_OFFSET 0x40U
#define SIO_GPIO_HI_OE_CLR_OFFSET 0x44U

#define BUTTON_TASK_STACK_SIZE 256U
#define BUTTON_TASK_PRIORITY 2U
#define BOOTSEL_HOLD_SAMPLES (RP2350_BUTTON_BOOTSEL_HOLD_MS / RP2350_BUTTON_POLL_PERIOD_MS)
#define DOWN_COMBO_GRACE_SAMPLES \
  (RP2350_BUTTON_DOWN_COMBO_GRACE_MS / RP2350_BUTTON_POLL_PERIOD_MS)
#define ALL_PHYSICAL_BUTTONS_MASK \
  ((1U << BUTTON_ID_BACK) | (1U << BUTTON_ID_UP) | (1U << BUTTON_ID_SELECT))

static bool s_rotated_180;
static TaskHandle_t s_button_task;
static volatile FruitJamButtonDebugSnapshot s_debug_snapshot = {
    .pending_button = NUM_BUTTONS,
    .last_event_button = NUM_BUTTONS,
};

typedef struct ButtonSynthState {
  uint32_t debounced_physical_state;
  uint32_t emitted_state;
  ButtonId pending_button;
  uint32_t pending_samples;
  uint32_t suppress_until_release_mask;
} ButtonSynthState;

static uint32_t prv_pad_config_for_pull(GPIOPuPd_TypeDef pull) {
  uint32_t pad = GPIO_PAD_IE | GPIO_PAD_DRIVE_4MA | GPIO_PAD_SCHMITT;

  if (pull == GPIO_PuPd_UP) {
    pad |= GPIO_PAD_PUE;
  } else if (pull == GPIO_PuPd_DOWN) {
    pad |= GPIO_PAD_PDE;
  }

  return pad;
}

static void prv_gpio_input_init(uint8_t pin, GPIOPuPd_TypeDef pull) {
  REG32(PADS_BANK0_GPIO(pin)) = prv_pad_config_for_pull(pull);
  REG32(IO_BANK0_GPIO_CTRL(pin)) = GPIO_FUNC_SIO;

  if (pin < 32U) {
    REG32(SIO_BASE + SIO_GPIO_OE_CLR_OFFSET) = 1U << pin;
  } else {
    REG32(SIO_BASE + SIO_GPIO_HI_OE_CLR_OFFSET) = 1U << (pin - 32U);
  }
}

static bool prv_gpio_read(uint8_t pin) {
  if (pin < 32U) {
    return (REG32(SIO_BASE + SIO_GPIO_IN_OFFSET) & (1U << pin)) != 0;
  }

  return (REG32(SIO_BASE + SIO_GPIO_HI_IN_OFFSET) & (1U << (pin - 32U))) != 0;
}

static bool prv_physical_button_is_pressed(ButtonId id) {
  const ButtonConfig *config = &BOARD_CONFIG_BUTTON.buttons[id];

  if (config->pin == GPIO_Pin_NULL || config->port == GPIO_Port_NULL) {
    return false;
  }

  const bool level = prv_gpio_read(config->pin);
  return config->active_high ? level : !level;
}

static uint8_t prv_physical_level_bits(void) {
  uint8_t levels = 0U;

  for (ButtonId id = BUTTON_ID_BACK; id <= BUTTON_ID_SELECT; ++id) {
    const ButtonConfig *config = &BOARD_CONFIG_BUTTON.buttons[id];

    if (config->pin == GPIO_Pin_NULL || config->port == GPIO_Port_NULL) {
      continue;
    }

    if (prv_gpio_read(config->pin)) {
      levels |= (uint8_t)(1U << id);
    }
  }

  return levels;
}

static uint8_t prv_physical_state_bits(void) {
  uint8_t state = 0U;

  for (ButtonId id = BUTTON_ID_BACK; id <= BUTTON_ID_SELECT; ++id) {
    if (prv_physical_button_is_pressed(id)) {
      state |= (uint8_t)(1U << id);
    }
  }

  return state;
}

static void prv_reset_debounce_counters(uint32_t counters[NUM_BUTTONS]) {
  for (ButtonId id = BUTTON_ID_BACK; id < NUM_BUTTONS; ++id) {
    counters[id] = 0U;
  }
}

static bool prv_all_physical_buttons_pressed(void) {
  uint8_t state = 0U;

  for (ButtonId id = BUTTON_ID_BACK; id <= BUTTON_ID_SELECT; ++id) {
    if (prv_physical_button_is_pressed(id)) {
      state |= (uint8_t)(1U << id);
    }
  }

  return (state & ALL_PHYSICAL_BUTTONS_MASK) == ALL_PHYSICAL_BUTTONS_MASK;
}

static uint8_t prv_apply_rotation(uint8_t state) {
  if (!s_rotated_180) {
    return state;
  }

  const uint8_t up = state & (1U << BUTTON_ID_UP);
  const uint8_t down = state & (1U << BUTTON_ID_DOWN);

  state &= (uint8_t)~((1U << BUTTON_ID_UP) | (1U << BUTTON_ID_DOWN));
  if (up) {
    state |= (uint8_t)(1U << BUTTON_ID_DOWN);
  }
  if (down) {
    state |= (uint8_t)(1U << BUTTON_ID_UP);
  }

  return state;
}

static ButtonId prv_apply_rotation_to_button(ButtonId id) {
  if (!s_rotated_180) {
    return id;
  }
  if (id == BUTTON_ID_UP) {
    return BUTTON_ID_DOWN;
  }
  if (id == BUTTON_ID_DOWN) {
    return BUTTON_ID_UP;
  }

  return id;
}

static uint8_t prv_unrotated_state_bits(void) {
  uint8_t state = prv_physical_state_bits();

  if ((state & RP2350_BUTTON_DOWN_COMBO_MASK) == RP2350_BUTTON_DOWN_COMBO_MASK) {
    state &= (uint8_t)~RP2350_BUTTON_DOWN_COMBO_MASK;
    state |= (uint8_t)(1U << BUTTON_ID_DOWN);
  }

  return state;
}

void button_init(void) {
  for (ButtonId id = BUTTON_ID_BACK; id < NUM_BUTTONS; ++id) {
    const ButtonConfig *config = &BOARD_CONFIG_BUTTON.buttons[id];

    if (config->pin == GPIO_Pin_NULL || config->port == GPIO_Port_NULL) {
      continue;
    }

    prv_gpio_input_init(config->pin, config->pull);
  }
}

void button_set_rotated(bool rotated) {
  s_rotated_180 = rotated;
}

bool button_is_pressed(ButtonId id) {
  if (id >= NUM_BUTTONS) {
    return false;
  }

  return (button_get_state_bits() & (1U << id)) != 0;
}

uint8_t button_get_state_bits(void) {
  return prv_apply_rotation(prv_unrotated_state_bits());
}

static void prv_emit_button_event(ButtonId id, bool is_pressed) {
  s_debug_snapshot.event_count++;
  s_debug_snapshot.last_event_button = id;
  s_debug_snapshot.last_event_down = is_pressed;

  PebbleEvent event = {
      .type = is_pressed ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
      .button.button_id = id,
  };

  event_put(&event);
}

bool button_debug_emit_event(ButtonId id, bool is_pressed) {
  if (id >= NUM_BUTTONS) {
    return false;
  }

  if (is_pressed) {
    s_debug_snapshot.injected_state |= 1U << id;
  } else {
    s_debug_snapshot.injected_state &= ~(1U << id);
  }
  prv_emit_button_event(id, is_pressed);
  return true;
}

bool button_debug_tap(ButtonId id) {
  if (id >= NUM_BUTTONS) {
    return false;
  }

  button_debug_emit_event(id, true);
  button_debug_emit_event(id, false);
  return true;
}

static void prv_set_emitted_button(ButtonSynthState *state, ButtonId id, bool is_pressed) {
  const ButtonId emitted_id = prv_apply_rotation_to_button(id);
  const uint32_t mask = 1U << emitted_id;
  const bool is_emitted = (state->emitted_state & mask) != 0U;

  if (is_emitted == is_pressed) {
    return;
  }

  if (is_pressed) {
    state->emitted_state |= mask;
  } else {
    state->emitted_state &= ~mask;
  }
  prv_emit_button_event(emitted_id, is_pressed);
}

static bool prv_is_emitted(ButtonSynthState *state, ButtonId id) {
  return (state->emitted_state & (1U << prv_apply_rotation_to_button(id))) != 0U;
}

static void prv_cancel_pending_button(ButtonSynthState *state) {
  state->pending_button = NUM_BUTTONS;
  state->pending_samples = 0U;
}

static void prv_emit_pending_tap(ButtonSynthState *state) {
  if (state->pending_button == NUM_BUTTONS) {
    return;
  }

  const ButtonId pending_button = state->pending_button;
  prv_cancel_pending_button(state);
  prv_set_emitted_button(state, pending_button, true);
  prv_set_emitted_button(state, pending_button, false);
}

static void prv_release_navigation_buttons(ButtonSynthState *state) {
  prv_cancel_pending_button(state);
  prv_set_emitted_button(state, BUTTON_ID_UP, false);
  prv_set_emitted_button(state, BUTTON_ID_SELECT, false);
  prv_set_emitted_button(state, BUTTON_ID_DOWN, false);
}

static void prv_update_pending_button(ButtonSynthState *state, ButtonId button) {
  if (state->pending_button == NUM_BUTTONS) {
    state->pending_button = button;
    state->pending_samples = 0U;
    return;
  }

  if (state->pending_button != button) {
    return;
  }

  state->pending_samples++;
  if (state->pending_samples >= DOWN_COMBO_GRACE_SAMPLES) {
    prv_cancel_pending_button(state);
    prv_set_emitted_button(state, button, true);
  }
}

static void prv_update_individual_navigation_button(ButtonSynthState *state, ButtonId button) {
  const uint32_t button_mask = 1U << button;
  const bool physically_pressed = (state->debounced_physical_state & button_mask) != 0U;

  if ((state->suppress_until_release_mask & button_mask) != 0U) {
    if (!physically_pressed) {
      state->suppress_until_release_mask &= ~button_mask;
    }
    if (state->pending_button == button) {
      prv_cancel_pending_button(state);
    }
    prv_set_emitted_button(state, button, false);
    return;
  }

  if (physically_pressed) {
    if (!prv_is_emitted(state, button)) {
      prv_update_pending_button(state, button);
    }
    return;
  }

  if (state->pending_button == button) {
    prv_emit_pending_tap(state);
  }
  prv_set_emitted_button(state, button, false);
}

static void prv_update_synthetic_buttons(ButtonSynthState *state) {
  const bool combo_pressed =
      (state->debounced_physical_state & RP2350_BUTTON_DOWN_COMBO_MASK) ==
      RP2350_BUTTON_DOWN_COMBO_MASK;

  prv_set_emitted_button(
      state, BUTTON_ID_BACK,
      (state->debounced_physical_state & (1U << BUTTON_ID_BACK)) != 0U);

  if (combo_pressed) {
    prv_cancel_pending_button(state);
    prv_set_emitted_button(state, BUTTON_ID_UP, false);
    prv_set_emitted_button(state, BUTTON_ID_SELECT, false);
    prv_set_emitted_button(state, BUTTON_ID_DOWN, true);
    return;
  }

  if (prv_is_emitted(state, BUTTON_ID_DOWN)) {
    prv_set_emitted_button(state, BUTTON_ID_DOWN, false);
    state->suppress_until_release_mask |=
        state->debounced_physical_state & RP2350_BUTTON_DOWN_COMBO_MASK;
  }

  prv_update_individual_navigation_button(state, BUTTON_ID_UP);
  prv_update_individual_navigation_button(state, BUTTON_ID_SELECT);
}

static void prv_suppress_all_button_events(ButtonSynthState *state) {
  prv_set_emitted_button(state, BUTTON_ID_BACK, false);
  prv_release_navigation_buttons(state);
  state->suppress_until_release_mask = state->debounced_physical_state & RP2350_BUTTON_DOWN_COMBO_MASK;
}

static void prv_update_debug_snapshot(const ButtonSynthState *state, uint8_t pin_level_state,
                                      uint8_t raw_state, uint32_t bootsel_hold_samples) {
  s_debug_snapshot.pin_level_state = pin_level_state;
  s_debug_snapshot.raw_physical_state = raw_state;
  s_debug_snapshot.debounced_physical_state = state->debounced_physical_state;
  s_debug_snapshot.emitted_state = state->emitted_state;
  s_debug_snapshot.suppress_until_release_mask = state->suppress_until_release_mask;
  s_debug_snapshot.pending_button = state->pending_button;
  s_debug_snapshot.pending_samples = state->pending_samples;
  s_debug_snapshot.bootsel_hold_samples = bootsel_hold_samples;
  s_debug_snapshot.down_combo_active =
      (state->debounced_physical_state & RP2350_BUTTON_DOWN_COMBO_MASK) ==
      RP2350_BUTTON_DOWN_COMBO_MASK;
  s_debug_snapshot.rotated_180 = s_rotated_180;
}

static void prv_button_task(void *data) {
  (void)data;

  ButtonSynthState synth_state = {
      .debounced_physical_state = prv_physical_state_bits(),
      .pending_button = NUM_BUTTONS,
  };
  uint8_t pin_level_state = prv_physical_level_bits();
  uint8_t raw_state = prv_physical_state_bits();
  uint32_t counters[NUM_BUTTONS] = {0};
  uint32_t bootsel_hold_samples = 0;

  prv_update_debug_snapshot(&synth_state, pin_level_state, raw_state, bootsel_hold_samples);

  while (true) {
    if (prv_all_physical_buttons_pressed()) {
      bootsel_hold_samples++;
      prv_reset_debounce_counters(counters);
      prv_suppress_all_button_events(&synth_state);
      pin_level_state = prv_physical_level_bits();
      raw_state = prv_physical_state_bits();
      prv_update_debug_snapshot(&synth_state, pin_level_state, raw_state, bootsel_hold_samples);
      if (bootsel_hold_samples >= BOOTSEL_HOLD_SAMPLES) {
        fruitjam_bootsel_enter();
      }
      vTaskDelay(pdMS_TO_TICKS(RP2350_BUTTON_POLL_PERIOD_MS));
      continue;
    }

    bootsel_hold_samples = 0U;
    pin_level_state = prv_physical_level_bits();
    raw_state = prv_physical_state_bits();

    for (ButtonId id = BUTTON_ID_BACK; id < NUM_BUTTONS; ++id) {
      if (id == BUTTON_ID_DOWN) {
        continue;
      }

      const bool raw_pressed = (raw_state & (1U << id)) != 0;
      const bool debounced_pressed =
          (synth_state.debounced_physical_state & (1U << id)) != 0;

      if (raw_pressed == debounced_pressed) {
        counters[id] = 0;
      } else {
        counters[id]++;
        if (counters[id] >= RP2350_BUTTON_DEBOUNCE_SAMPLES) {
          counters[id] = 0;
          if (raw_pressed) {
            synth_state.debounced_physical_state |= (1U << id);
          } else {
            synth_state.debounced_physical_state &= ~(1U << id);
          }
        }
      }
    }

    prv_update_synthetic_buttons(&synth_state);
    prv_update_debug_snapshot(&synth_state, pin_level_state, raw_state, bootsel_hold_samples);

    vTaskDelay(pdMS_TO_TICKS(RP2350_BUTTON_POLL_PERIOD_MS));
  }
}

void button_debug_get_snapshot(FruitJamButtonDebugSnapshot *snapshot) {
  if (!snapshot) {
    return;
  }

  *snapshot = s_debug_snapshot;
}

void debounced_button_init(void) {
  button_init();

  if (!s_button_task) {
    xTaskCreate(prv_button_task, "Buttons", BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY,
                &s_button_task);
  }
}

void command_button_read(const char *button_id_str) {
  int button = atoi(button_id_str);

  if (button < 0 || button >= NUM_BUTTONS) {
    prompt_send_response("Invalid button");
    return;
  }

  prompt_send_response(button_is_pressed((ButtonId)button) ? "down" : "up");
}

void command_put_raw_button_event(const char *button_index, const char *is_button_down_event) {
  const int button = atoi(button_index);
  const int is_down = atoi(is_button_down_event);

  if (button < 0 || button >= NUM_BUTTONS || (is_down != 0 && is_down != 1)) {
    return;
  }

  prv_emit_button_event((ButtonId)button, is_down);
}
