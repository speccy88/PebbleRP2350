/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_bt_debug.h"

#include <string.h>

void fruitjam_bt_debug_get_snapshot(FruitJamBtDebugSnapshot *snapshot) {
  memset(snapshot, 0, sizeof(*snapshot));
}

void fruitjam_bt_debug_record_discovery_request(int rc) {
  (void)rc;
}
