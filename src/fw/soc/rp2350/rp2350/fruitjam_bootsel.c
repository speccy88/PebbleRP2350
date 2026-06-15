/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "soc/rp2350/rp2350/fruitjam_bootsel.h"

#include "system/bootbits.h"
#include "system/reset.h"

#include <stdint.h>

#define ROM_TABLE_CODE(c1, c2) ((uint32_t)(c1) | ((uint32_t)(c2) << 8))
#define ROM_FUNC_REBOOT ROM_TABLE_CODE('R', 'B')
#define BOOTROM_TABLE_LOOKUP_OFFSET 0x16U
#define RT_FLAG_FUNC_ARM_SEC 0x0004U
#define REBOOT2_FLAG_REBOOT_TYPE_BOOTSEL 0x2U
#define REBOOT2_FLAG_NO_RETURN_ON_SUCCESS 0x100U

typedef void *(*RomTableLookupFn)(uint32_t code, uint32_t mask);
typedef int (*RomRebootFn)(uint32_t flags, uint32_t delay_ms, uint32_t param0,
                           uint32_t param1);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static uint16_t __attribute__((noinline, optimize("O0"))) prv_read_bootrom_hword(
    uintptr_t address) {
  return *(volatile uint16_t *)address;
}
#pragma GCC diagnostic pop

static bool prv_reboot_was_unsafe(const RebootReason *reason) {
  return reason->code >= RebootReasonCode_Watchdog && !reason->restarted_safely;
}

void fruitjam_bootsel_enter(void) {
  const uint16_t lookup_addr = prv_read_bootrom_hword(BOOTROM_TABLE_LOOKUP_OFFSET);
  RomTableLookupFn lookup = (RomTableLookupFn)(uintptr_t)lookup_addr;
  RomRebootFn reboot = (RomRebootFn)lookup(ROM_FUNC_REBOOT, RT_FLAG_FUNC_ARM_SEC);

  if (reboot) {
    reboot(REBOOT2_FLAG_REBOOT_TYPE_BOOTSEL | REBOOT2_FLAG_NO_RETURN_ON_SUCCESS,
           10U, 0U, 0U);
  }

  system_hard_reset();
}

void fruitjam_bootsel_clear_boot_loop_strikes(void) {
  boot_bit_clear(BOOT_BIT_FRUITJAM_BOOTSEL_STRIKE_ONE);
  boot_bit_clear(BOOT_BIT_FRUITJAM_BOOTSEL_STRIKE_TWO);
}

void fruitjam_bootsel_clear_fault_state(void) {
  reboot_reason_clear();
  boot_bit_clear(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
  fruitjam_bootsel_clear_boot_loop_strikes();
}

bool fruitjam_bootsel_should_enter_after_unsafe_boot(const RebootReason *reason) {
  boot_bit_init();

  if (!prv_reboot_was_unsafe(reason)) {
    fruitjam_bootsel_clear_boot_loop_strikes();
    return false;
  }

  if (boot_bit_test(BOOT_BIT_FRUITJAM_BOOTSEL_STRIKE_TWO)) {
    return true;
  }

  if (boot_bit_test(BOOT_BIT_FRUITJAM_BOOTSEL_STRIKE_ONE)) {
    boot_bit_set(BOOT_BIT_FRUITJAM_BOOTSEL_STRIKE_TWO);
  } else {
    boot_bit_set(BOOT_BIT_FRUITJAM_BOOTSEL_STRIKE_ONE);
  }

  return false;
}
