// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define SM62_EXT_BANK_SIZE 4096U
#define SM62_EXT_DATA_SIZE 840U
#define SM62_EXT_FLASH_BASE 0x3fa000U
int sm62_vial_ext_get(uint8_t kind, uint8_t index, void *entry);
int sm62_vial_ext_set(uint8_t kind, uint8_t index, const void *entry);
int sm62_vial_ext_reset(void);
/* Checked reset is target-local; the existing global void API is unchanged. */
bool dynamic_keymap_reset_checked(void);
/* Backend owns exactly two independent erase sectors. XIP reads are immutable
 * while these main-thread-only APIs run. No EEPROM calls are permitted here. */
const uint8_t *sm62_vial_flash_bank(unsigned bank);
bool sm62_vial_flash_erase(unsigned bank);
bool sm62_vial_flash_program(unsigned bank, unsigned offset, const uint8_t *page);

// Audited historical physical section size; advertised capacity is independent.
#define SM62_VIAL_LEGACY_ENTRIES 4U
