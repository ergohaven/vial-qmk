// SPDX-License-Identifier: GPL-2.0-or-later
#include "macro_storage.h"
#include "layout_reserve.h"
#include "wear_leveling_rp2040_flash_config.h"
#include "hardware/sync.h"
#include "flash_guard.h"
#if !defined(SM62_OLD_RENDERER) || defined(SM62_DUALCORE_RENDER)
# error This macro backend is qualified for the OLD single-core line only
#endif
STATIC_ASSERT(SM62_MACRO_FLASH_BASE == SM62_LAYOUT_RESERVE_END, "Macro banks follow resident100");
STATIC_ASSERT(SM62_MACRO_FLASH_BASE + 2*SM62_MACRO_BANK_SIZE <= SM62_LEGACY_CACHE_BASE, "Macro banks precede every legacy store");
STATIC_ASSERT(SM62_MACRO_FLASH_BASE % FLASH_SECTOR_SIZE == 0 && SM62_MACRO_BANK_SIZE == 2*FLASH_SECTOR_SIZE, "Macro erase alignment");
STATIC_ASSERT(FLASH_PAGE_SIZE == 256 && SM62_MACRO_DATA_OFFSET == FLASH_PAGE_SIZE, "Macro page geometry");
STATIC_ASSERT(SM62_MACRO_DATA_OFFSET+SM62_MACRO_SIZE <= SM62_MACRO_COMMIT_OFFSET && SM62_MACRO_COMMIT_OFFSET%FLASH_PAGE_SIZE == 0 && SM62_MACRO_COMMIT_OFFSET+FLASH_PAGE_SIZE <= SM62_MACRO_BANK_SIZE, "Macro snapshot fits");
STATIC_ASSERT(WEAR_LEVELING_RP2040_FLASH_BASE == 0x3fc000u, "EEPROM remains unchanged");
const uint8_t *sm62_macro_flash_bank(unsigned bank) {
    if (bank>=2) return 0;
    return (const uint8_t *)(uintptr_t)(XIP_BASE+SM62_MACRO_FLASH_BASE+bank*SM62_MACRO_BANK_SIZE);
}
bool sm62_macro_flash_erase(unsigned bank) {
    if (bank>=2 || !sm62_flash_begin()) return false;
    /* Separate sector-sized IRQ-off windows, matching the existing OLD
     * extension's erase bound. No recursive input/USB calls. */
    for (unsigned off=0;off<SM62_MACRO_BANK_SIZE;off+=FLASH_SECTOR_SIZE) {
        uint32_t irq=save_and_disable_interrupts();
        flash_range_erase(SM62_MACRO_FLASH_BASE+bank*SM62_MACRO_BANK_SIZE+off,FLASH_SECTOR_SIZE);
        restore_interrupts(irq);
    }
    return sm62_flash_end();
}
bool sm62_macro_flash_program(unsigned bank,unsigned offset,const uint8_t *data) {
    if (bank>=2 || offset%FLASH_PAGE_SIZE || offset>SM62_MACRO_BANK_SIZE-FLASH_PAGE_SIZE || !sm62_flash_begin()) return false;
    uint32_t irq=save_and_disable_interrupts();
    flash_range_program(SM62_MACRO_FLASH_BASE+bank*SM62_MACRO_BANK_SIZE+offset,data,FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    return sm62_flash_end();
}
