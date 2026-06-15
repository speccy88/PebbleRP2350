/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum FruitJamBootProgressStage {
  FruitJamBootProgressStageEarly,
  FruitJamBootProgressStagePfsStart,
  FruitJamBootProgressStagePfsDone,
  FruitJamBootProgressStageDriversStart,
  FruitJamBootProgressStageDriversDone,
  FruitJamBootProgressStageResourcesStart,
  FruitJamBootProgressStageResourcesDone,
  FruitJamBootProgressStageDisplayInit,
  FruitJamBootProgressStageDriverReady,
  FruitJamBootProgressStageCompositorStart,
  FruitJamBootProgressStageCompositorDone,
  FruitJamBootProgressStageBluetoothInitStart,
  FruitJamBootProgressStageBluetoothInitDone,
  FruitJamBootProgressStageServicesStart,
  FruitJamBootProgressStageServicesDone,
  FruitJamBootProgressStageBluetoothStart,
  FruitJamBootProgressStageBluetoothUnavailable,
  FruitJamBootProgressStageBluetoothDone,
  FruitJamBootProgressStageLauncherReady,
  FruitJamBootProgressStageCount,
  FruitJamBootProgressStageResourceError = FruitJamBootProgressStageCount,
  FruitJamBootProgressStageFaultReset,
  FruitJamBootProgressStageFaultAssert,
  FruitJamBootProgressStageFaultHard,
  FruitJamBootProgressStageFaultStack,
  FruitJamBootProgressStageFaultOom,
  FruitJamBootProgressStageFaultWatchdog,
  FruitJamBootProgressStageFaultCoreDump,
} FruitJamBootProgressStage;

void fruitjam_boot_progress_show(FruitJamBootProgressStage stage);
void fruitjam_boot_progress_show_label(FruitJamBootProgressStage stage, const char *label);
void fruitjam_boot_progress_show_reboot_reason(uint8_t reason_code, uint32_t raw1,
                                               uint32_t raw2, uint32_t raw3);
void fruitjam_boot_progress_write_frame(FruitJamBootProgressStage stage);
void fruitjam_boot_progress_write_label(FruitJamBootProgressStage stage, const char *label);
void fruitjam_boot_progress_mark(FruitJamBootProgressStage stage);
void fruitjam_boot_progress_mark_label(FruitJamBootProgressStage stage, const char *label);
FruitJamBootProgressStage fruitjam_boot_progress_last_stage(void);
const char *fruitjam_boot_progress_last_label(void);
uint32_t fruitjam_boot_progress_sequence(void);
const char *fruitjam_boot_progress_label(FruitJamBootProgressStage stage);
