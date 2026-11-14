/* Schema2 COW journal. 100 compact record indices, one rotating spare.
 * No live layout is published during recovery or durable writes. */
#pragma once
#include "resident_cache_flash.h"
static bool app_compact_expand(uint8_t *b, unsigned size);
static bool app_compact_validate_only, app_compact_validated;
/* Process-local immutable-record semantic check; never an ingress grant. */
static bool app_compact_record_integrity;
static uint8_t cache_latest[100]; /* record + 1; zero means absent */
static bool cache_scanned;
static uint32_t cache_generation;
static void cache_put32(uint8_t *p, uint32_t x) { for (unsigned i=0;i<4;i++) p[i]=x>>(8*i); }
static bool cache_payload_valid(const uint8_t *raw,unsigned n) {
    bool saved=app_compact_validate_only;
    app_compact_validate_only=true;
    bool ok=app_compact_expand((uint8_t *)raw,n);
    app_compact_validate_only=saved;
    return ok;
}
static bool cache_header_valid(const uint8_t *p) {
    if(!p || app_layout_read_u32(p)!=0x32524345 || p[4]!=2 || p[5]>=100 ||
       (p[6]!=1 && p[6]!=2) || p[7] || !app_layout_read_u32(p+8) || p[14] || p[15] ||
       app_layout_read_u32(p+40)!=app_compact_crc(p,40) ||
       app_layout_read_u32(p+7936)!=0x324d4345 ||
       app_layout_read_u32(p+7940)!=app_layout_read_u32(p+8) ||
       app_layout_read_u32(p+7944)!=app_layout_read_u32(p+40) ||
       app_layout_read_u32(p+7948)!=app_compact_crc(p+7936,12)) return false;
    unsigned n=app_compact_u16(p+12);
    return p[6]==2 ? n==0 : n>=3440 && n<=APP_COMPACT_LIMIT;
}
/* For an already semantically validated index, recheck current durable bytes
 * without replaying E1/E3 through the shared ordinary staging image. */
static bool cache_record_integrity(unsigned r) {
    const uint8_t *p=sm62_cache_flash_record(r);
    if(!cache_header_valid(p)) return false;
    if(p[6]==2) return true;
    unsigned n=app_compact_u16(p+12);
    return app_compact_crc(p+256,n)==app_layout_read_u32(p+16) &&
       p[256+2]==1 && p[256+41]==0x57 && p[256+42]==2 && !memcmp(p+20,p+256+43,20);
}
static bool cache_record_valid(unsigned r) {
    if(!cache_record_integrity(r)) return false;
    const uint8_t *p=sm62_cache_flash_record(r);
    if(p[6]==2) return true;
    /* Scope only the synchronous semantic check of committed durable bytes.
     * STORE and mode-2 import keep cache_payload_valid's lock authorization. */
    bool saved=app_compact_record_integrity;
    app_compact_record_integrity=true;
    bool ok=cache_payload_valid(p+256,app_compact_u16(p+12));
    app_compact_record_integrity=saved;
    return ok;
}
static void cache_init(void) {
    /* A cold scan requires the sole semantic parser's staging image. Defer
     * rather than clobber an ordinary transfer or allocate another decoded map. */
    if(cache_scanned || app_layout_staging.valid) return;
    memset(cache_latest,0,sizeof(cache_latest)); cache_generation=0;
    for(unsigned r=0;r<SM62_CACHE_RECORDS;r++) if(cache_record_valid(r)) {
        const uint8_t *p=sm62_cache_flash_record(r);unsigned s=p[5];uint32_t g=app_layout_read_u32(p+8);
        if(g>cache_generation) cache_generation=g;
        if(!cache_latest[s] || g>app_layout_read_u32(sm62_cache_flash_record(cache_latest[s]-1)+8)) cache_latest[s]=r+1;
    }
    cache_scanned=true;
}
static int cache_current(unsigned slot) {
    cache_init();return cache_scanned && slot<100 && cache_latest[slot] ? cache_latest[slot]-1 : -1;
}
static int cache_match(unsigned slot,const uint8_t *id,uint32_t crc) {
    int r=cache_current(slot);if(r<0) return -1;
    const uint8_t *p=sm62_cache_flash_record(r);
    return p && p[6]==1 && app_layout_read_u32(p+16)==crc && !memcmp(p+20,id,20) && cache_record_integrity(r) ? r : -1;
}
static bool cache_append(unsigned slot,const uint8_t *id,uint32_t crc,const uint8_t *raw,unsigned n,bool deleted) {
    if(slot>=100) return false;
    if(!deleted && (n<3440 || n>APP_COMPACT_LIMIT || raw[2]!=1 || raw[41]!=0x57 || raw[42]!=2 ||
       memcmp(raw+43,id,20) || app_compact_crc(raw,n)!=crc || !cache_payload_valid(raw,n))) return false;
    /* An indexed but damaged record is not a no-wear hit. Fail closed without
     * rescanning away its identity, reclaiming it, or silently repairing it. */
    int current=cache_current(slot);
    if(!cache_scanned || (current>=0 && !cache_record_integrity(current))) return false;
    if(!deleted && cache_match(slot,id,crc)>=0) return true;
    cache_init();if(cache_generation==UINT32_MAX) return false;
    unsigned r;
    for(r=0;r<SM62_CACHE_RECORDS;r++) {
        unsigned s;for(s=0;s<100;s++) if(cache_latest[s]==r+1) break;
        if(s==100) break;
    }
    if(r==SM62_CACHE_RECORDS) return false;
    /* A failed IO may have reached flash. Reconcile on the next read rather
     * than retaining an optimistic or stale index after an uncertain commit. */
    cache_scanned=false;
    if(!sm62_cache_flash_erase(r)) return false;
    static uint8_t page[256]; memset(page,255,256);
    cache_put32(page,0x32524345);page[4]=2;page[5]=slot;page[6]=deleted?2:1;page[7]=0;
    cache_put32(page+8,cache_generation+1);page[12]=n;page[13]=n>>8;page[14]=page[15]=0;
    cache_put32(page+16,crc);memcpy(page+20,id,20);cache_put32(page+40,app_compact_crc(page,40));
    uint32_t header_crc=app_layout_read_u32(page+40);
    if(!sm62_cache_flash_program(r,0,page)) return false;
    for(unsigned off=0;off<n;off+=256) {
        unsigned count=n-off;if(count>256) count=256;
        memset(page,255,256);memcpy(page,raw+off,count);
        if(!sm62_cache_flash_program(r,256+off,page)) return false;
    }
    const uint8_t *p=sm62_cache_flash_record(r);
    if(app_layout_read_u32(p+40)!=header_crc || app_compact_crc(p,40)!=header_crc || (n && memcmp(p+256,raw,n))) return false;
    memset(page,255,256);cache_put32(page,0x324d4345);cache_put32(page+4,cache_generation+1);
    cache_put32(page+8,header_crc);cache_put32(page+12,app_compact_crc(page,12));
    if(!sm62_cache_flash_program(r,7936,page) || !cache_record_valid(r)) return false;
    cache_generation++;cache_latest[slot]=r+1;cache_scanned=true;return true;
}
static bool cache_store(unsigned slot,const uint8_t *id,uint32_t crc,const uint8_t *raw,unsigned n) {
    return cache_append(slot,id,crc,raw,n,false);
}
