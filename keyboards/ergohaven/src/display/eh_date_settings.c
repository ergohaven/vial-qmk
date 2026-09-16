#include "eh_date_settings.h"
#include "quantum.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include <string.h>
// Dedicated two-sector journal in the unused gap between firmware and splash.
// Does NOT enlarge EEPROM or move any Vial keymap/macro addresses.
#define EXTRA_BASE 0x000B0000u
#define EXTRA_PAGES (2 * FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)
#define EXTRA_MAGIC 0x31584445u
_Static_assert(EXTRA_BASE >= 0xB0000u && EXTRA_BASE + 2 * FLASH_SECTOR_SIZE <= 0xC0000u, "extra settings overlap");
typedef struct { uint32_t magic, sequence, crc; uint8_t data[64]; uint8_t padding[180]; } record_t;
_Static_assert(sizeof(record_t) == FLASH_PAGE_SIZE, "journal page size");
static const uint8_t defaults[] = {0,100,255,255,255,0,1,1,0,7,100,1,255,255,255};
static uint8_t values[64];
static bool initialized, dirty;
static uint32_t sequence, changed_at;
static int newest = -1;
static uint32_t checksum(const record_t *r) {
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned i = 0; i < 4 + sizeof(r->data); i++) {
        uint8_t byte = i < 4 ? (uint8_t)(r->sequence >> (8 * i)) : r->data[i - 4];
        crc ^= byte;
        for (unsigned bit=0; bit<8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1));
    }
    return crc ^ 0xFFFFFFFFu;
}
static void init(void) {
    if (initialized) return;
    initialized = true;
    memset(values, 0xFF, sizeof(values));
    memcpy(values, defaults, sizeof(defaults));
    const record_t *records = (const record_t *)(XIP_BASE + EXTRA_BASE);
    for (unsigned i=0; i<EXTRA_PAGES; i++) {
        const record_t *r = &records[i];
        if (r->magic == EXTRA_MAGIC && r->crc == checksum(r) && (newest < 0 || r->sequence > sequence)) {
            newest = i; sequence = r->sequence; memcpy(values,r->data,sizeof(values));
        }
    }
    // Older journals leave these formerly unused bytes erased.
    if(values[10]>100) values[10]=100;
    if(values[11]>1) { values[11]=1; values[12]=values[13]=values[14]=255; }
}
uint8_t eh_date_get(uint8_t index) { init(); return index < 15 ? values[index] : 0; }
bool eh_date_set(uint8_t index, uint8_t value) {
    static const uint8_t maximum[] = {1,100,255,255,255,9,3,2,2,7,100,1,255,255,255};
    if (index >= 15 || value > maximum[index]) return false;
    eh_extra_settings_write(index,&value,1); return true;
}
void eh_extra_settings_read(uint8_t offset, void *data, uint8_t size) {
    init(); if (offset + size <= sizeof(values)) memcpy(data,values+offset,size);
}
void eh_extra_settings_write(uint8_t offset, const void *data, uint8_t size) {
    init(); if (offset + size > sizeof(values) || !memcmp(values+offset,data,size)) return;
    memcpy(values+offset,data,size); dirty = true; changed_at = timer_read32();
}
void eh_extra_settings_flush(void) {
    if (!dirty) return;
    unsigned next = (newest + 1) % EXTRA_PAGES;
    record_t record; memset(&record,0xFF,sizeof(record));
    record.magic=EXTRA_MAGIC; record.sequence=sequence+1; memcpy(record.data,values,sizeof(values)); record.crc=checksum(&record);
    const uint8_t *target = (const uint8_t *)(XIP_BASE + EXTRA_BASE + next * FLASH_PAGE_SIZE);
    bool erased=true;
    for(unsigned i=0;i<FLASH_PAGE_SIZE;i++) if(target[i]!=0xFF) { erased=false; break; }
    // Skip a torn page to the opposite sector, retaining the last valid record.
    if (!erased && next % (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE) != 0)
        next = ((newest / (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE) + 1) % 2) * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE);
    uint32_t irq=save_and_disable_interrupts();
    if (next % (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE) == 0)
        flash_range_erase(EXTRA_BASE + next * FLASH_PAGE_SIZE,FLASH_SECTOR_SIZE);
    flash_range_program(EXTRA_BASE + next * FLASH_PAGE_SIZE,(const uint8_t *)&record,FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    const record_t *stored=(const record_t *)(XIP_BASE+EXTRA_BASE+next*FLASH_PAGE_SIZE);
    if(stored->magic==EXTRA_MAGIC && stored->crc==checksum(stored)) { newest=next; sequence=record.sequence; dirty=false; }
}

void eh_date_reset(void) {
    eh_extra_settings_write(0, defaults, sizeof(defaults));
}
void eh_extra_settings_housekeep(void) {
    if (timer_elapsed32(changed_at) >= 350) eh_extra_settings_flush();
}
