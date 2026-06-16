/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct PebbleCommSessionEvent PebbleCommSessionEvent;

typedef enum {
  ObjectUnknown = 0x00,
  ObjectFirmware = 0x01,
  ObjectRecovery = 0x02,
  ObjectSysResources = 0x03,
  ObjectAppResources = 0x04,
  ObjectWatchApp = 0x05,
  ObjectFile = 0x06,
  ObjectWatchWorker = 0x07,
  NumObjects
} PutBytesObjectType;

typedef struct PbInstallStatus {
  uint32_t num_bytes_written;
  uint32_t crc_of_bytes;
} PbInstallStatus;

typedef struct {
  uint32_t token;
  uint32_t index;
  uint32_t total_size;
  uint32_t remaining_bytes;
  uint32_t append_offset;
  uint32_t current_offset;
  uint32_t receiver_length;
  uint32_t receiver_pos;
  uint32_t request_length;
  uint32_t last_data_length;
  uint32_t last_crc;
  uint32_t last_expected_crc;
  uint32_t init_count;
  uint32_t put_count;
  uint32_t commit_count;
  uint32_t abort_count;
  uint32_t install_count;
  uint32_t cleanup_count;
  uint32_t timeout_count;
  uint32_t fail_count;
  uint32_t ack_count;
  uint32_t nack_count;
  uint32_t storage_init_fail_count;
  uint32_t crc_fail_count;
  uint32_t prepare_fail_count;
  uint8_t current_command;
  uint8_t object_type;
  uint8_t has_cookie;
  uint8_t is_success;
  uint8_t receiver_should_nack;
  uint8_t pending_jobs;
  uint8_t allocated_jobs;
  uint8_t read_idx;
  uint8_t preack_enabled;
  uint8_t ack_later;
  uint8_t last_command;
  uint8_t last_response_code;
  bool lock_ok;
} PutBytesDebugInfo;

void put_bytes_init(void);

//! Tells put_bytes to clean up instantly. If put_bytes is already cleaned up, this is a no-op.
//! Any future messages sent by clients will be NACK'ed appropriately.
//! NOTE: Must be called from the KernelBackground task.
void put_bytes_cancel(void);

//! Reset all put bytes state. Only useful for unit tests.
void put_bytes_deinit(void);

//! Sets an initialization timeout for put_bytes.
//! If the phone doesn't send any data within the specified timeout,
//! put_bytes raises a timeout event.
void put_bytes_expect_init(uint32_t timeout_ms);

//! Informs Put Bytes when the Pebble app disconnects to the Pebble, to make
//! it cancel any on-going transaction.
void put_bytes_handle_comm_session_event(const PebbleCommSessionEvent *app_event);

void put_bytes_get_debug_info(PutBytesDebugInfo *info);
