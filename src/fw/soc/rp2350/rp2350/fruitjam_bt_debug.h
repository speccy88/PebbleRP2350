/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FRUITJAM_BT_DEBUG_NAME_SIZE 32U
#define FRUITJAM_BT_DEBUG_AD_PREFIX_SIZE 16U

typedef struct BLEAdData BLEAdData;

typedef enum FruitJamBtDebugDriverStage {
  FruitJamBtDebugDriverStageIdle = 0,
  FruitJamBtDebugDriverStageHostSync = 1,
  FruitJamBtDebugDriverStageHostReset = 2,
  FruitJamBtDebugDriverStageStartEnter = 10,
  FruitJamBtDebugDriverStageAlreadyStarted = 11,
  FruitJamBtDebugDriverStageBadState = 12,
  FruitJamBtDebugDriverStageStarting = 13,
  FruitJamBtDebugDriverStageServicesReady = 14,
  FruitJamBtDebugDriverStageWaitSync = 15,
  FruitJamBtDebugDriverStageSyncTimeout = 16,
  FruitJamBtDebugDriverStageSynced = 17,
  FruitJamBtDebugDriverStageEnsureAddr = 18,
  FruitJamBtDebugDriverStageAddrDone = 19,
  FruitJamBtDebugDriverStageStarted = 20,
  FruitJamBtDebugDriverStageError = 21,
} FruitJamBtDebugDriverStage;

typedef struct {
  uint32_t driver_start_count;
  uint32_t driver_start_enter_count;
  uint32_t driver_start_ok_count;
  uint32_t driver_start_fail_count;
  uint32_t driver_stage_count;
  int32_t last_driver_rc;
  uint32_t host_sync_count;
  uint32_t host_reset_count;
  int32_t last_host_reset_reason;
  uint8_t last_driver_stage;
  uint8_t driver_state;
  uint32_t ctl_state_count;
  int32_t ctl_override;
  bool ctl_initialized;
  bool ctl_enabled;
  bool ctl_airplane;
  bool ctl_running;
  bool ctl_active;
  bool ctl_holdoff;
  uint32_t adv_data_set_count;
  uint32_t adv_data_set_fail_count;
  int32_t last_adv_set_rc;
  int32_t last_scan_rsp_set_rc;
  uint8_t ad_data_length;
  uint8_t scan_rsp_data_length;
  uint8_t ad_prefix_length;
  uint8_t scan_rsp_prefix_length;
  uint8_t ad_prefix[FRUITJAM_BT_DEBUG_AD_PREFIX_SIZE];
  uint8_t scan_rsp_prefix[FRUITJAM_BT_DEBUG_AD_PREFIX_SIZE];
  char local_name[FRUITJAM_BT_DEBUG_NAME_SIZE];
  uint32_t adv_start_count;
  uint32_t adv_start_ok_count;
  uint32_t adv_start_fail_count;
  uint32_t adv_stop_count;
  uint32_t min_interval_ms;
  uint32_t max_interval_ms;
  int32_t last_addr_rc;
  int32_t last_adv_start_rc;
  uint8_t own_addr_type;
  bool adv_active;
  uint32_t gap_event_count;
  uint32_t gap_connect_count;
  uint32_t gap_connect_ok_count;
  uint32_t gap_disconnect_count;
  uint32_t gap_pairing_complete_count;
  int32_t last_gap_event;
  int32_t last_gap_status;
  int32_t last_gap_reason;
} FruitJamBtDebugSnapshot;

void fruitjam_bt_debug_record_driver_stage(FruitJamBtDebugDriverStage stage, uint8_t driver_state,
                                           int rc);
void fruitjam_bt_debug_record_driver_start(bool ok);
void fruitjam_bt_debug_record_host_sync(void);
void fruitjam_bt_debug_record_host_reset(int reason);
void fruitjam_bt_debug_record_ctl_state(bool initialized, bool enabled, bool airplane,
                                        bool running, int override, bool active, bool holdoff);
void fruitjam_bt_debug_record_adv_data(const BLEAdData *ad_data, int adv_rc, int scan_rsp_rc);
void fruitjam_bt_debug_record_adv_start(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                        int addr_rc, uint8_t own_addr_type, int start_rc);
void fruitjam_bt_debug_record_adv_stop(void);
void fruitjam_bt_debug_record_gap_event(int event_type, int status, int reason);
void fruitjam_bt_debug_get_snapshot(FruitJamBtDebugSnapshot *snapshot);
