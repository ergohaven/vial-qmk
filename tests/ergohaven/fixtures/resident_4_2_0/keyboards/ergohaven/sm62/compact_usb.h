/* SM62 USB compact FULL v1; only included by the production HID parser.
 * All lengths bounded; no heap, peer traffic, persistence, or live-image writes.
 * EB capability, EC begin, ED data, EE end (see compact_usb.md). */
#define APP_COMPACT_LIMIT 4560
static struct {
    bool valid;
    uint8_t codec, mode;
    uint16_t encoded, decoded, count, sequence, received;
    uint32_t transaction, decoded_crc, encoded_crc, started;
    uint8_t input[APP_COMPACT_LIMIT], output[APP_COMPACT_LIMIT];
} app_compact;
static bool app_compact_replaying;
bool hid_app_layout_process_packet(uint8_t *data, uint8_t length);
static uint16_t app_compact_u16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static bool app_compact_zero(const uint8_t *p, unsigned n) {
    while (n--) if (*p++) return false;
    return true;
}
static uint32_t app_compact_crc(const uint8_t *p, unsigned n) {
    uint32_t crc = UINT32_MAX;
    while (n--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
    }
    return ~crc;
}
#include "resident_cache.h"
static bool app_compact_last_valid;
static unsigned app_compact_last_size;
static void app_compact_reset(void) { app_compact.valid = false; app_compact_last_valid = false; }
static void app_compact_expire(void) {
    if ((app_compact.valid || app_compact_last_valid) && timer_elapsed32(app_compact.started) >= EH_APP_LAYOUT_TIMEOUT_MS) {
        app_compact_reset();
        app_layout_staging.valid = false;
    }
}
static bool app_compact_decode(void) {
    unsigned s = 0, d = 0;
    while (s < app_compact.encoded) {
        uint8_t tag = app_compact.input[s++];
        unsigned n = tag < 128 ? tag + 1 : (tag & 127) + 3;
        if (n > app_compact.decoded - d) return false;
        if (tag < 128) {
            if (n > app_compact.encoded - s) return false;
            memcpy(app_compact.output + d, app_compact.input + s, n); s += n; d += n;
        } else {
            if (app_compact.encoded - s < 2) return false;
            unsigned offset = app_compact_u16(app_compact.input + s); s += 2;
            if (!offset || offset > 4096 || offset > d) return false;
            while (n--) { app_compact.output[d] = app_compact.output[d - offset]; d++; }
        }
    }
    return d == app_compact.decoded;
}
/* Validate variable fields in a separate pass before even touching staging.
 * The original parser still validates completeness, revision and CRC16 and
 * is the sole publisher, including semantic identity and held-key releases. */
static bool app_compact_expand(uint8_t *b, unsigned size) {
    if (size < 64 + 1984 + 992 + 64 * 6 + 16 || b[0] != 0xE1 || b[1] != 8 ||
        b[2] > 1 || b[7] != 62 || b[8] != 16 || b[9] > 22 ||
        b[32] != 0xE3 || b[33] != 8 || b[34] != b[2] || memcmp(b + 3, b + 35, 4) ||
        !app_compact_zero(b+10+b[9],22-b[9]) || b[63]) return false;
    unsigned pos = 64 + 1984 + 992;
    for (unsigned layer = 0; layer < 16; layer++) {
        uint8_t count = 0;
        for (unsigned slot = 0; slot < 4; slot++) {
            if (size - pos < 6) return false;
            uint8_t c = b[pos], n = b[pos + 1];
            if ((c != 0 && (c < 2 || c > 4)) || n > 12 || (slot && c != count) || size - pos < 6U + n) return false;
            count = c; pos += 6 + n;
        }
    }
    for (unsigned layer = 0; layer < 16; layer++) {
        if (pos >= size || b[pos] > 22 || size - pos < 1U + b[pos]) return false;
        pos += 1 + b[pos];
    }
    if (pos != size) return false;
    app_compact_replaying = true;
    hid_app_layout_process_packet(b, 32);
    pos = 64;
    uint8_t p[32];
    for (unsigned l = 0; l < 16; l++) for (unsigned start = 0; start < 62; start += 13) {
        unsigned n = 62 - start; if (n > 13) n = 13;
        memset(p, 0, sizeof(p)); p[0] = 0xE2; p[1] = 8; p[2] = l; p[3] = start; p[4] = n;
        memcpy(p + 5, b + pos, n * 2); pos += n * 2; hid_app_layout_process_packet(p, 32);
    }
    for (unsigned l = 0; l < 16; l++) {
        memset(p, 0, sizeof(p));
        for (unsigned start = 0; start < 62; start += 27) {
            unsigned n = 62 - start; if (n > 27) n = 27;
            p[0] = 0xE7; p[1] = 8; p[2] = l; p[3] = start; p[4] = n;
            memcpy(p + 5, b + pos, n); pos += n; hid_app_layout_process_packet(p, 32);
        }
    }
    for (unsigned l = 0; l < 16; l++) for (unsigned slot = 0; slot < 4; slot++) {
        unsigned n = b[pos + 1]; memset(p, 0, sizeof(p)); p[0] = 0xE9; p[1] = 8; p[2] = l; p[3] = slot;
        memcpy(p + 4, b + pos, 6 + n); pos += 6 + n; hid_app_layout_process_packet(p, 32);
    }
    for (unsigned l = 0; l < 16; l++) {
        unsigned n = b[pos]; memset(p, 0, sizeof(p)); p[0] = 0xE4; p[1] = 8; p[2] = l;
        memcpy(p + 3, b + pos, 1 + n); pos += 1 + n; hid_app_layout_process_packet(p, 32);
    }
    uint32_t generation = app_layout_visual_generation;
    app_compact_validated = false;
    hid_app_layout_process_packet(b + 32, 32);
    app_compact_replaying = false;
    return app_compact_validate_only ? app_compact_validated : app_layout_visual_generation != generation;
}
static bool app_compact_ready(const uint8_t *p) {
    return app_compact_last_valid && app_compact.mode==2 && !app_compact.valid &&
        timer_elapsed32(app_compact.started)<EH_APP_LAYOUT_TIMEOUT_MS &&
        app_layout_read_u32(p+4)==app_compact.transaction && app_compact.output[2]==1 &&
        app_compact.output[41]==0x57 && app_compact.output[42]==2 &&
        !memcmp(p+8,app_compact.output+43,20) && app_layout_read_u32(p+28)==app_compact.decoded_crc;
}
static void app_resident_packet(uint8_t *p) {
    unsigned op=p[2],slot=p[3],status=2;
    if(op==6) {
        unsigned off=app_compact_u16(p+8),n=p[10],actual=0;
        bool valid=slot<100 && n>=1 && n<=20 && !p[11] && app_compact_zero(p+12,20);
        const uint8_t *record=0;
        if(valid) {
            int r=cache_current(slot);record=r>=0?sm62_cache_flash_record(r):0;
            status=record && record[6]==1 ? 0 : 1;
            if(record && !cache_record_integrity(r)) status=7;
            if(!cache_scanned) status=3;
            if(!status) {
                unsigned length=app_compact_u16(record+12);
                if(off>length) status=2;
                else {actual=length-off;if(actual>n) actual=n;}
            }
        }
        p[2]=0x86;p[10]=actual;p[11]=status;memset(p+12,0,20);
        if(!status && actual) memcpy(p+12,record+256+off,actual);
        return;
    }
    if(op==2) app_compact_reset();
    if(slot<100 && op<=5) {
        int r=cache_current(slot);
        const uint8_t *record=r>=0?sm62_cache_flash_record(r):0;
        bool present=record && record[6]==1;
        bool match=present && !memcmp(record+20,p+8,20) && app_layout_read_u32(record+16)==app_layout_read_u32(p+28);
        /* Confirm the current indexed payload, not only the CRC stored in its
         * header. This is a bounded single-record command path, never a key path. */
        if(op!=5 && !cache_scanned) status=3;
        else if(op!=5 && record && !cache_record_integrity(r)) status=7;
        else if(op==0) status=match?0:1;
        else if(op==3) {
            if(!app_compact_zero(p+8,24)) status=2;
            else {
                status=present?0:1;
                if(present) {memcpy(p+8,record+20,20);memcpy(p+28,record+16,4);}
            }
        } else if(op==5) status=app_compact_ready(p)?0:3;
        else if(op==1) {
            if(!app_compact_ready(p)) status=3;
            else if(present && memcmp(record+20,p+8,20)) status=6;
            else if(cache_generation==UINT32_MAX && !match) status=8;
            else status=cache_store(slot,p+8,app_layout_read_u32(p+28),app_compact.output,app_compact_last_size)?0:5;
        } else if(op==4) {
            if(!match) status=1;
            else if(cache_generation==UINT32_MAX) status=8;
            else status=cache_append(slot,p+8,app_layout_read_u32(p+28),0,0,true)?0:5;
        } else if(op==2) {
            if(!match) status=1;
            else {
                unsigned n=app_compact_u16(record+12);
                memcpy(app_compact.output,record+256,n);
                status=app_compact_expand(app_compact.output,n)?0:7;
            }
        }
    }
    p[2]=op|0x80;p[3]=status;
}
static bool app_compact_packet(uint8_t *p, uint8_t length) {
    if (p[0] < 0xEB || p[0] > 0xEF) return false;
    app_compact_expire();
    if (p[0] == 0xEF) {
        if(length==32 && p[1]==9) {app_resident_packet(p);return true;}
        if (length != 32 || p[1] != 8 || p[2] > 2) return true;
        /* EF8 is read-only compatibility: never activate native Default or
         * modify either the legacy partition or the schema2 journal. */
        unsigned command=p[2];
        bool accepted=command==0 && cache_match(p[3],p+8,app_layout_read_u32(p+28))>=0;
        p[2]=command|0x80; p[3]=accepted ? 1 : 0;
        return true;
    }
    if (p[0] == 0xEB) {
        if (length == 32 && p[1] == 8 && p[2] == 0xA5 && (p[3] == 1 || p[3] == 2) && app_compact_zero(p + 8, 24)) {
            p[2] = 0x5A; p[8] = p[3]==2 ? 2 : 3; p[9] = p[3]==2 ? 100 : 0;
            p[10] = APP_COMPACT_LIMIT & 255; p[11] = APP_COMPACT_LIMIT >> 8;
            if(p[3]==2) {
                p[12]=16;p[13]=62;p[14]=22;cache_put32(p+16,4194304);cache_put32(p+20,15);p[24]=2;
            }
        }
        return true;
    }
    if (length != 32 || p[1] != 8) goto invalid;
    if (p[0] == 0xEC) {
        app_compact_reset(); app_layout_staging.valid = false;
        unsigned encoded = app_compact_u16(p + 8), decoded = app_compact_u16(p + 10), count = app_compact_u16(p + 12);
        if ((p[2] != 1 && p[2] != 2) || p[3] > 1 || !encoded || encoded > APP_COMPACT_LIMIT || decoded < 3440 || decoded > APP_COMPACT_LIMIT ||
            count != (encoded + 23) / 24 || !app_compact_zero(p + 14, 2) || !app_compact_zero(p + 24, 8) ||
            (!p[3] && encoded != decoded) || (p[3] && encoded >= decoded)) goto invalid;
        app_compact.mode = p[2]; app_compact.codec = p[3]; app_compact.transaction = app_layout_read_u32(p + 4);
        app_compact.encoded = encoded; app_compact.decoded = decoded; app_compact.count = count;
        app_compact.decoded_crc = app_layout_read_u32(p + 16); app_compact.encoded_crc = app_layout_read_u32(p + 20);
        app_compact.sequence = app_compact.received = 0; app_compact.started = timer_read32(); app_compact.valid = true;
        return true;
    }
    if (!app_compact.valid || app_layout_read_u32(p + 2) != app_compact.transaction) goto invalid;
    if (p[0] == 0xED) {
        /* The existing host retries a transient output write once. A write
         * error can follow delivery: accept ONLY an exact immediate previous
         * DATA retry. It neither appends bytes nor renews the transaction TTL. */
        uint16_t sequence = app_compact_u16(p + 6);
        if (app_compact.sequence && sequence == app_compact.sequence - 1U) {
            unsigned offset = sequence * 24U;
            unsigned n = app_compact.encoded - offset; if (n > 24) n = 24;
            if (memcmp(app_compact.input + offset, p + 8, n) || !app_compact_zero(p + 8 + n, 24 - n)) goto invalid;
            return true;
        }
        if (app_compact_u16(p + 6) != app_compact.sequence || app_compact.sequence >= app_compact.count) goto invalid;
        unsigned n = app_compact.encoded - app_compact.received; if (n > 24) n = 24;
        if (!n || !app_compact_zero(p + 8 + n, 24 - n)) goto invalid;
        memcpy(app_compact.input + app_compact.received, p + 8, n); app_compact.received += n; app_compact.sequence++;
        return true;
    }
    if (app_compact_u16(p + 6) != app_compact.count || app_compact.sequence != app_compact.count ||
        app_compact.received != app_compact.encoded || app_layout_read_u32(p + 8) != app_compact.encoded_crc ||
        !app_compact_zero(p + 12, 20) || app_compact_crc(app_compact.input, app_compact.encoded) != app_compact.encoded_crc) goto invalid;
    if (app_compact.codec) { if (!app_compact_decode()) goto invalid; }
    else memcpy(app_compact.output, app_compact.input, app_compact.decoded);
    if (app_compact_crc(app_compact.output, app_compact.decoded) != app_compact.decoded_crc) goto invalid;
    if (app_compact.mode==2 && (app_compact.output[2]!=1 || app_compact.output[41]!=0x57 || app_compact.output[42]!=2)) goto invalid;
    app_compact.valid = false;
    bool accepted = app_compact.mode==2 ? cache_payload_valid(app_compact.output,app_compact.decoded) : app_compact_expand(app_compact.output,app_compact.decoded);
    if (!accepted) goto invalid;
    app_compact_last_valid = true; app_compact_last_size = app_compact.decoded;
    app_compact.started = timer_read32();
    return true;
invalid:
    app_compact_reset(); app_layout_staging.valid = false;
    return true;
}
