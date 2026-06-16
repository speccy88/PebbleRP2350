/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/rp2350_bluetooth_hci.h"

#include "soc/rp2350/rp2350/fruitjam_esp.h"

const char *rp2350_bluetooth_hci_backend_name(void) {
  return "fruitjam-esp32c6-h4";
}

void rp2350_bluetooth_hci_init(void) {
  fruitjam_esp_hci_init();
}

void rp2350_bluetooth_hci_deinit(void) {
  fruitjam_esp_hci_deinit();
}

bool rp2350_bluetooth_hci_read_byte(uint8_t *byte) {
  return fruitjam_esp_hci_read_byte(byte);
}

bool rp2350_bluetooth_hci_write(const uint8_t *data, size_t length) {
  return fruitjam_esp_hci_write(data, length);
}

void rp2350_bluetooth_hci_debug_record_h4_discard(uint8_t byte) {
  fruitjam_esp_hci_debug_record_h4_discard(byte);
}

void rp2350_bluetooth_hci_debug_record_h4_parse_error(int error) {
  fruitjam_esp_hci_debug_record_h4_parse_error(error);
}

void rp2350_bluetooth_hci_debug_record_h4_frame(uint8_t pkt_type) {
  fruitjam_esp_hci_debug_record_h4_frame(pkt_type);
}

void rp2350_bluetooth_hci_debug_record_cmd(uint16_t opcode, uint8_t length, bool ok) {
  fruitjam_esp_hci_debug_record_cmd(opcode, length, ok);
}

void rp2350_bluetooth_hci_debug_record_event(const uint8_t *event, uint16_t length) {
  fruitjam_esp_hci_debug_record_event(event, length);
}
