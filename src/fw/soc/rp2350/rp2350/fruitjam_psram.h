/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#define FRUITJAM_PSRAM_BASE 0x11000000U
#define FRUITJAM_PSRAM_NOCACHE_BASE 0x15000000U
#define FRUITJAM_PSRAM_EXPECTED_SIZE (8U * 1024U * 1024U)

void fruitjam_psram_init(void);
size_t fruitjam_psram_get_size(void);
bool fruitjam_psram_is_available(void);
