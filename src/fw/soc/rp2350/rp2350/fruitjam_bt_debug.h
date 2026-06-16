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

typedef enum FruitJamBtDebugPPoGATTEvent {
  FruitJamBtDebugPPoGATTEventState = 0,
  FruitJamBtDebugPPoGATTEventMetaOk = 1,
  FruitJamBtDebugPPoGATTEventMetaFail = 2,
  FruitJamBtDebugPPoGATTEventSubscribeOk = 3,
  FruitJamBtDebugPPoGATTEventSubscribeFail = 4,
  FruitJamBtDebugPPoGATTEventResetStart = 5,
  FruitJamBtDebugPPoGATTEventResetRequestRx = 6,
  FruitJamBtDebugPPoGATTEventResetCompleteRx = 7,
  FruitJamBtDebugPPoGATTEventSessionOpen = 8,
  FruitJamBtDebugPPoGATTEventDataRx = 9,
  FruitJamBtDebugPPoGATTEventAckRx = 10,
  FruitJamBtDebugPPoGATTEventSendNext = 11,
  FruitJamBtDebugPPoGATTEventSubscribeStart = 12,
  FruitJamBtDebugPPoGATTEventSubscribeUnexpected = 13,
} FruitJamBtDebugPPoGATTEvent;

typedef enum FruitJamBtDebugRecoveryReason {
  FruitJamBtDebugRecoveryReasonNone = 0,
  FruitJamBtDebugRecoveryReasonHostReset = 1,
  FruitJamBtDebugRecoveryReasonStartTimeout = 2,
  FruitJamBtDebugRecoveryReasonStartError = 3,
  FruitJamBtDebugRecoveryReasonAdvAddress = 4,
  FruitJamBtDebugRecoveryReasonAdvStart = 5,
} FruitJamBtDebugRecoveryReason;

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
  uint32_t recovery_schedule_count;
  uint32_t recovery_schedule_fail_count;
  int32_t recovery_last_rc;
  uint8_t recovery_last_reason;
  bool recovery_pending;
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
  uint32_t gap_enc_change_count;
  uint32_t gap_mtu_count;
  uint32_t gap_subscribe_count;
  uint32_t gap_notify_rx_count;
  uint32_t gap_notify_tx_count;
  uint32_t gap_repeat_pairing_count;
  int32_t last_gap_event;
  int32_t last_gap_status;
  int32_t last_gap_reason;
  uint16_t last_conn_handle;
  uint16_t last_conn_mtu;
  bool last_conn_master;
  bool last_conn_encrypted;
  bool last_conn_bonded;
  uint8_t sm_io_cap;
  uint8_t sm_our_key_dist;
  uint8_t sm_their_key_dist;
  bool sm_bonding;
  bool sm_mitm;
  bool sm_sc;
  bool sm_keypress;
  uint32_t sm_pair_cfg_count;
  uint32_t sm_persist_count;
  uint8_t sm_pair_req_auth;
  uint8_t sm_pair_rsp_auth;
  uint8_t sm_pair_init_key_dist;
  uint8_t sm_pair_resp_key_dist;
  uint8_t sm_pair_rx_key_flags;
  uint8_t sm_pair_proc_flags;
  uint8_t sm_pair_key_size;
  uint8_t sm_persist_proc_flags;
  uint8_t sm_persist_our_key_flags;
  uint8_t sm_persist_peer_key_flags;
  uint32_t sm_process_count;
  uint32_t sm_execute_count;
  uint32_t sm_remove_count;
  uint32_t sm_passkey_event_count;
  int32_t sm_last_app_status;
  uint16_t sm_last_proc_flags;
  uint8_t sm_last_proc_state;
  uint8_t sm_last_after_state;
  uint8_t sm_last_execute;
  uint8_t sm_last_enc_cb;
  uint8_t sm_last_sm_err;
  uint8_t sm_last_passkey_action;
  uint8_t sm_last_pair_alg;
  uint8_t sm_last_rx_key_flags;
  uint32_t sm_public_key_exec_count;
  uint32_t sm_public_key_rx_count;
  uint32_t sm_dhkey_exec_count;
  uint32_t sm_dhkey_rx_count;
  int32_t sm_last_sc_status;
  uint8_t sm_last_sc_step;
  uint8_t sm_last_sc_state;
  uint16_t last_subscribe_attr_handle;
  bool last_subscribe_notify;
  bool last_subscribe_indicate;
  uint32_t pps_status_read_count;
  uint32_t pps_trigger_read_count;
  uint32_t pps_trigger_write_count;
  uint32_t pps_notify_count;
  uint32_t pps_notify_fail_count;
  int32_t pps_last_rc;
  uint8_t pps_last_flags;
  bool pps_last_connected;
  bool pps_last_bonded;
  bool pps_last_encrypted;
  bool pps_last_gateway;
  uint32_t discovery_request_count;
  uint32_t discovery_request_fail_count;
  uint32_t discovery_complete_count;
  uint32_t discovery_success_count;
  uint32_t discovery_fail_count;
  uint32_t discovery_last_service_count;
  int32_t discovery_last_status;
  int32_t discovery_last_request_rc;
  uint32_t kernel_connection_event_count;
  uint32_t kernel_connected_count;
  uint32_t kernel_disconnected_count;
  uint32_t kernel_bond_change_count;
  uint32_t kernel_gateway_connect_count;
  uint32_t kernel_ppogatt_found_count;
  uint8_t kernel_last_bonding_id;
  int32_t kernel_last_bond_op;
  uint32_t store_sec_write_count;
  uint32_t store_our_sec_write_count;
  uint32_t store_peer_sec_write_count;
  uint32_t store_sec_reject_count;
  uint32_t store_bond_create_count;
  uint32_t store_bond_skip_count;
  uint8_t store_last_obj_type;
  uint8_t store_last_key_size;
  uint8_t store_last_key_flags;
  uint8_t store_last_bond_flags;
  uint32_t ppogatt_meta_ok_count;
  uint32_t ppogatt_meta_fail_count;
  uint32_t ppogatt_subscribe_ok_count;
  uint32_t ppogatt_subscribe_fail_count;
  uint32_t ppogatt_reset_start_count;
  uint32_t ppogatt_reset_request_rx_count;
  uint32_t ppogatt_reset_complete_rx_count;
  uint32_t ppogatt_session_open_count;
  uint32_t ppogatt_data_rx_count;
  uint32_t ppogatt_ack_rx_count;
  uint32_t ppogatt_packet_rx_count;
  uint32_t ppogatt_packet_tx_count;
  uint32_t ppogatt_write_ok_count;
  uint32_t ppogatt_write_fail_count;
  uint32_t ppogatt_send_next_count;
  int32_t ppogatt_last_error;
  uint16_t ppogatt_last_rx_length;
  uint16_t ppogatt_last_tx_length;
  uint16_t ppogatt_queue_length;
  uint8_t ppogatt_client_count;
  uint8_t ppogatt_state;
  uint8_t ppogatt_version;
  uint8_t ppogatt_destination;
  uint8_t ppogatt_tx_window;
  uint8_t ppogatt_rx_window;
  uint8_t ppogatt_in_sn;
  uint8_t ppogatt_out_ack_sn;
  uint8_t ppogatt_out_data_sn;
  uint8_t ppogatt_last_rx_type;
  uint8_t ppogatt_last_rx_sn;
  uint8_t ppogatt_last_tx_type;
  uint8_t ppogatt_last_tx_sn;
  uint8_t ppogatt_last_event;
  uint32_t pp_rx_message_count;
  uint32_t pp_tx_message_count;
  uint32_t pp_error_count;
  uint32_t pp_no_buffer_count;
  uint16_t pp_last_rx_endpoint;
  uint16_t pp_last_rx_length;
  uint16_t pp_last_tx_endpoint;
  uint16_t pp_last_tx_length;
  uint16_t pp_last_error_endpoint;
  uint8_t pp_last_error_code;
} FruitJamBtDebugSnapshot;

void fruitjam_bt_debug_record_driver_stage(FruitJamBtDebugDriverStage stage, uint8_t driver_state,
                                           int rc);
void fruitjam_bt_debug_record_driver_start(bool ok);
void fruitjam_bt_debug_record_host_sync(void);
void fruitjam_bt_debug_record_host_reset(int reason);
void fruitjam_bt_debug_schedule_recovery(FruitJamBtDebugRecoveryReason reason, int rc);
void fruitjam_bt_debug_record_ctl_state(bool initialized, bool enabled, bool airplane, bool running,
                                        int override, bool active, bool holdoff);
void fruitjam_bt_debug_record_adv_data(const BLEAdData *ad_data, int adv_rc, int scan_rsp_rc);
void fruitjam_bt_debug_record_adv_start(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                        int addr_rc, uint8_t own_addr_type, int start_rc);
void fruitjam_bt_debug_record_adv_stop(void);
void fruitjam_bt_debug_record_gap_event(int event_type, int status, int reason);
void fruitjam_bt_debug_record_connection(uint16_t conn_handle, uint16_t mtu, bool master,
                                         bool encrypted, bool bonded);
void fruitjam_bt_debug_record_gap_subscribe(uint16_t attr_handle, bool notify, bool indicate);
void fruitjam_bt_debug_record_pps_status(bool connected, bool bonded, bool encrypted, bool gateway);
void fruitjam_bt_debug_record_pps_access(uint8_t op, int rc, uint8_t flags);
void fruitjam_bt_debug_record_pps_notify(int rc);
void fruitjam_bt_debug_record_discovery_request(int rc);
void fruitjam_bt_debug_record_discovery_complete(int status, uint32_t service_count);
void fruitjam_bt_debug_record_kernel_connection(bool connected);
void fruitjam_bt_debug_record_kernel_bonding(uint8_t bonding_id, int op, bool is_gateway);
void fruitjam_bt_debug_record_kernel_gateway_connect(void);
void fruitjam_bt_debug_record_kernel_ppogatt_found(void);
void fruitjam_bt_debug_record_store_sec(int obj_type, bool ok, uint8_t key_size, bool ltk, bool irk,
                                        bool sc, bool authenticated);
void fruitjam_bt_debug_record_store_bonding(bool created, bool local_valid, bool remote_valid,
                                            bool identity_valid);
void fruitjam_bt_debug_record_sm_pair(uint8_t req_auth, uint8_t rsp_auth, uint8_t init_key_dist,
                                      uint8_t resp_key_dist, uint8_t rx_key_flags,
                                      uint8_t proc_flags, uint8_t key_size);
void fruitjam_bt_debug_record_sm_persist(uint8_t proc_flags, uint8_t our_key_flags,
                                         uint8_t peer_key_flags);
void fruitjam_bt_debug_record_sm_process(uint8_t state_before, uint8_t state_after, bool execute,
                                         bool enc_cb, int app_status, uint8_t sm_err,
                                         uint8_t passkey_action, uint8_t pair_alg,
                                         uint8_t rx_key_flags, uint16_t proc_flags, bool removing);
void fruitjam_bt_debug_record_sm_sc(uint8_t step, uint8_t state, int status);
void fruitjam_bt_debug_record_ppogatt_state(uint8_t state, uint8_t version, uint8_t destination,
                                            uint8_t client_count, uint8_t tx_window,
                                            uint8_t rx_window, uint8_t in_sn, uint8_t out_ack_sn,
                                            uint8_t out_data_sn, uint16_t queue_length);
void fruitjam_bt_debug_record_ppogatt_event(FruitJamBtDebugPPoGATTEvent event, int error,
                                            uint16_t length);
void fruitjam_bt_debug_record_ppogatt_packet(bool tx, bool ok, uint8_t type, uint8_t sn,
                                             uint16_t length, int error);
void fruitjam_bt_debug_record_pp_message(bool tx, uint16_t endpoint_id, uint16_t length);
void fruitjam_bt_debug_record_pp_error(uint16_t endpoint_id, uint8_t error_code);
void fruitjam_bt_debug_record_pp_no_buffer(uint16_t endpoint_id, uint16_t length);
void fruitjam_bt_debug_get_snapshot(FruitJamBtDebugSnapshot *snapshot);
