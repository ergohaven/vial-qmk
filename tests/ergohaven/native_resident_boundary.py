"""NOR boundary for rendering/processor fixtures that never store profiles.

Use the actual geometry/declarations; reads see erased NOR. Any accidental
write aborts instead of silently returning fabricated persistence success.
Persistence acceptance uses resident100_native.c's writable NOR model instead.
"""
READONLY_FLASH_SOURCE = r'''
#include "sm62/resident_cache_flash.h"
const uint8_t *sm62_cache_flash_record(unsigned r){static const uint8_t erased[SM62_CACHE_RECORD_SIZE]={[0 ... SM62_CACHE_RECORD_SIZE-1]=0xff};return r<SM62_CACHE_RECORDS?erased:NULL;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t *p){(void)r;(void)off;(void)p;assert(!"read-only fixture must not write resident flash");return false;}
bool sm62_cache_flash_erase(unsigned r){(void)r;assert(!"read-only fixture must not write resident flash");return false;}
'''
