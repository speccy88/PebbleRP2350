/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FRUITJAM_ESP_HCI_DEBUG_EVT_PREFIX_SIZE 8U
#define FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE 8U

typedef struct FruitJamEspHciDebugCmdEntry {
  uint16_t opcode;
  uint8_t length;
  uint8_t ok;
} FruitJamEspHciDebugCmdEntry;

typedef struct FruitJamEspHciDebugEvtEntry {
  uint16_t opcode;
  uint8_t code;
  uint8_t length;
  uint8_t status;
} FruitJamEspHciDebugEvtEntry;

typedef struct FruitJamEspHciDebugSnapshot {
  uint32_t init_count;
  uint32_t ready_count;
  uint32_t ready_timeout_count;
  uint32_t drain_bytes;
  uint32_t rx_bytes;
  uint32_t rx_error_count;
  uint32_t rx_irq_count;
  uint32_t rx_ring_drop_count;
  uint32_t tx_bytes;
  uint32_t tx_timeout_count;
  uint32_t h4_discard_count;
  uint32_t h4_parse_error_count;
  uint32_t h4_evt_count;
  uint32_t h4_acl_count;
  uint32_t h4_iso_count;
  uint32_t hci_cmd_count;
  uint32_t hci_cmd_fail_count;
  uint32_t hci_evt_cmd_complete_count;
  uint32_t hci_evt_cmd_status_count;
  uint32_t hci_evt_other_count;
  uint32_t hci_cmd_seq;
  uint32_t hci_evt_seq;
  FruitJamEspHciDebugCmdEntry hci_cmd_history[FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE];
  FruitJamEspHciDebugEvtEntry hci_evt_history[FRUITJAM_ESP_HCI_DEBUG_SEQ_SIZE];
  uint16_t last_hci_cmd_opcode;
  uint16_t last_hci_evt_opcode;
  uint8_t last_hci_cmd_length;
  uint8_t last_hci_evt_code;
  uint8_t last_hci_evt_length;
  uint8_t last_hci_evt_status;
  uint8_t last_hci_evt_prefix_length;
  uint8_t last_hci_evt_prefix[FRUITJAM_ESP_HCI_DEBUG_EVT_PREFIX_SIZE];
  uint8_t last_rx_byte;
  uint8_t last_rx_error;
  uint8_t last_h4_discard_byte;
  uint8_t last_h4_type;
  uint16_t rx_ring_depth;
  uint16_t rx_ring_high_watermark;
  bool cts_ready;
} FruitJamEspHciDebugSnapshot;

void fruitjam_esp_hci_init(void);
void fruitjam_esp_hci_deinit(void);
bool fruitjam_esp_hci_read_byte(uint8_t *byte);
bool fruitjam_esp_hci_write(const uint8_t *data, size_t length);
void fruitjam_esp_passthrough_enter_bootloader(uint32_t baud);
void fruitjam_esp_passthrough_set_baud(uint32_t baud);
bool fruitjam_esp_passthrough_read_byte(uint8_t *byte);
bool fruitjam_esp_passthrough_write(const uint8_t *data, size_t length);
void fruitjam_esp_hci_debug_get_snapshot(FruitJamEspHciDebugSnapshot *snapshot);
void fruitjam_esp_hci_debug_record_h4_discard(uint8_t byte);
void fruitjam_esp_hci_debug_record_h4_parse_error(int error);
void fruitjam_esp_hci_debug_record_h4_frame(uint8_t pkt_type);
void fruitjam_esp_hci_debug_record_cmd(uint16_t opcode, uint8_t length, bool ok);
void fruitjam_esp_hci_debug_record_event(const uint8_t *event, uint16_t length);
