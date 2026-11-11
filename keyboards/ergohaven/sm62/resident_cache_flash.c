// SPDX-License-Identifier: GPL-2.0-or-later
#include "resident_cache_flash.h"
#include "vial_extension.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "compiler_support.h"
#include "flash_guard.h"
#include "layout_reserve.h"
STATIC_ASSERT(SM62_CACHE_BASE + SM62_CACHE_RECORDS * SM62_CACHE_RECORD_SIZE == 0x2ca000u, "Resident journal end");
STATIC_ASSERT(SM62_CACHE_BASE >= 0x1f2000u && 0x2ca000u <= SM62_EXT_FLASH_BASE, "Resident journal outside code and legacy settings");
STATIC_ASSERT(SM62_CACHE_BASE % FLASH_SECTOR_SIZE == 0 && SM62_CACHE_RECORD_SIZE % FLASH_SECTOR_SIZE == 0, "Cache erase alignment");
STATIC_ASSERT(FLASH_PAGE_SIZE == 256, "Cache page size");
const uint8_t *sm62_cache_flash_record(unsigned record) {
    if(record>=SM62_CACHE_RECORDS) return 0;
    return (const uint8_t *)(uintptr_t)(XIP_BASE+SM62_CACHE_BASE+record*SM62_CACHE_RECORD_SIZE);
}
bool sm62_cache_flash_erase(unsigned record) {
    if(record>=SM62_CACHE_RECORDS || !sm62_flash_begin()) return false;
    uint32_t irq=save_and_disable_interrupts();
    flash_range_erase(SM62_CACHE_BASE+record*SM62_CACHE_RECORD_SIZE,SM62_CACHE_RECORD_SIZE);
    restore_interrupts(irq);sm62_flash_end();return true;
}
bool sm62_cache_flash_program(unsigned record,unsigned offset,const uint8_t *page) {
    if(record>=SM62_CACHE_RECORDS || offset%FLASH_PAGE_SIZE || offset>SM62_CACHE_RECORD_SIZE-FLASH_PAGE_SIZE || !sm62_flash_begin()) return false;
    uint32_t irq=save_and_disable_interrupts();
    flash_range_program(SM62_CACHE_BASE+record*SM62_CACHE_RECORD_SIZE+offset,page,FLASH_PAGE_SIZE);
    restore_interrupts(irq);sm62_flash_end();return true;
}
