/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_esp.h"

#include <string.h>

void fruitjam_esp_hci_init(void) {}

void fruitjam_esp_hci_deinit(void) {}

bool fruitjam_esp_hci_read_byte(uint8_t *byte) {
  (void)byte;
  return false;
}

bool fruitjam_esp_hci_write(const uint8_t *data, size_t length) {
  (void)data;
  (void)length;
  return false;
}

void fruitjam_esp_passthrough_enter_bootloader(uint32_t baud) {
  (void)baud;
}

void fruitjam_esp_passthrough_set_baud(uint32_t baud) {
  (void)baud;
}

bool fruitjam_esp_passthrough_read_byte(uint8_t *byte) {
  (void)byte;
  return false;
}

bool fruitjam_esp_passthrough_write(const uint8_t *data, size_t length) {
  (void)data;
  (void)length;
  return false;
}

void fruitjam_esp_hci_debug_get_snapshot(FruitJamEspHciDebugSnapshot *snapshot) {
  memset(snapshot, 0, sizeof(*snapshot));
}

void fruitjam_esp_hci_debug_record_h4_discard(uint8_t byte) {
  (void)byte;
}

void fruitjam_esp_hci_debug_record_h4_parse_error(int error) {
  (void)error;
}

void fruitjam_esp_hci_debug_record_h4_frame(uint8_t pkt_type) {
  (void)pkt_type;
}

void fruitjam_esp_hci_debug_record_cmd(uint16_t opcode, uint8_t length, bool ok) {
  (void)opcode;
  (void)length;
  (void)ok;
}

void fruitjam_esp_hci_debug_record_event(const uint8_t *event, uint16_t length) {
  (void)event;
  (void)length;
}
