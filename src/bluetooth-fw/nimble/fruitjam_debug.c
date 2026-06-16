/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_bt_debug.h"

#include <bluetooth/bluetooth_types.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <pbl/services/system_task.h>
#include <system/reset.h>

#include <stddef.h>
#include <string.h>

#define BLE_AD_TYPE_SHORT_NAME 0x08U
#define BLE_AD_TYPE_COMPLETE_NAME 0x09U

static FruitJamBtDebugSnapshot s_debug;

static void prv_recovery_reset_cb(void *data) {
  (void)data;
  system_reset();
}

static uint8_t prv_min_u8(uint8_t a, uint8_t b) {
  return (a < b) ? a : b;
}

static void prv_copy_prefix(uint8_t *out, uint8_t *length_out, const uint8_t *data,
                            uint8_t length) {
  const uint8_t copy_length = prv_min_u8(length, FRUITJAM_BT_DEBUG_AD_PREFIX_SIZE);
  memcpy(out, data, copy_length);
  *length_out = copy_length;
}

static void prv_capture_local_name_from_payload(const uint8_t *data, uint8_t length) {
  const uint8_t *cursor = data;
  const uint8_t *end = data + length;

  while (cursor < end && cursor[0] != 0U) {
    const uint8_t field_length = cursor[0];
    if ((cursor + 1U + field_length) > end) {
      break;
    }

    const uint8_t type = cursor[1];
    if (type == BLE_AD_TYPE_SHORT_NAME || type == BLE_AD_TYPE_COMPLETE_NAME) {
      uint8_t name_length = field_length - 1U;
      name_length = prv_min_u8(name_length, FRUITJAM_BT_DEBUG_NAME_SIZE - 1U);
      memcpy(s_debug.local_name, &cursor[2], name_length);
      s_debug.local_name[name_length] = '\0';
      return;
    }

    cursor += 1U + field_length;
  }
}

void fruitjam_bt_debug_record_driver_start(bool ok) {
  ++s_debug.driver_start_count;
  if (ok) {
    ++s_debug.driver_start_ok_count;
  } else {
    ++s_debug.driver_start_fail_count;
  }
}

void fruitjam_bt_debug_record_driver_stage(FruitJamBtDebugDriverStage stage, uint8_t driver_state,
                                           int rc) {
  ++s_debug.driver_stage_count;
  s_debug.last_driver_stage = stage;
  s_debug.driver_state = driver_state;
  s_debug.last_driver_rc = rc;

  if (stage == FruitJamBtDebugDriverStageStartEnter) {
    ++s_debug.driver_start_enter_count;
  }
}

void fruitjam_bt_debug_record_host_sync(void) {
  ++s_debug.host_sync_count;
}

void fruitjam_bt_debug_record_host_reset(int reason) {
  ++s_debug.host_reset_count;
  s_debug.last_host_reset_reason = reason;
}

void fruitjam_bt_debug_schedule_recovery(FruitJamBtDebugRecoveryReason reason, int rc) {
  if (s_debug.recovery_pending) {
    return;
  }

  s_debug.recovery_pending = true;
  s_debug.recovery_last_reason = reason;
  s_debug.recovery_last_rc = rc;
  ++s_debug.recovery_schedule_count;

  if (!system_task_add_callback(prv_recovery_reset_cb, NULL)) {
    ++s_debug.recovery_schedule_fail_count;
    system_reset();
  }
}

void fruitjam_bt_debug_record_ctl_state(bool initialized, bool enabled, bool airplane, bool running,
                                        int override, bool active, bool holdoff) {
  ++s_debug.ctl_state_count;
  s_debug.ctl_initialized = initialized;
  s_debug.ctl_enabled = enabled;
  s_debug.ctl_airplane = airplane;
  s_debug.ctl_running = running;
  s_debug.ctl_override = override;
  s_debug.ctl_active = active;
  s_debug.ctl_holdoff = holdoff;
}

void fruitjam_bt_debug_record_adv_data(const BLEAdData *ad_data, int adv_rc, int scan_rsp_rc) {
  ++s_debug.adv_data_set_count;
  s_debug.last_adv_set_rc = adv_rc;
  s_debug.last_scan_rsp_set_rc = scan_rsp_rc;

  if (adv_rc != 0 || scan_rsp_rc != 0) {
    ++s_debug.adv_data_set_fail_count;
  }

  s_debug.ad_data_length = ad_data->ad_data_length;
  s_debug.scan_rsp_data_length = ad_data->scan_resp_data_length;
  s_debug.local_name[0] = '\0';
  prv_copy_prefix(s_debug.ad_prefix, &s_debug.ad_prefix_length, ad_data->data,
                  ad_data->ad_data_length);
  prv_copy_prefix(s_debug.scan_rsp_prefix, &s_debug.scan_rsp_prefix_length,
                  &ad_data->data[ad_data->ad_data_length], ad_data->scan_resp_data_length);
  prv_capture_local_name_from_payload(ad_data->data, ad_data->ad_data_length);
  if (s_debug.local_name[0] == '\0') {
    prv_capture_local_name_from_payload(&ad_data->data[ad_data->ad_data_length],
                                        ad_data->scan_resp_data_length);
  }
}

void fruitjam_bt_debug_record_adv_start(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                        int addr_rc, uint8_t own_addr_type, int start_rc) {
  ++s_debug.adv_start_count;
  s_debug.min_interval_ms = min_interval_ms;
  s_debug.max_interval_ms = max_interval_ms;
  s_debug.last_addr_rc = addr_rc;
  s_debug.last_adv_start_rc = start_rc;
  s_debug.own_addr_type = own_addr_type;

  if (addr_rc == 0 && start_rc == 0) {
    ++s_debug.adv_start_ok_count;
  } else {
    ++s_debug.adv_start_fail_count;
  }
}

void fruitjam_bt_debug_record_adv_stop(void) {
  ++s_debug.adv_stop_count;
}

void fruitjam_bt_debug_record_gap_event(int event_type, int status, int reason) {
  ++s_debug.gap_event_count;
  s_debug.last_gap_event = event_type;
  s_debug.last_gap_status = status;
  s_debug.last_gap_reason = reason;

  switch (event_type) {
    case BLE_GAP_EVENT_CONNECT:
      ++s_debug.gap_connect_count;
      if (status == 0) {
        ++s_debug.gap_connect_ok_count;
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      ++s_debug.gap_disconnect_count;
      break;
    case BLE_GAP_EVENT_PAIRING_COMPLETE:
      ++s_debug.gap_pairing_complete_count;
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      ++s_debug.gap_enc_change_count;
      break;
    case BLE_GAP_EVENT_MTU:
      ++s_debug.gap_mtu_count;
      break;
    case BLE_GAP_EVENT_SUBSCRIBE:
      ++s_debug.gap_subscribe_count;
      break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      ++s_debug.gap_notify_rx_count;
      break;
    case BLE_GAP_EVENT_NOTIFY_TX:
      ++s_debug.gap_notify_tx_count;
      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      ++s_debug.gap_repeat_pairing_count;
      break;
    default:
      break;
  }
}

void fruitjam_bt_debug_record_connection(uint16_t conn_handle, uint16_t mtu, bool master,
                                         bool encrypted, bool bonded) {
  s_debug.last_conn_handle = conn_handle;
  s_debug.last_conn_mtu = mtu;
  s_debug.last_conn_master = master;
  s_debug.last_conn_encrypted = encrypted;
  s_debug.last_conn_bonded = bonded;
}

void fruitjam_bt_debug_record_gap_subscribe(uint16_t attr_handle, bool notify, bool indicate) {
  s_debug.last_subscribe_attr_handle = attr_handle;
  s_debug.last_subscribe_notify = notify;
  s_debug.last_subscribe_indicate = indicate;
}

void fruitjam_bt_debug_record_pps_status(bool connected, bool bonded, bool encrypted,
                                         bool gateway) {
  s_debug.pps_last_connected = connected;
  s_debug.pps_last_bonded = bonded;
  s_debug.pps_last_encrypted = encrypted;
  s_debug.pps_last_gateway = gateway;
}

void fruitjam_bt_debug_record_pps_access(uint8_t op, int rc, uint8_t flags) {
  if (op == 0U) {
    ++s_debug.pps_status_read_count;
  } else if (op == 1U) {
    ++s_debug.pps_trigger_read_count;
  } else {
    ++s_debug.pps_trigger_write_count;
  }
  s_debug.pps_last_rc = rc;
  s_debug.pps_last_flags = flags;
}

void fruitjam_bt_debug_record_pps_notify(int rc) {
  ++s_debug.pps_notify_count;
  if (rc != 0) {
    ++s_debug.pps_notify_fail_count;
  }
  s_debug.pps_last_rc = rc;
}

void fruitjam_bt_debug_record_discovery_request(int rc) {
  ++s_debug.discovery_request_count;
  if (rc != 0) {
    ++s_debug.discovery_request_fail_count;
  }
  s_debug.discovery_last_request_rc = rc;
}

void fruitjam_bt_debug_record_discovery_complete(int status, uint32_t service_count) {
  ++s_debug.discovery_complete_count;
  if (status == 0) {
    ++s_debug.discovery_success_count;
  } else {
    ++s_debug.discovery_fail_count;
  }
  s_debug.discovery_last_status = status;
  s_debug.discovery_last_service_count = service_count;
}

void fruitjam_bt_debug_record_kernel_connection(bool connected) {
  ++s_debug.kernel_connection_event_count;
  if (connected) {
    ++s_debug.kernel_connected_count;
  } else {
    ++s_debug.kernel_disconnected_count;
  }
}

void fruitjam_bt_debug_record_kernel_bonding(uint8_t bonding_id, int op, bool is_gateway) {
  ++s_debug.kernel_bond_change_count;
  s_debug.kernel_last_bonding_id = bonding_id;
  s_debug.kernel_last_bond_op = op;
}

void fruitjam_bt_debug_record_kernel_gateway_connect(void) {
  ++s_debug.kernel_gateway_connect_count;
}

void fruitjam_bt_debug_record_kernel_ppogatt_found(void) {
  ++s_debug.kernel_ppogatt_found_count;
}

void fruitjam_bt_debug_record_store_sec(int obj_type, bool ok, uint8_t key_size, bool ltk, bool irk,
                                        bool sc, bool authenticated) {
  ++s_debug.store_sec_write_count;
  if (!ok) {
    ++s_debug.store_sec_reject_count;
  }
  if (obj_type == 1) {
    ++s_debug.store_our_sec_write_count;
  } else if (obj_type == 2) {
    ++s_debug.store_peer_sec_write_count;
  }
  s_debug.store_last_obj_type = (uint8_t)obj_type;
  s_debug.store_last_key_size = key_size;
  s_debug.store_last_key_flags =
      (ltk ? 0x01U : 0U) | (irk ? 0x02U : 0U) | (sc ? 0x04U : 0U) | (authenticated ? 0x08U : 0U);
}

void fruitjam_bt_debug_record_store_bonding(bool created, bool local_valid, bool remote_valid,
                                            bool identity_valid) {
  if (created) {
    ++s_debug.store_bond_create_count;
  } else {
    ++s_debug.store_bond_skip_count;
  }
  s_debug.store_last_bond_flags =
      (local_valid ? 0x01U : 0U) | (remote_valid ? 0x02U : 0U) | (identity_valid ? 0x04U : 0U);
}

void fruitjam_bt_debug_record_sm_pair(uint8_t req_auth, uint8_t rsp_auth, uint8_t init_key_dist,
                                      uint8_t resp_key_dist, uint8_t rx_key_flags,
                                      uint8_t proc_flags, uint8_t key_size) {
  ++s_debug.sm_pair_cfg_count;
  s_debug.sm_pair_req_auth = req_auth;
  s_debug.sm_pair_rsp_auth = rsp_auth;
  s_debug.sm_pair_init_key_dist = init_key_dist;
  s_debug.sm_pair_resp_key_dist = resp_key_dist;
  s_debug.sm_pair_rx_key_flags = rx_key_flags;
  s_debug.sm_pair_proc_flags = proc_flags;
  s_debug.sm_pair_key_size = key_size;
}

void fruitjam_bt_debug_record_sm_persist(uint8_t proc_flags, uint8_t our_key_flags,
                                         uint8_t peer_key_flags) {
  ++s_debug.sm_persist_count;
  s_debug.sm_persist_proc_flags = proc_flags;
  s_debug.sm_persist_our_key_flags = our_key_flags;
  s_debug.sm_persist_peer_key_flags = peer_key_flags;
}

void fruitjam_bt_debug_record_sm_process(uint8_t state_before, uint8_t state_after, bool execute,
                                         bool enc_cb, int app_status, uint8_t sm_err,
                                         uint8_t passkey_action, uint8_t pair_alg,
                                         uint8_t rx_key_flags, uint16_t proc_flags, bool removing) {
  ++s_debug.sm_process_count;
  if (execute) {
    ++s_debug.sm_execute_count;
  }
  if (removing) {
    ++s_debug.sm_remove_count;
  }
  if (passkey_action != 0U) {
    ++s_debug.sm_passkey_event_count;
  }
  s_debug.sm_last_proc_state = state_before;
  s_debug.sm_last_after_state = state_after;
  s_debug.sm_last_execute = execute ? 1U : 0U;
  s_debug.sm_last_enc_cb = enc_cb ? 1U : 0U;
  s_debug.sm_last_app_status = app_status;
  s_debug.sm_last_sm_err = sm_err;
  s_debug.sm_last_passkey_action = passkey_action;
  s_debug.sm_last_pair_alg = pair_alg;
  s_debug.sm_last_rx_key_flags = rx_key_flags;
  s_debug.sm_last_proc_flags = proc_flags;
}

void fruitjam_bt_debug_record_sm_sc(uint8_t step, uint8_t state, int status) {
  switch (step) {
    case 1:
      ++s_debug.sm_public_key_exec_count;
      break;
    case 2:
      ++s_debug.sm_public_key_rx_count;
      break;
    case 3:
      ++s_debug.sm_dhkey_exec_count;
      break;
    case 4:
      ++s_debug.sm_dhkey_rx_count;
      break;
    default:
      break;
  }

  s_debug.sm_last_sc_step = step;
  s_debug.sm_last_sc_state = state;
  s_debug.sm_last_sc_status = status;
}

void fruitjam_bt_debug_record_ppogatt_state(uint8_t state, uint8_t version, uint8_t destination,
                                            uint8_t client_count, uint8_t tx_window,
                                            uint8_t rx_window, uint8_t in_sn, uint8_t out_ack_sn,
                                            uint8_t out_data_sn, uint16_t queue_length) {
  s_debug.ppogatt_state = state;
  s_debug.ppogatt_version = version;
  s_debug.ppogatt_destination = destination;
  s_debug.ppogatt_client_count = client_count;
  s_debug.ppogatt_tx_window = tx_window;
  s_debug.ppogatt_rx_window = rx_window;
  s_debug.ppogatt_in_sn = in_sn;
  s_debug.ppogatt_out_ack_sn = out_ack_sn;
  s_debug.ppogatt_out_data_sn = out_data_sn;
  s_debug.ppogatt_queue_length = queue_length;
}

void fruitjam_bt_debug_record_ppogatt_event(FruitJamBtDebugPPoGATTEvent event, int error,
                                            uint16_t length) {
  s_debug.ppogatt_last_event = event;
  s_debug.ppogatt_last_error = error;

  switch (event) {
    case FruitJamBtDebugPPoGATTEventMetaOk:
      ++s_debug.ppogatt_meta_ok_count;
      s_debug.ppogatt_last_rx_length = length;
      break;
    case FruitJamBtDebugPPoGATTEventMetaFail:
      ++s_debug.ppogatt_meta_fail_count;
      break;
    case FruitJamBtDebugPPoGATTEventSubscribeOk:
      ++s_debug.ppogatt_subscribe_ok_count;
      break;
    case FruitJamBtDebugPPoGATTEventSubscribeFail:
      ++s_debug.ppogatt_subscribe_fail_count;
      break;
    case FruitJamBtDebugPPoGATTEventResetStart:
      ++s_debug.ppogatt_reset_start_count;
      break;
    case FruitJamBtDebugPPoGATTEventResetRequestRx:
      ++s_debug.ppogatt_reset_request_rx_count;
      break;
    case FruitJamBtDebugPPoGATTEventResetCompleteRx:
      ++s_debug.ppogatt_reset_complete_rx_count;
      break;
    case FruitJamBtDebugPPoGATTEventSessionOpen:
      ++s_debug.ppogatt_session_open_count;
      break;
    case FruitJamBtDebugPPoGATTEventDataRx:
      ++s_debug.ppogatt_data_rx_count;
      break;
    case FruitJamBtDebugPPoGATTEventAckRx:
      ++s_debug.ppogatt_ack_rx_count;
      break;
    case FruitJamBtDebugPPoGATTEventSendNext:
      ++s_debug.ppogatt_send_next_count;
      break;
    case FruitJamBtDebugPPoGATTEventSubscribeStart:
    case FruitJamBtDebugPPoGATTEventSubscribeUnexpected:
    case FruitJamBtDebugPPoGATTEventState:
      break;
  }
}

void fruitjam_bt_debug_record_ppogatt_packet(bool tx, bool ok, uint8_t type, uint8_t sn,
                                             uint16_t length, int error) {
  if (tx) {
    ++s_debug.ppogatt_packet_tx_count;
    if (ok) {
      ++s_debug.ppogatt_write_ok_count;
    } else {
      ++s_debug.ppogatt_write_fail_count;
    }
    s_debug.ppogatt_last_tx_type = type;
    s_debug.ppogatt_last_tx_sn = sn;
    s_debug.ppogatt_last_tx_length = length;
  } else {
    ++s_debug.ppogatt_packet_rx_count;
    s_debug.ppogatt_last_rx_type = type;
    s_debug.ppogatt_last_rx_sn = sn;
    s_debug.ppogatt_last_rx_length = length;
  }
  s_debug.ppogatt_last_error = error;
}

void fruitjam_bt_debug_record_pp_message(bool tx, uint16_t endpoint_id, uint16_t length) {
  if (tx) {
    ++s_debug.pp_tx_message_count;
    s_debug.pp_last_tx_endpoint = endpoint_id;
    s_debug.pp_last_tx_length = length;
  } else {
    ++s_debug.pp_rx_message_count;
    s_debug.pp_last_rx_endpoint = endpoint_id;
    s_debug.pp_last_rx_length = length;
  }
}

void fruitjam_bt_debug_record_pp_error(uint16_t endpoint_id, uint8_t error_code) {
  ++s_debug.pp_error_count;
  s_debug.pp_last_error_endpoint = endpoint_id;
  s_debug.pp_last_error_code = error_code;
}

void fruitjam_bt_debug_record_pp_no_buffer(uint16_t endpoint_id, uint16_t length) {
  ++s_debug.pp_no_buffer_count;
  s_debug.pp_last_error_endpoint = endpoint_id;
  s_debug.pp_last_error_code = 0xffU;
  s_debug.pp_last_rx_length = length;
}

void fruitjam_bt_debug_get_snapshot(FruitJamBtDebugSnapshot *snapshot) {
  *snapshot = s_debug;
  snapshot->adv_active = (ble_gap_adv_active() != 0);
  snapshot->sm_io_cap = ble_hs_cfg.sm_io_cap;
  snapshot->sm_our_key_dist = ble_hs_cfg.sm_our_key_dist;
  snapshot->sm_their_key_dist = ble_hs_cfg.sm_their_key_dist;
  snapshot->sm_bonding = ble_hs_cfg.sm_bonding;
  snapshot->sm_mitm = ble_hs_cfg.sm_mitm;
  snapshot->sm_sc = ble_hs_cfg.sm_sc;
  snapshot->sm_keypress = ble_hs_cfg.sm_keypress;
}
