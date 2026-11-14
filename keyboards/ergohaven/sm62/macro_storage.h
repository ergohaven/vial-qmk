// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* OLD-only overlay. Historical EEPROM [2968,8192) remains byte-identical.
 * Resident100 ends at 0x2ca000. Two new independent 8KiB COW banks occupy
 * [0x2ca000,0x2ce000), in previously unused 4MiB flash. */
#define SM62_MACRO_FLASH_BASE 0x2ca000u
#define SM62_MACRO_BANK_SIZE 8192u
#define SM62_MACRO_SIZE 6066u
#define SM62_MACRO_LEGACY_SIZE 5224u
#define SM62_MACRO_DATA_OFFSET 256u
#define SM62_MACRO_COMMIT_OFFSET 6400u
bool sm62_macro_read(uint32_t offset, uint32_t size, uint8_t *data);
bool sm62_macro_update(uint32_t offset, uint32_t size, const uint8_t *data);
bool sm62_macro_reset(void);
/* Void QMK NVM wrappers must not disguise a failed durable operation. */
bool sm62_macro_ok(void);
/* Playback owns a read-only snapshot only for its synchronous call. No flash
 * writes/reentrant readers are allowed inside this lease. */
bool sm62_macro_read_begin(void);
void sm62_macro_read_end(void);
/* Hardware/legacy boundaries; all format/COW logic lives in macro_storage.c. */
const uint8_t *sm62_macro_flash_bank(unsigned bank);
bool sm62_macro_flash_erase(unsigned bank);
bool sm62_macro_flash_program(unsigned bank, unsigned offset, const uint8_t *page);
void sm62_macro_legacy_read(uint32_t offset, uint32_t size, uint8_t *data);
