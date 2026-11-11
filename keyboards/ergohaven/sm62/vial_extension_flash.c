// SPDX-License-Identifier: GPL-2.0-or-later
#include "vial_extension.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "wear_leveling_rp2040_flash_config.h"
#include "compiler_support.h"
#include "flash_guard.h"
STATIC_ASSERT(SM62_EXT_FLASH_BASE + 2 * SM62_EXT_BANK_SIZE == WEAR_LEVELING_RP2040_FLASH_BASE, "Extension must end before the unchanged EEPROM backing");
STATIC_ASSERT(SM62_EXT_BANK_SIZE == FLASH_SECTOR_SIZE, "Extension requires independent erase sectors");
STATIC_ASSERT(FLASH_PAGE_SIZE == 256, "Unexpected flash page size");
/* Acquire the other-core RAM rendezvous BEFORE disabling local IRQs. Current
 * single-core behavior is retained. Fail closed without erase/program when
 * core1 cannot quiesce. Page buffers must be SRAM (extension.c statics). */
const uint8_t *sm62_vial_flash_bank(unsigned bank) {
    return (const uint8_t *)(XIP_BASE + SM62_EXT_FLASH_BASE + bank * SM62_EXT_BANK_SIZE);
}
bool sm62_vial_flash_erase(unsigned bank) {
    if (bank>=2) return false;
    if (!sm62_flash_begin()) return false;
    uint32_t irq=save_and_disable_interrupts();
    flash_range_erase(SM62_EXT_FLASH_BASE+bank*SM62_EXT_BANK_SIZE,SM62_EXT_BANK_SIZE);
    restore_interrupts(irq);
    sm62_flash_end();
    return true;
}
bool sm62_vial_flash_program(unsigned bank, unsigned offset, const uint8_t *page) {
    if (bank>=2 || offset%FLASH_PAGE_SIZE || offset+FLASH_PAGE_SIZE>SM62_EXT_BANK_SIZE) return false;
    if (!sm62_flash_begin()) return false;
    uint32_t irq=save_and_disable_interrupts();
    flash_range_program(SM62_EXT_FLASH_BASE+bank*SM62_EXT_BANK_SIZE+offset,page,FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    sm62_flash_end();
    return true;
}
