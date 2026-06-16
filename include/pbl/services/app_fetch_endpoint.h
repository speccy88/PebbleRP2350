/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"
#include "kernel/events.h"
#include "process_management/app_install_types.h"

typedef enum {
  AppFetchResultSuccess,
  AppFetchResultTimeoutError,
  AppFetchResultGeneralFailure,
  AppFetchResultPhoneBusy,
  AppFetchResultUUIDInvalid,
  AppFetchResultNoBluetooth,
  AppFetchResultPutBytesFailure,
  AppFetchResultNoData,
  AppFetchResultUserCancelled,
  AppFetchResultIncompatibleJSFailure,
} AppFetchResult;

typedef struct {
  AppFetchResult error;
  AppInstallId id;
} AppFetchError;

typedef struct {
  AppInstallId app_id;
  Uuid request_uuid;
  uint32_t total_size;
  uint32_t request_count;
  uint32_t request_send_ok_count;
  uint32_t request_send_fail_count;
  uint32_t response_count;
  uint32_t response_short_count;
  uint32_t response_invalid_count;
  uint32_t last_response_length;
  uint32_t cleanup_count;
  uint32_t put_bytes_event_count;
  uint32_t put_bytes_start_count;
  uint32_t put_bytes_progress_count;
  uint32_t put_bytes_cleanup_count;
  uint32_t put_bytes_timeout_count;
  uint32_t put_bytes_failure_count;
  AppFetchResult prev_error;
  AppFetchResult last_cleanup_result;
  uint8_t last_response_command;
  uint8_t last_response_result;
  uint8_t last_put_bytes_event_type;
  uint8_t last_put_bytes_object_type;
  uint8_t last_put_bytes_progress_percent;
  bool cancelling;
  bool in_progress;
  bool phone_started;
  bool app_done;
  bool worker_done;
  bool resources_done;
  bool last_put_bytes_failed;
} AppFetchDebugInfo;

void app_fetch_binaries(const Uuid *uuid, AppInstallId app_id, bool has_worker);

//! @param app_id The AppInstallId of the fetch to be cancelled.
//! NOTE: If `app_id` is INSTALL_ID_INVALID, it will cancel the fetch regardless of AppInstallId
void app_fetch_cancel(AppInstallId app_id);

//! @param app_id The AppInstallId of the fetch to be cancelled.
//! NOTE: If `app_id` is INSTALL_ID_INVALID, it will cancel the fetch regardless of AppInstallId
//! NOTE: Must be called from PebbleTask_KernelBackground
void app_fetch_cancel_from_system_task(AppInstallId app_id);

bool app_fetch_in_progress(void);

//! Put Bytes handler. Used for keeping track of progress and cleanup events
void app_fetch_put_bytes_event_handler(PebblePutBytesEvent *pb_event);

AppFetchError app_fetch_get_previous_error(void);

void app_fetch_get_debug_info(AppFetchDebugInfo *info);
