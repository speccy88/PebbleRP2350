/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <os/os_mbuf.h>
#include <os/os_mempool.h>
#include <nimble/transport.h>

void ble_transport_ll_init(void) {
}

void ble_transport_ll_deinit(void) {
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  ble_transport_free(buf);
  return -1;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  os_mbuf_free_chain(om);
  return -1;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  os_mbuf_free_chain(om);
  return -1;
}
