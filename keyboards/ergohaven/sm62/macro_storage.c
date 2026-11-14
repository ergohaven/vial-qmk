// SPDX-License-Identifier: GPL-2.0-or-later
/* Versioned OLD-line macro overlay. No EEPROM writes or automatic migration.
 * With no committed overlay, read the unchanged legacy prefix + zero suffix.
 * Each accepted SET is durable, not the entire host's multi-packet import.
 * Only the inactive bank is erased. Header + payload are read back before the
 * all-zero commit page is programmed LAST. An interrupted write retains the
 * previous valid snapshot. A committed-but-corrupt bank is never a blank legacy
 * store. NOR bit errors can recover the preceding bank, not unbounded history.
 * Main-thread-only; the OLD build has no second XIP executor. */
#include "macro_storage.h"
#include <string.h>
static uint8_t page[256]; /* Sole COW buffer; never a whole-pool SRAM image. */
static bool last_ok = true;
static bool reading;
static const uint8_t *read_snapshot;
static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void store32(uint8_t *p, uint32_t x) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(8*i));
}
static uint32_t crc_byte(uint32_t crc, uint8_t x) {
    crc ^= x;
    for (unsigned b=0;b<8;b++) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    return crc;
}
static uint32_t crc(const uint8_t *p, unsigned n) {
    uint32_t c=~0u;
    for (unsigned i=0;i<n;i++) c=crc_byte(c,p[i]);
    return ~c;
}
static uint32_t header_crc(const uint8_t *p) {
    uint32_t c=~0u;
    for (unsigned i=0;i<256;i++) c=crc_byte(c,i>=20 && i<24 ? 0 : p[i]);
    return ~c;
}
static bool committed(const uint8_t *p) {
    for (unsigned i=0;i<256;i++) if (p[SM62_MACRO_COMMIT_OFFSET+i]) return false;
    return true;
}
static bool valid(const uint8_t *p) {
    if (!committed(p) || memcmp(p,"SMM1\1\0\0\0",8) || load32(p+12)!=SM62_MACRO_SIZE || load32(p+20)!=header_crc(p)) return false;
    if (load32(p+16)!=crc(p+SM62_MACRO_DATA_OFFSET,SM62_MACRO_SIZE)) return false;
    for (unsigned i=SM62_MACRO_DATA_OFFSET+SM62_MACRO_SIZE;i<SM62_MACRO_COMMIT_OFFSET;i++) if (p[i]!=255) return false;
    return true;
}
/* Revalidate on every public operation, INCLUDING identical SET. No stale RAM
 * index may certify damaged data. The synchronous playback lease scans once. */
static bool scan(const uint8_t **selected, unsigned *bank, uint32_t *gen) {
    const uint8_t *a=sm62_macro_flash_bank(0), *b=sm62_macro_flash_bank(1);
    if (!a || !b) return false;
    bool av=valid(a), bv=valid(b);
    *selected=0; *bank=1; *gen=0;
    if (!av && !bv) return !committed(a) && !committed(b);
    if (av && bv) {
        uint32_t d=load32(b+8)-load32(a+8);
        if (d!=1u && d!=UINT32_MAX) return false;
        if (d==1u) av=false;
    }
    *bank=av?0:1; *selected=av?a:b; *gen=load32(*selected+8);
    return true;
}
static void source_read(const uint8_t *snapshot, uint32_t offset, unsigned size, uint8_t *data) {
    if (snapshot) {memcpy(data,snapshot+SM62_MACRO_DATA_OFFSET+offset,size);return;}
    unsigned prefix=offset>=SM62_MACRO_LEGACY_SIZE ? 0 : SM62_MACRO_LEGACY_SIZE-offset;
    if (prefix>size) prefix=size;
    if (prefix) sm62_macro_legacy_read(offset,prefix,data);
    memset(data+prefix,0,size-prefix);
}
bool sm62_macro_ok(void) {return last_ok;}
bool sm62_macro_read_begin(void) {
    unsigned bank; uint32_t gen;
    if (reading) return last_ok=false;
    last_ok=scan(&read_snapshot,&bank,&gen);
    if (last_ok && !read_snapshot) {
        // Legacy imports used their own last byte as the unfinished-upload
        // marker. The extended pool's zero suffix must not unmute a torn
        // legacy upload. Keep raw GET unchanged and never migrate on playback.
        uint8_t marker;
        sm62_macro_legacy_read(SM62_MACRO_LEGACY_SIZE-1,1,&marker);
        last_ok=marker==0;
    }
    reading=last_ok;
    return last_ok;
}
void sm62_macro_read_end(void) {reading=false;read_snapshot=0;}
bool sm62_macro_read(uint32_t offset, uint32_t size, uint8_t *data) {
    const uint8_t *snapshot=read_snapshot;
    unsigned bank; uint32_t gen;
    if (offset>SM62_MACRO_SIZE || size>SM62_MACRO_SIZE-offset || (!reading && !scan(&snapshot,&bank,&gen))) {
        memset(data,0,size);return last_ok=false;
    }
    source_read(snapshot,offset,size,data);
    return last_ok=true;
}
static bool write_snapshot(uint32_t offset, uint32_t size, const uint8_t *data, bool reset) {
    const uint8_t *snapshot;
    unsigned active; uint32_t gen;
    if (reading || offset>SM62_MACRO_SIZE || size>SM62_MACRO_SIZE-offset || !scan(&snapshot,&active,&gen)) return false;
    bool changed=false;
    /* Preflight equality also verifies integrity; this does not certify a stale
     * cached EEPROM value after a failed transaction. */
    for (unsigned off=0;off<size;) {
        unsigned n=size-off>256?256:size-off;
        source_read(snapshot,offset+off,n,page);
        for (unsigned i=0;i<n;i++) if (page[i]!=(reset?0:data[off+i])) changed=true;
        off+=n;
    }
    if (!changed) return true;
    unsigned target=active^1u;
    if (!sm62_macro_flash_erase(target)) return false;
    uint32_t c=~0u;
    for (unsigned off=0;off<SM62_MACRO_SIZE;off+=256) {
        unsigned n=SM62_MACRO_SIZE-off>256?256:SM62_MACRO_SIZE-off;
        source_read(snapshot,off,n,page);
        memset(page+n,255,256-n);
        for (unsigned i=0;i<n;i++) {
            unsigned pos=off+i;
            if (pos>=offset && pos-offset<size) page[i]=reset?0:data[pos-offset];
            c=crc_byte(c,page[i]);
        }
        if (!sm62_macro_flash_program(target,SM62_MACRO_DATA_OFFSET+off,page)) return false;
        const uint8_t *dest=sm62_macro_flash_bank(target);
        if (!dest || memcmp(dest+SM62_MACRO_DATA_OFFSET+off,page,256)) return false;
    }
    memset(page,255,256);memcpy(page,"SMM1\1\0\0\0",8);
    store32(page+8,gen+1u);store32(page+12,SM62_MACRO_SIZE);store32(page+16,~c);store32(page+20,header_crc(page));
    if (!sm62_macro_flash_program(target,0,page)) return false;
    const uint8_t *dest=sm62_macro_flash_bank(target);
    if (!dest || memcmp(dest,page,256) || crc(dest+SM62_MACRO_DATA_OFFSET,SM62_MACRO_SIZE)!=~c) return false;
    memset(page,0,256);
    if (!sm62_macro_flash_program(target,SM62_MACRO_COMMIT_OFFSET,page)) return false;
    dest=sm62_macro_flash_bank(target);
    return dest && valid(dest) && load32(dest+8)==gen+1u;
}
bool sm62_macro_update(uint32_t offset, uint32_t size, const uint8_t *data) {
    return last_ok=write_snapshot(offset,size,data,false);
}
bool sm62_macro_reset(void) {
    return last_ok=write_snapshot(0,SM62_MACRO_SIZE,0,true);
}
