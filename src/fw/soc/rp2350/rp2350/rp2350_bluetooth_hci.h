/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const char *rp2350_bluetooth_hci_backend_name(void);
void rp2350_bluetooth_hci_init(void);
void rp2350_bluetooth_hci_deinit(void);
bool rp2350_bluetooth_hci_read_byte(uint8_t *byte);
bool rp2350_bluetooth_hci_write(const uint8_t *data, size_t length);
void rp2350_bluetooth_hci_debug_record_h4_discard(uint8_t byte);
void rp2350_bluetooth_hci_debug_record_h4_parse_error(int error);
void rp2350_bluetooth_hci_debug_record_h4_frame(uint8_t pkt_type);
void rp2350_bluetooth_hci_debug_record_cmd(uint16_t opcode, uint8_t length, bool ok);
void rp2350_bluetooth_hci_debug_record_event(const uint8_t *event, uint16_t length);
