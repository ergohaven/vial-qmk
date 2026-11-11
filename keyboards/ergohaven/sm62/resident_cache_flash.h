#pragma once
#include <stdint.h>
#include <stdbool.h>
#define SM62_CACHE_BASE 0x200000u
#define SM62_CACHE_RECORD_SIZE 8192u
#define SM62_CACHE_RECORDS 101u
#define SM62_CACHE_SLOTS 100u
#define SM62_LEGACY_CACHE_BASE 0x3f2000u
#define SM62_LEGACY_CACHE_END 0x3fa000u
const uint8_t *sm62_cache_flash_record(unsigned record);
bool sm62_cache_flash_erase(unsigned record);
bool sm62_cache_flash_program(unsigned record, unsigned offset, const uint8_t *page);
