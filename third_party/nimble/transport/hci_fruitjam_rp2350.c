/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_esp.h"

#include <kernel/pebble_tasks.h>
#include <soc/rp2350/rp2350/fruitjam_boot_progress.h>
#include <system/logging.h>
#include <system/passert.h>

// NOTE: transport.h needs os_mbuf.h to be included first.
// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/ble.h>
#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>
#include <os/os_mempool.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stddef.h>
#include <stdint.h>

#define HCI_RX_POLL_MS 1U
#define HCI_RX_BURST_LIMIT 128U
#define HCI_TASK_STACK_SIZE 1024U
#define HCI_TASK_PRIORITY 3U
#define HCI_H4_SM_W4_PKT_TYPE 0U

static TaskHandle_t s_hci_task_handle;
static SemaphoreHandle_t s_acl_pool_avail;
static struct hci_h4_sm s_hci_h4sm;
static uint32_t s_h4_discard_count;

static bool prv_h4_controller_packet_type_valid(uint8_t type) {
  switch (type) {
    case HCI_H4_EVT:
    case HCI_H4_ACL:
    case HCI_H4_ISO:
      return true;
    default:
      return false;
  }
}

static bool prv_h4_waiting_for_packet_type(void) {
  return s_hci_h4sm.state == HCI_H4_SM_W4_PKT_TYPE;
}

static struct os_mbuf *prv_alloc_acl_from_ll(void) {
  struct os_mbuf *om = ble_transport_alloc_acl_from_ll();
  if (om != NULL) {
    return om;
  }

  PBL_LOG_D_DBG(LOG_DOMAIN_BT_STACK, "ACL pool empty, waiting for buffer");
  do {
    (void)xSemaphoreTake(s_acl_pool_avail, pdMS_TO_TICKS(100));
    om = ble_transport_alloc_acl_from_ll();
  } while (om == NULL);

  return om;
}

static os_error_t prv_acl_put_signal(struct os_mempool_ext *mpe, void *data, void *arg) {
  os_error_t err = os_memblock_put_from_cb(&mpe->mpe_mp, data);
  if (s_acl_pool_avail != NULL) {
    (void)xSemaphoreGive(s_acl_pool_avail);
  }
  return err;
}

static void *prv_alloc_evt(int discardable) {
  void *buf = ble_transport_alloc_evt(discardable);
  if (!buf) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT_STACK, "EVT alloc failed (discardable=%d)", discardable);
  }

  return buf;
}

static const struct hci_h4_allocators s_hci_h4_allocs_from_ll = {
    .acl = prv_alloc_acl_from_ll,
    .evt = prv_alloc_evt,
    .iso = ble_transport_alloc_iso_from_ll,
};

static int prv_hci_frame_cb(uint8_t pkt_type, void *data) {
  fruitjam_esp_hci_debug_record_h4_frame(pkt_type);
  switch (pkt_type) {
    case HCI_H4_EVT:
      {
        const struct ble_hci_ev *ev = data;
        fruitjam_esp_hci_debug_record_event((const uint8_t *)ev,
                                            sizeof(*ev) + ev->length);
      }
      return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
      return ble_transport_to_hs_acl(data);
    case HCI_H4_ISO:
      return ble_transport_to_hs_iso(data);
    default:
      PBL_LOG_D_ERR(LOG_DOMAIN_BT_STACK, "Unexpected H4 packet type %u", pkt_type);
      break;
  }

  return -1;
}

static void prv_hci_task_main(void *unused) {
  (void)unused;

  while (true) {
    uint8_t byte;
    uint32_t burst_count = 0;

    while (fruitjam_esp_hci_read_byte(&byte)) {
      if (prv_h4_waiting_for_packet_type() && !prv_h4_controller_packet_type_valid(byte)) {
        fruitjam_esp_hci_debug_record_h4_discard(byte);
        if (s_h4_discard_count == 0U) {
          fruitjam_boot_progress_mark_label(FruitJamBootProgressStageBluetoothStart, "HCI DROP");
        }
        ++s_h4_discard_count;
        if (s_h4_discard_count <= 4U || (s_h4_discard_count & 0x3fU) == 0U) {
          PBL_LOG_D_DBG(LOG_DOMAIN_BT_STACK, "Dropping stray H4 byte 0x%02x", byte);
        }
        continue;
      }

      const int consumed = hci_h4_sm_rx(&s_hci_h4sm, &byte, 1U);
      if (consumed <= 0) {
        fruitjam_esp_hci_debug_record_h4_parse_error(consumed);
        fruitjam_boot_progress_mark_label(FruitJamBootProgressStageBluetoothStart, "HCI RXERR");
        PBL_LOG_D_ERR(LOG_DOMAIN_BT_STACK, "hci_h4_sm_rx returned %d", consumed);
        hci_h4_sm_init(&s_hci_h4sm, &s_hci_h4_allocs_from_ll, prv_hci_frame_cb);
        break;
      }

      if (++burst_count >= HCI_RX_BURST_LIMIT) {
        taskYIELD();
        burst_count = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(HCI_RX_POLL_MS));
  }
}

void ble_transport_ll_reinit(void) {
  fruitjam_boot_progress_mark_label(FruitJamBootProgressStageBluetoothStart, "HCI INIT");
  hci_h4_sm_init(&s_hci_h4sm, &s_hci_h4_allocs_from_ll, prv_hci_frame_cb);
  fruitjam_esp_hci_init();
  fruitjam_boot_progress_mark_label(FruitJamBootProgressStageBluetoothStart, "HCI RUN");
}

void ble_transport_ll_init(void) {
  s_acl_pool_avail = xSemaphoreCreateBinary();
  PBL_ASSERTN(s_acl_pool_avail != NULL);

  ble_transport_register_put_acl_from_ll_cb(prv_acl_put_signal);
  ble_transport_ll_reinit();

  TaskParameters_t task_params = {
      .pvTaskCode = prv_hci_task_main,
      .pcName = "NimbleHCI",
      .usStackDepth = HCI_TASK_STACK_SIZE / sizeof(StackType_t),
      .uxPriority = (tskIDLE_PRIORITY + HCI_TASK_PRIORITY) | portPRIVILEGE_BIT,
      .puxStackBuffer = NULL,
  };

  pebble_task_create(PebbleTask_BTHCI, &task_params, &s_hci_task_handle);
  PBL_ASSERTN(s_hci_task_handle);
}

void ble_transport_ll_deinit(void) {
  fruitjam_esp_hci_deinit();
}

static bool prv_write_h4_packet(uint8_t type, const uint8_t *data, size_t length) {
  return fruitjam_esp_hci_write(&type, 1U) && fruitjam_esp_hci_write(data, length);
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = buf;
  int err = 0;
  const uint16_t opcode = cmd->opcode;
  const uint8_t length = cmd->length;
  const bool ok = prv_write_h4_packet(HCI_H4_CMD, (const uint8_t *)cmd,
                                      sizeof(*cmd) + cmd->length);

  fruitjam_esp_hci_debug_record_cmd(opcode, length, ok);
  if (!ok) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT_STACK, "Failed to write HCI CMD");
    err = BLE_ERR_MEM_CAPACITY;
  }

  ble_transport_free(buf);
  return err;
}

static bool prv_write_mbuf_chain(uint8_t type, struct os_mbuf *om) {
  if (!fruitjam_esp_hci_write(&type, 1U)) {
    return false;
  }

  for (struct os_mbuf *x = om; x != NULL; x = SLIST_NEXT(x, om_next)) {
    if (!fruitjam_esp_hci_write(x->om_data, x->om_len)) {
      return false;
    }
  }

  return true;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  int err = 0;

  if (!prv_write_mbuf_chain(HCI_H4_ACL, om)) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT_STACK, "Failed to write HCI ACL");
    err = BLE_ERR_MEM_CAPACITY;
  }

  os_mbuf_free_chain(om);
  return err;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  int err = 0;

  if (!prv_write_mbuf_chain(HCI_H4_ISO, om)) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT_STACK, "Failed to write HCI ISO");
    err = BLE_ERR_MEM_CAPACITY;
  }

  os_mbuf_free_chain(om);
  return err;
}
