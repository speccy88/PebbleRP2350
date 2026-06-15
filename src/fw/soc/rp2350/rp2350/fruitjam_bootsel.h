/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/reboot_reason.h"

#include <stdbool.h>

void fruitjam_bootsel_enter(void) __attribute__((noreturn));
bool fruitjam_bootsel_should_enter_after_unsafe_boot(const RebootReason *reason);
void fruitjam_bootsel_clear_boot_loop_strikes(void);
void fruitjam_bootsel_clear_fault_state(void);
