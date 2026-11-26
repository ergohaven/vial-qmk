/* Schema2 COW journal. 100 compact record indices, one rotating spare.
 * No live layout is published during recovery or durable writes. */
#pragma once
#include "resident_cache_flash.h"
#if SM62_CACHE_SLOTS == 0 || SM62_CACHE_SLOTS > 128
#error "Resident inventory requires 1..128 slots"
#endif
static bool app_compact_expand(uint8_t *b, unsigned size);
static bool app_compact_validate_only, app_compact_validated;
/* Process-local immutable-record semantic check; never an ingress grant. */
static bool app_compact_record_integrity;
static uint8_t cache_latest[SM62_CACHE_SLOTS]; /* record + 1; zero means absent */
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
    if(!p || app_layout_read_u32(p)!=0x32524345 || p[4]!=2 || p[5]>=SM62_CACHE_SLOTS ||
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
    cache_init();return cache_scanned && slot<SM62_CACHE_SLOTS && cache_latest[slot] ? cache_latest[slot]-1 : -1;
}
/* EF09 INVENTORY payload: generation, capacity, bitmap length, occupied bits.
 * Reuse the sole recovered index; tombstones must pass integrity too. Never
 * expose a partial bitmap or replay semantics through an owned staging image. */
static unsigned cache_inventory(uint8_t *payload) {
    memset(payload,0,24);
    cache_init();
    if(!cache_scanned) return 3;
    for(unsigned slot=0;slot<SM62_CACHE_SLOTS;slot++) if(cache_latest[slot]) {
        unsigned r=cache_latest[slot]-1;
        if(!cache_record_integrity(r)) {memset(payload,0,24);return 7;}
        if(sm62_cache_flash_record(r)[6]==1) payload[6+slot/8]|=1U<<(slot%8);
    }
    cache_put32(payload,cache_generation);
    payload[4]=SM62_CACHE_SLOTS;payload[5]=(SM62_CACHE_SLOTS+7)/8;
    return 0;
}
static int cache_match(unsigned slot,const uint8_t *id,uint32_t crc) {
    int r=cache_current(slot);if(r<0) return -1;
    const uint8_t *p=sm62_cache_flash_record(r);
    return p && p[6]==1 && app_layout_read_u32(p+16)==crc && !memcmp(p+20,id,20) && cache_record_integrity(r) ? r : -1;
}
