// SPDX-License-Identifier: GPL-2.0-or-later
#include "dualcore_trace.h"
#include "render_owner.h"
#include "keyboard.h"
#include "raw_hid.h"
#include <string.h>
#if defined(SM62_RENDER_NATIVE_TEST)
extern uint32_t sm62_dc_test_us(void);
#define NOW() sm62_dc_test_us()
#else
#include "hardware/structs/timer.h"
#define NOW() (timer_hw->timerawl)
#endif
#define LOAD(p) __atomic_load_n((p),__ATOMIC_ACQUIRE)
#define STORE(p,v) __atomic_store_n((p),(v),__ATOMIC_RELEASE)
static uint32_t dc_request, dc_done; /* core0 request / core1 done */
static uint8_t dc_seq; /* core0; published by request release */
static uint32_t dc_loops, dc_keys, dc_packets; /* single core0 writers */
static uint32_t dc_words[SM62_DC_TRACE_WORDS]; /* core1 until done; then immutable */
static uint32_t dc_owner_token, dc_panel_start[3];
static bool dc_live, dc_started, dc_in_part;

void sm62_dc_trace_loop(void) { STORE(&dc_loops,LOAD(&dc_loops)+1U); }
void sm62_dc_trace_key(void) { STORE(&dc_keys,LOAD(&dc_keys)+1U); }
void sm62_dc_trace_packet(const uint8_t *d,uint8_t n) {
    if (n>=2 && d[0]==0xD7 && d[1]==0x62) return;
    STORE(&dc_packets,LOAD(&dc_packets)+1U);
}
static void dc_finish(uint32_t flags) {
    dc_words[7]|=flags;
    dc_words[6]=NOW(); dc_words[9]=sm62_render_screen();
    dc_words[12]=LOAD(&dc_loops); dc_words[14]=LOAD(&dc_keys); dc_words[16]=LOAD(&dc_packets);
    dc_words[18]=sm62_render_frames();
    if (dc_words[16]!=dc_words[15]) dc_words[7]|=2U;
    if (dc_started && dc_words[18]-dc_words[17]!=1U) dc_words[7]|=8U;
    dc_words[19]=NOW(); dc_live=false; dc_in_part=false;
    STORE(&dc_done,dc_owner_token);
}
void sm62_dc_trace_poll(void) {
    uint32_t request=LOAD(&dc_request);
    if (request && request!=dc_owner_token) {
        dc_owner_token=request; memset(dc_words,0,sizeof(dc_words));
        dc_words[0]=0x18323644U; dc_words[1]=SM62_DC_TRACE_VERSION; dc_words[2]=SM62_DC_TRACE_WORDS; dc_words[3]=dc_seq;
        dc_words[4]=NOW(); dc_words[11]=LOAD(&dc_loops); dc_words[13]=LOAD(&dc_keys); dc_words[15]=LOAD(&dc_packets);
        dc_live=true; dc_started=false; dc_in_part=false;
    }
    if (dc_live && (uint32_t)(NOW()-dc_words[4])>=5000000U) dc_finish(1U);
}
void sm62_dc_trace_abort(void) { if (dc_live) dc_finish(16U); }
void sm62_dc_trace_part(bool begin,bool first,bool last,int16_t x1,int16_t y1,int16_t x2,int16_t y2) {
    if (!dc_live) return;
    if (begin && !dc_started) {
        if (!first) return;
        dc_started=true; dc_words[5]=NOW(); dc_words[8]=sm62_render_screen(); dc_words[17]=sm62_render_frames();
    }
    if (!dc_started) return;
    uint32_t i=dc_words[10];
    if (i>=SM62_DC_TRACE_PARTS) { dc_words[7]|=4U; if (!begin && last) dc_finish(0); return; }
    uint32_t *p=dc_words+24U+i*8U;
    if (begin) {
        dc_in_part=true; p[0]=NOW(); p[2]=(uint16_t)x1|((uint32_t)(uint16_t)y1<<16); p[3]=(uint16_t)x2|((uint32_t)(uint16_t)y2<<16); p[7]=(first?1U:0U)|(last?2U:0U);
    } else if (dc_in_part) {
        p[1]=NOW(); dc_in_part=false; dc_words[10]=i+1U;
        if (last) dc_finish(0);
    }
}
void sm62_dc_trace_panel(uint8_t id,bool begin) {
    if (!dc_live || !dc_in_part || id>=3 || dc_words[10]>=SM62_DC_TRACE_PARTS) return;
    if (begin) dc_panel_start[id]=NOW();
    else dc_words[24U+dc_words[10]*8U+4U+id]+=NOW()-dc_panel_start[id];
}
bool sm62_dc_diag_raw(uint8_t *q,uint8_t n) {
    if (n<2 || q[0]!=0xD7 || q[1]!=0x62) return false;
    uint8_t r[32]={0xD7,0x62,SM62_DC_TRACE_VERSION,0,0,0,0,0};
    if (n!=32) return true;
    r[3]=q[3]; r[4]=q[5]; r[5]=q[6]; r[6]=(is_keyboard_master()?0x10:0x20)|(is_keyboard_left()?1:2);
    if (q[2]!=SM62_DC_TRACE_VERSION) r[7]=0xFE; /* old main-only protocols explicitly unsupported */
    else if (!is_keyboard_master()) r[7]=5;
    else if (!q[5] || q[4] || q[7] || q[6]>=SM62_DC_TRACE_PAGES || (q[3]!=1 && q[3]!=2) || (q[3]==1 && q[6])) r[7]=8;
    else {
        bool reserved=false; for (unsigned i=8;i<32;i++) reserved|=q[i]!=0;
        if (reserved) r[7]=8;
        else if (q[3]==1) {
            uint32_t request=LOAD(&dc_request);
            if (request!=LOAD(&dc_done)) r[7]=6;
            else if (request==UINT32_MAX) r[7]=8;
            else { dc_seq=q[5]; STORE(&dc_request,request+1U); r[7]=1; }
        } else if (q[5]!=dc_seq) r[7]=4;
        else if (!LOAD(&dc_request) || LOAD(&dc_done)!=LOAD(&dc_request)) r[7]=3;
        else {
            r[7]=2;
            for (unsigned i=0;i<6;i++) {
                uint32_t v=dc_words[q[6]*6U+i];
                for (unsigned b=0;b<4;b++) r[8+i*4+b]=(uint8_t)(v>>(b*8));
            }
        }
    }
    raw_hid_send(r,32);
    return true; /* existing VIA tail may still emit the exact request echo */
}
