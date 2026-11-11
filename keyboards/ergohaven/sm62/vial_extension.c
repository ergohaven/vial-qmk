// SPDX-License-Identifier: GPL-2.0-or-later
/* SM62 append-only capacity extension: never relocates or writes legacy EEPROM.
 * Two independently erasable 4K sectors; each snapshot is four data pages plus
 * one commit page. Never erase the selected bank. The commit page is programmed
 * last, only after byte-for-byte data readback. A cut during erase/program leaves
 * the preceding committed bank authoritative. This is not a repair for QMK's
 * existing EEPROM consolidation power-loss window. Main thread only. */
#include "vial_extension.h"
#include <string.h>
#ifndef TAPPING_TERM
#    define TAPPING_TERM 200
#endif
static bool initialized;
static int active_bank;
static uint32_t generation;
/* Static: QMK's process stack is only 2K. Not reentrant. */
static uint8_t image[1024];
static uint8_t commit_page[256];
static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void store32(uint8_t *p, uint32_t v) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
static uint32_t image_crc(const uint8_t *p) {
    uint32_t crc=~0U;
    for (unsigned i=0;i<1024;i++) {
        crc ^= i>=12 && i<16 ? 0 : p[i];
        for (unsigned b=0;b<8;b++) crc=(crc>>1)^((0U-(crc&1U))&0xedb88320U);
    }
    return ~crc;
}
static bool bank_valid(unsigned bank) {
    const uint8_t *p=sm62_vial_flash_bank(bank);
    for (unsigned i=1024;i<1280;i++) if (p[i]!=0) return false;
    return !memcmp(p,"SMX1\1\0\0\0",8) && load32(p+12)==image_crc(p);
}
static void ensure_initialized(void) {
    if (initialized) return;
    bool a=bank_valid(0), b=bank_valid(1);
    active_bank=-1; generation=0;
    if (a) active_bank=0;
    /* Unsigned serial arithmetic; adjacent committed banks differ by one. */
    if (b && (!a || (uint32_t)(load32(sm62_vial_flash_bank(1)+8)-load32(sm62_vial_flash_bank(0)+8))==1U)) active_bank=1;
    if (active_bank>=0) generation=load32(sm62_vial_flash_bank((unsigned)active_bank)+8);
    initialized=true;
}
static int slot_offset(uint8_t kind, uint8_t index) {
    if (kind>=3 || index<4 || index>=32) return -1;
    return 32+(kind*28+index-4)*10;
}
static void default_entry(uint8_t kind, uint8_t *p) {
    memset(p,0,10);
    if (kind==0) {p[8]=(uint8_t)TAPPING_TERM;p[9]=(uint8_t)(TAPPING_TERM>>8);}
    if (kind==2) {p[4]=255;p[5]=255;p[9]=7;} // QMK dynamic_keymap_reset defaults
}
int sm62_vial_ext_get(uint8_t kind, uint8_t index, void *entry) {
#ifdef SM62_RESERVED_TD_BYTES
    if (kind == 0) return -1;
#endif
    int off=slot_offset(kind,index);
    if (off<0) return -1;
    ensure_initialized();
    if (active_bank<0) default_entry(kind,entry);
    else memcpy(entry,sm62_vial_flash_bank((unsigned)active_bank)+off,10);
    return 0;
}
static int commit_image(void);
/* Reset all 84 added slots in one atomic snapshot, not separate SETs. An
 * absent bank already represents these defaults and must not cause a write. */
int sm62_vial_ext_reset(void) {
    ensure_initialized();
    if (active_bank < 0) return 0;
    memcpy(image,sm62_vial_flash_bank((unsigned)active_bank),sizeof(image));
    bool changed=false;
    for (unsigned k=0;k<3;k++) for (unsigned i=4;i<32;i++) {
        uint8_t entry[10];
        default_entry(k,entry);
        uint8_t *p=image+slot_offset(k,i);
        if (memcmp(p,entry,10)) {memcpy(p,entry,10);changed=true;}
    }
    return changed ? commit_image() : 0;
}
int sm62_vial_ext_set(uint8_t kind, uint8_t index, const void *entry) {
#ifdef SM62_RESERVED_TD_BYTES
    if (kind == 0) return -1;
#endif
    int off=slot_offset(kind,index);
    if (off<0) return -1;
    uint8_t previous[10];
    sm62_vial_ext_get(kind,index,previous);
    if (!memcmp(previous,entry,10)) return 0;
    if (active_bank<0) {
        memset(image,255,sizeof(image));
        for (unsigned k=0;k<3;k++) for(unsigned i=4;i<32;i++) default_entry(k,image+slot_offset(k,i));
    } else memcpy(image,sm62_vial_flash_bank((unsigned)active_bank),sizeof(image));
    memcpy(image+off,entry,10);
    return commit_image();
}
static int commit_image(void) {
    memcpy(image,"SMX1\1\0\0\0",8);
    store32(image+8,generation+1U);
    store32(image+12,image_crc(image));
    unsigned target=active_bank==0?1:0;
    initialized=false; // all failures must re-read actual committed state
    if (!sm62_vial_flash_erase(target)) return -1;
    for (unsigned i=0;i<sizeof(image);i+=256) if (!sm62_vial_flash_program(target,i,image+i)) return -1;
    if (memcmp(image,sm62_vial_flash_bank(target),sizeof(image))) return -1;
    memset(commit_page,0,sizeof(commit_page));
    if (!sm62_vial_flash_program(target,1024,commit_page) || !bank_valid(target)) return -1;
    ensure_initialized();
    return active_bank==(int)target ? 0 : -1;
}
