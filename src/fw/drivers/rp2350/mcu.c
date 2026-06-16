/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/mcu.h"

#include <cmsis_core.h>

#define ROM_TABLE_CODE(c1, c2) ((uint32_t)(c1) | ((uint32_t)(c2) << 8))
#define ROM_FUNC_GET_SYS_INFO ROM_TABLE_CODE('G', 'S')
#define BOOTROM_TABLE_LOOKUP_OFFSET 0x16U
#define RT_FLAG_FUNC_ARM_SEC 0x0004U
#define SYS_INFO_CHIP_INFO 0x0001U
#define RP2350_CHIP_ID_SIZE 8U

typedef void *(*RomTableLookupFn)(uint32_t code, uint32_t mask);
typedef int (*RomGetSysInfoFn)(uint32_t *out_buffer, uint32_t out_buffer_word_size,
                               uint32_t flags);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static uint16_t __attribute__((noinline, optimize("O0"))) prv_read_bootrom_hword(
    uintptr_t address) {
  return *(volatile uint16_t *)address;
}
#pragma GCC diagnostic pop

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  if (!buf || !buf_sz) {
    return E_INVALID_ARGUMENT;
  }

  if (*buf_sz < RP2350_CHIP_ID_SIZE) {
    return E_OUT_OF_MEMORY;
  }

  const uint16_t lookup_addr = prv_read_bootrom_hword(BOOTROM_TABLE_LOOKUP_OFFSET);
  RomTableLookupFn lookup = (RomTableLookupFn)(uintptr_t)lookup_addr;
  if (!lookup) {
    return E_DOES_NOT_EXIST;
  }

  RomGetSysInfoFn get_sys_info =
      (RomGetSysInfoFn)lookup(ROM_FUNC_GET_SYS_INFO, RT_FLAG_FUNC_ARM_SEC);
  if (!get_sys_info) {
    return E_DOES_NOT_EXIST;
  }

  union {
    uint32_t words[9];
    uint8_t bytes[9 * sizeof(uint32_t)];
  } out = {0};

  const int words_returned = get_sys_info(out.words, 9, SYS_INFO_CHIP_INFO);
  if (words_returned < 4) {
    return E_ERROR;
  }

  uint8_t *serial = buf;
  for (size_t i = 0; i < RP2350_CHIP_ID_SIZE; ++i) {
    serial[i] = out.bytes[RP2350_CHIP_ID_SIZE - 1U + (2U * sizeof(uint32_t)) - i];
  }
  *buf_sz = RP2350_CHIP_ID_SIZE;

  return S_SUCCESS;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  return ((cpu_ticks * 1000) / SystemCoreClock);
}
