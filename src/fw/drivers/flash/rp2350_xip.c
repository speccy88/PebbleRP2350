/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash/flash_impl.h"

#include "util/attributes.h"

#include <cmsis_core.h>
#include <stdbool.h>
#include <string.h>

#define RP2350_FLASH_BASE 0x10000000u
#define RP2350_FLASH_SIZE (16u * 1024u * 1024u)
#define RP2350_SECTOR_SIZE 0x10000u
#define RP2350_SUBSECTOR_SIZE 0x1000u
#define RP2350_PAGE_SIZE 256u

#define XIP_QMI_BASE 0x400d0000u
#define QMI_M1_TIMING_OFFSET 0x20u
#define QMI_M1_RFMT_OFFSET 0x24u
#define QMI_M1_RCMD_OFFSET 0x28u
#define QMI_M1_WFMT_OFFSET 0x2cu
#define QMI_M1_WCMD_OFFSET 0x30u

#define BOOTROM_TABLE_LOOKUP_OFFSET 0x16u
#define RT_FLAG_FUNC_ARM_SEC 0x0004u
#define ROM_TABLE_CODE(c1, c2) ((uint32_t)(c1) | ((uint32_t)(c2) << 8))
#define ROM_FUNC_FLASH_ENTER_CMD_XIP ROM_TABLE_CODE('C', 'X')
#define ROM_FUNC_FLASH_EXIT_XIP ROM_TABLE_CODE('E', 'X')
#define ROM_FUNC_FLASH_FLUSH_CACHE ROM_TABLE_CODE('F', 'C')
#define ROM_FUNC_CONNECT_INTERNAL_FLASH ROM_TABLE_CODE('I', 'F')
#define ROM_FUNC_FLASH_RANGE_ERASE ROM_TABLE_CODE('R', 'E')
#define ROM_FUNC_FLASH_RANGE_PROGRAM ROM_TABLE_CODE('R', 'P')

#define FLASH_SUBSECTOR_ERASE_CMD 0x20u
#define FLASH_BLOCK_ERASE_CMD 0xd8u

#define RAMFUNC NOINLINE SECTION(".ramfunc")
#define REG32(addr) (*(volatile uint32_t *)(addr))

typedef void *(*RomTableLookupFn)(uint32_t code, uint32_t mask);
typedef void (*RomConnectInternalFlashFn)(void);
typedef void (*RomFlashExitXipFn)(void);
typedef void (*RomFlashRangeEraseFn)(uint32_t offset, size_t count,
                                     uint32_t block_size,
                                     uint8_t block_cmd);
typedef void (*RomFlashRangeProgramFn)(uint32_t offset, const uint8_t *data,
                                       size_t count);
typedef void (*RomFlashFlushCacheFn)(void);
typedef void (*RomFlashEnterCmdXipFn)(void);

typedef enum FlashOp {
  FlashOpErase,
  FlashOpProgram,
} FlashOp;

typedef struct QmiCs1State {
  uint32_t timing;
  uint32_t rfmt;
  uint32_t rcmd;
  uint32_t wfmt;
  uint32_t wcmd;
} QmiCs1State;

static const FlashSecurityRegisters s_security_regs = {
    .sec_regs = NULL,
    .num_sec_regs = 0,
    .sec_reg_size = 0,
};

static uint8_t s_page_buffer[RP2350_PAGE_SIZE] ALIGN(4);
static status_t s_last_write_status = S_SUCCESS;
static status_t s_last_erase_status = S_SUCCESS;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static uint16_t RAMFUNC prv_read_bootrom_hword(uintptr_t address) {
  return *(volatile uint16_t *)address;
}
#pragma GCC diagnostic pop

static void *RAMFUNC prv_rom_func_lookup(uint32_t code) {
  const uint16_t lookup_addr = prv_read_bootrom_hword(BOOTROM_TABLE_LOOKUP_OFFSET);
  RomTableLookupFn lookup = (RomTableLookupFn)(uintptr_t)lookup_addr;

  return lookup(code, RT_FLAG_FUNC_ARM_SEC);
}

static void RAMFUNC prv_save_qmi_cs1(QmiCs1State *state) {
  state->timing = REG32(XIP_QMI_BASE + QMI_M1_TIMING_OFFSET);
  state->rfmt = REG32(XIP_QMI_BASE + QMI_M1_RFMT_OFFSET);
  state->rcmd = REG32(XIP_QMI_BASE + QMI_M1_RCMD_OFFSET);
  state->wfmt = REG32(XIP_QMI_BASE + QMI_M1_WFMT_OFFSET);
  state->wcmd = REG32(XIP_QMI_BASE + QMI_M1_WCMD_OFFSET);
}

static void RAMFUNC prv_restore_qmi_cs1(const QmiCs1State *state) {
  REG32(XIP_QMI_BASE + QMI_M1_TIMING_OFFSET) = state->timing;
  REG32(XIP_QMI_BASE + QMI_M1_RFMT_OFFSET) = state->rfmt;
  REG32(XIP_QMI_BASE + QMI_M1_RCMD_OFFSET) = state->rcmd;
  REG32(XIP_QMI_BASE + QMI_M1_WFMT_OFFSET) = state->wfmt;
  REG32(XIP_QMI_BASE + QMI_M1_WCMD_OFFSET) = state->wcmd;
}

static status_t RAMFUNC prv_flash_op(FlashOp op, uint32_t offset,
                                     const uint8_t *data, size_t count,
                                     uint32_t erase_size,
                                     uint8_t erase_cmd) {
  RomConnectInternalFlashFn connect =
      (RomConnectInternalFlashFn)prv_rom_func_lookup(ROM_FUNC_CONNECT_INTERNAL_FLASH);
  RomFlashExitXipFn exit_xip = (RomFlashExitXipFn)prv_rom_func_lookup(ROM_FUNC_FLASH_EXIT_XIP);
  RomFlashFlushCacheFn flush =
      (RomFlashFlushCacheFn)prv_rom_func_lookup(ROM_FUNC_FLASH_FLUSH_CACHE);
  RomFlashEnterCmdXipFn enter_xip =
      (RomFlashEnterCmdXipFn)prv_rom_func_lookup(ROM_FUNC_FLASH_ENTER_CMD_XIP);
  RomFlashRangeEraseFn erase = (RomFlashRangeEraseFn)prv_rom_func_lookup(
      ROM_FUNC_FLASH_RANGE_ERASE);
  RomFlashRangeProgramFn program =
      (RomFlashRangeProgramFn)prv_rom_func_lookup(ROM_FUNC_FLASH_RANGE_PROGRAM);

  if (!connect || !exit_xip || !flush || !enter_xip) {
    return E_INVALID_OPERATION;
  }
  if ((op == FlashOpErase && !erase) || (op == FlashOpProgram && !program)) {
    return E_INVALID_OPERATION;
  }

  QmiCs1State cs1_state;
  prv_save_qmi_cs1(&cs1_state);

  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  __DSB();
  __ISB();

  connect();
  exit_xip();
  if (op == FlashOpErase) {
    erase(offset, count, erase_size, erase_cmd);
  } else {
    program(offset, data, count);
  }
  flush();
  enter_xip();
  prv_restore_qmi_cs1(&cs1_state);

  __DSB();
  __ISB();
  __set_PRIMASK(primask);

  return S_SUCCESS;
}

static bool prv_range_is_valid(FlashAddress addr, size_t len) {
  if (addr < RP2350_FLASH_BASE) {
    return false;
  }

  const uint32_t offset = addr - RP2350_FLASH_BASE;
  return offset <= RP2350_FLASH_SIZE && len <= (RP2350_FLASH_SIZE - offset);
}

static status_t prv_blank_check(FlashAddress addr, size_t len) {
  if (!prv_range_is_valid(addr, len)) {
    return E_RANGE;
  }

  const uint8_t *flash = (const uint8_t *)(uintptr_t)addr;
  for (size_t i = 0; i < len; ++i) {
    if (flash[i] != 0xffu) {
      return S_FALSE;
    }
  }

  return S_TRUE;
}

status_t flash_impl_init(bool coredump_mode) {
  (void)coredump_mode;
  return S_SUCCESS;
}

status_t flash_impl_set_burst_mode(bool enable) {
  (void)enable;
  return S_SUCCESS;
}

FlashAddress flash_impl_get_sector_base_address(FlashAddress addr) {
  return addr & ~(RP2350_SECTOR_SIZE - 1u);
}

FlashAddress flash_impl_get_subsector_base_address(FlashAddress addr) {
  return addr & ~(RP2350_SUBSECTOR_SIZE - 1u);
}

size_t flash_impl_get_capacity(void) {
  return RP2350_FLASH_SIZE;
}

status_t flash_impl_enter_low_power_mode(void) {
  return S_SUCCESS;
}

status_t flash_impl_exit_low_power_mode(void) {
  return S_SUCCESS;
}

status_t flash_impl_read_sync(void *buffer, FlashAddress addr, size_t len) {
  if (!prv_range_is_valid(addr, len)) {
    return E_RANGE;
  }

  memcpy(buffer, (const void *)(uintptr_t)addr, len);
  return S_SUCCESS;
}

status_t flash_impl_read_dma_begin(void *buffer, FlashAddress addr, size_t len) {
  status_t result = flash_impl_read_sync(buffer, addr, len);
  flash_impl_on_read_dma_complete_from_isr(result);
  return result;
}

void flash_impl_enable_write_protection(void) {
}

status_t flash_impl_write_protect(FlashAddress start_sector,
                                  FlashAddress end_sector) {
  (void)start_sector;
  (void)end_sector;
  return S_SUCCESS;
}

status_t flash_impl_unprotect(void) {
  return S_SUCCESS;
}

int flash_impl_write_page_begin(const void *buffer, FlashAddress addr,
                                size_t len) {
  if (!buffer || len == 0) {
    s_last_write_status = E_INVALID_ARGUMENT;
    return s_last_write_status;
  }
  if (!prv_range_is_valid(addr, len)) {
    s_last_write_status = E_RANGE;
    return s_last_write_status;
  }

  const FlashAddress page_addr = addr & ~(RP2350_PAGE_SIZE - 1u);
  const size_t page_offset = addr - page_addr;
  const size_t write_len =
      (len < (RP2350_PAGE_SIZE - page_offset)) ? len : (RP2350_PAGE_SIZE - page_offset);
  const uint8_t *src = (const uint8_t *)buffer;
  const uint8_t *flash_page = (const uint8_t *)(uintptr_t)page_addr;

  memcpy(s_page_buffer, flash_page, sizeof(s_page_buffer));
  for (size_t i = 0; i < write_len; ++i) {
    s_page_buffer[page_offset + i] = flash_page[page_offset + i] & src[i];
  }

  const uint32_t offset = page_addr - RP2350_FLASH_BASE;
  s_last_write_status =
      prv_flash_op(FlashOpProgram, offset, s_page_buffer, sizeof(s_page_buffer),
                   0, 0);
  if (FAILED(s_last_write_status)) {
    return s_last_write_status;
  }

  if (memcmp(flash_page, s_page_buffer, sizeof(s_page_buffer)) != 0) {
    s_last_write_status = E_ERROR;
    return s_last_write_status;
  }

  return (int)write_len;
}

status_t flash_impl_get_write_status(void) {
  return s_last_write_status;
}

status_t flash_impl_write_suspend(FlashAddress addr) {
  (void)addr;
  return S_NO_ACTION_REQUIRED;
}

status_t flash_impl_write_resume(FlashAddress addr) {
  (void)addr;
  return S_NO_ACTION_REQUIRED;
}

status_t flash_impl_erase_subsector_begin(FlashAddress subsector_addr) {
  const FlashAddress base = flash_impl_get_subsector_base_address(subsector_addr);
  if (!prv_range_is_valid(base, RP2350_SUBSECTOR_SIZE)) {
    s_last_erase_status = E_RANGE;
    return s_last_erase_status;
  }

  s_last_erase_status =
      prv_flash_op(FlashOpErase, base - RP2350_FLASH_BASE, NULL,
                   RP2350_SUBSECTOR_SIZE, RP2350_SUBSECTOR_SIZE,
                   FLASH_SUBSECTOR_ERASE_CMD);
  return s_last_erase_status;
}

status_t flash_impl_erase_sector_begin(FlashAddress sector_addr) {
  const FlashAddress base = flash_impl_get_sector_base_address(sector_addr);
  if (!prv_range_is_valid(base, RP2350_SECTOR_SIZE)) {
    s_last_erase_status = E_RANGE;
    return s_last_erase_status;
  }

  s_last_erase_status =
      prv_flash_op(FlashOpErase, base - RP2350_FLASH_BASE, NULL,
                   RP2350_SECTOR_SIZE, RP2350_SECTOR_SIZE,
                   FLASH_BLOCK_ERASE_CMD);
  return s_last_erase_status;
}

status_t flash_impl_erase_bulk_begin(void) {
  return E_INVALID_OPERATION;
}

status_t flash_impl_get_erase_status(void) {
  return s_last_erase_status;
}

uint32_t flash_impl_get_typical_subsector_erase_duration_ms(void) {
  return 50;
}

uint32_t flash_impl_get_typical_sector_erase_duration_ms(void) {
  return 150;
}

status_t flash_impl_erase_suspend(FlashAddress addr) {
  (void)addr;
  return S_NO_ACTION_REQUIRED;
}

status_t flash_impl_erase_resume(FlashAddress addr) {
  (void)addr;
  return S_NO_ACTION_REQUIRED;
}

status_t flash_impl_blank_check_subsector(FlashAddress addr) {
  return prv_blank_check(flash_impl_get_subsector_base_address(addr), RP2350_SUBSECTOR_SIZE);
}

status_t flash_impl_blank_check_sector(FlashAddress addr) {
  return prv_blank_check(flash_impl_get_sector_base_address(addr), RP2350_SECTOR_SIZE);
}

void flash_impl_use(void) {
}

void flash_impl_release(void) {
}

void flash_impl_release_many(uint32_t num_locks) {
  (void)num_locks;
}

status_t flash_impl_read_security_register(uint32_t addr, uint8_t *val) {
  (void)addr;
  *val = 0xff;
  return S_SUCCESS;
}

status_t flash_impl_security_register_is_locked(uint32_t address, bool *locked) {
  (void)address;
  *locked = false;
  return S_SUCCESS;
}

status_t flash_impl_erase_security_register(uint32_t addr) {
  (void)addr;
  return E_ERROR;
}

status_t flash_impl_write_security_register(uint32_t addr, uint8_t val) {
  (void)addr;
  (void)val;
  return E_ERROR;
}

const FlashSecurityRegisters *flash_impl_security_registers_info(void) {
  return &s_security_regs;
}
