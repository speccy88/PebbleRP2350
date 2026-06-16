/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// FIXME: PBL-21049 Fix platform abstraction and board definition scheme
#ifdef CONFIG_BOARD_ASTERIX
#include "boards/board_asterix.h"
#elif defined(CONFIG_BOARD_OBELIX_DVT) || defined(CONFIG_BOARD_OBELIX_PVT) || defined(CONFIG_BOARD_OBELIX_BB2)
#include "boards/board_obelix.h"
#elif defined(CONFIG_BOARD_GETAFIX_EVT) || defined(CONFIG_BOARD_GETAFIX_DVT) || defined(CONFIG_BOARD_GETAFIX_DVT2)
#include "boards/board_getafix.h"
#elif defined(CONFIG_BOARD_QEMU_EMERY)
#include "boards/board_qemu_emery.h"
#elif defined(CONFIG_BOARD_QEMU_FLINT)
#include "boards/board_qemu_flint.h"
#elif defined(CONFIG_BOARD_QEMU_GABBRO)
#include "boards/board_qemu_gabbro.h"
#elif defined(CONFIG_BOARD_FRUITJAM_RP2350)
#include "boards/board_fruitjam_rp2350.h"
#elif defined(CONFIG_BOARD_PICO2_W_RP2350)
#include "boards/board_pico2_w_rp2350.h"
#else
#error "Unknown board definition"
#endif
