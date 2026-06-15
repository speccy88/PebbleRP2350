/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
#include "soc/rp2350/rp2350/fruitjam_bootsel.h"
#include "soc/rp2350/rp2350/fruitjam_boot_progress.h"
#include "system/reboot_reason.h"

static void prv_fruitjam_early_lcd_smoke(void) {
  RebootReason reason;
  reboot_reason_get(&reason);
  if (reason.code != RebootReasonCode_Unknown && !reason.restarted_safely) {
    const uint32_t *raw = (const uint32_t *)&reason;
    fruitjam_boot_progress_show_reboot_reason((uint8_t)reason.code, raw[1], raw[2], raw[3]);
  }

  if (fruitjam_bootsel_should_enter_after_unsafe_boot(&reason)) {
    fruitjam_boot_progress_show_label(FruitJamBootProgressStageFaultReset, "BOOTSEL");
    fruitjam_bootsel_clear_fault_state();
    fruitjam_bootsel_enter();
  }

  fruitjam_boot_progress_show(FruitJamBootProgressStageEarly);
}
#endif

uint32_t SystemCoreClock = 150000000;

void soc_early_init(void) {
#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
  prv_fruitjam_early_lcd_smoke();
#endif
}
