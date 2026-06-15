/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/vibe.h"
#include "kernel/core_dump.h"
#include "kernel/logging_private.h"
#include "kernel/pulse_logging.h"
#include "system/bootbits.h"
#include "system/passert.h"
#include "system/reboot_reason.h"
#include "system/reset.h"

#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
#include "soc/rp2350/rp2350/fruitjam_boot_progress.h"
#endif

#include <cmsis_core.h>

#if defined(CONFIG_NO_WATCHDOG)
#include "FreeRTOS.h"
#endif

#if defined(CONFIG_BOARD_FRUITJAM_RP2350)
static FruitJamBootProgressStage prv_fruitjam_stage_from_reboot_reason(
    RebootReasonCode reason_code) {
  switch (reason_code) {
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

static void prv_fruitjam_show_software_failure(void) {
  RebootReason reason;
  reboot_reason_get(&reason);
  fruitjam_boot_progress_show(prv_fruitjam_stage_from_reboot_reason(reason.code));
}
#else
static void prv_fruitjam_show_software_failure(void) {
}
#endif

void prepare_for_software_failure(void) {
  prv_fruitjam_show_software_failure();

#ifdef CONFIG_PULSE_EVERYWHERE
  pulse_logging_log_buffer_flush();
#endif

#ifndef CONFIG_MFG
  boot_bit_set(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
#endif
}

NORETURN reset_due_to_software_failure(void) {
  prepare_for_software_failure();

#if defined(CONFIG_NO_WATCHDOG)
  // Don't reset right away, leave it in a state we can inspect

  __disable_irq();
  while (1) {
    continue;
  }
#endif

  PBL_LOG_FROM_FAULT_HANDLER("Resetting!");
  system_reset();
}
