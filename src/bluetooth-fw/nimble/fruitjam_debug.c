/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_bt_debug.h"

#include <bluetooth/bluetooth_types.h>
#include <host/ble_gap.h>

#include <stddef.h>
#include <string.h>

#define BLE_AD_TYPE_SHORT_NAME 0x08U
#define BLE_AD_TYPE_COMPLETE_NAME 0x09U

static FruitJamBtDebugSnapshot s_debug;

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

void fruitjam_bt_debug_record_host_sync(void) {
  ++s_debug.host_sync_count;
}

void fruitjam_bt_debug_record_host_reset(int reason) {
  ++s_debug.host_reset_count;
  s_debug.last_host_reset_reason = reason;
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
    default:
      break;
  }
}

void fruitjam_bt_debug_get_snapshot(FruitJamBtDebugSnapshot *snapshot) {
  *snapshot = s_debug;
  snapshot->adv_active = (ble_gap_adv_active() != 0);
}
