/* Serial callback only stages ARM / reads immutable completed RAM.
 * Main loop owns all measurement mutation; ready uses release/acquire. */
#include "fps_protocol.h"
static fps_state fps_data;
static uint8_t fps_pending, fps_applied, fps_arm_seq, fps_ready, fps_active;
/* One query writer per half: raw/main on USB; serial worker on peer. */
static uint32_t fps_active_epoch, fps_diag_calls, fps_diag_base;
extern uint8_t sm62_diag_screen(void);
static void fps_note_query(void) {
    if(__atomic_load_n(&fps_active,__ATOMIC_ACQUIRE) &&
       (uint32_t)(timer_read32()-__atomic_load_n(&fps_active_epoch,__ATOMIC_RELAXED))<FPS_WINDOW)
        __atomic_store_n(&fps_diag_calls,__atomic_load_n(&fps_diag_calls,__ATOMIC_RELAXED)+1,__ATOMIC_RELAXED);
}
static void fps_local_request(const uint8_t *q,uint8_t *r) {
    if(q[3]==1) {
        __atomic_store_n(&fps_ready,0,__ATOMIC_RELEASE);
        __atomic_store_n(&fps_arm_seq,q[5],__ATOMIC_RELAXED);
        __atomic_store_n(&fps_pending,(uint8_t)(__atomic_load_n(&fps_pending,__ATOMIC_RELAXED)+1),__ATOMIC_RELEASE);
        fps_encode(r,q,&fps_data,sm62_side_id(),is_keyboard_master()?1:2,1);
    } else if(!__atomic_load_n(&fps_ready,__ATOMIC_ACQUIRE) || __atomic_load_n(&fps_pending,__ATOMIC_ACQUIRE)!=__atomic_load_n(&fps_applied,__ATOMIC_ACQUIRE)) {
        fps_encode(r,q,&fps_data,sm62_side_id(),is_keyboard_master()?1:2,3);
    } else {
        fps_encode(r,q,&fps_data,sm62_side_id(),is_keyboard_master()?1:2,fps_data.seq==q[5]?2:4);
    }
}
static bool sm62_fps_rpc(uint8_t in_len,const void *in,uint8_t out_len,void *out) {
    if(in_len!=8 || !fps_request(in,in_len) || out_len!=32 || !out || is_keyboard_master()) return false;
    fps_note_query(); fps_local_request(in,out); return true;
}
static bool sm62_fps_raw(uint8_t *q,uint8_t length) {
    if(length<3 || q[0]!=0xD7 || q[1]!=0x62 || q[2]!=3) return false;
    /* Consume malformed v3 diagnostics, never fall through into settings. */
    if(length!=32 || !fps_request(q,length)) return true;
    uint8_t r[32]={0}; fps_note_query();
    if(q[4]) {
        uint32_t start=timer_read32();
        bool ok=is_keyboard_master() && transaction_rpc_exec(RPC_SM62_DIAG,8,q,32,r);
        fps_duration(&fps_data,start,timer_read32(),40);
        if(!ok) fps_encode(r,q,&fps_data,sm62_side_id(),1,5);
    } else fps_local_request(q,r);
    raw_hid_send(r,32); return true;
}
static void sm62_fps_tick(void) {
    uint32_t now=timer_read32();
    uint8_t pending=__atomic_load_n(&fps_pending,__ATOMIC_ACQUIRE);
    if(pending!=__atomic_load_n(&fps_applied,__ATOMIC_RELAXED)) {
        __atomic_store_n(&fps_active,0,__ATOMIC_RELEASE);
        fps_arm(&fps_data,now,__atomic_load_n(&fps_arm_seq,__ATOMIC_RELAXED),sm62_diag_screen(),sm62_local_display_settings()->mode);
        fps_diag_base=__atomic_load_n(&fps_diag_calls,__ATOMIC_RELAXED);
        __atomic_store_n(&fps_active_epoch,now,__ATOMIC_RELAXED);
        __atomic_store_n(&fps_active,1,__ATOMIC_RELEASE);
        __atomic_store_n(&fps_applied,pending,__ATOMIC_RELEASE);
    }
    bool was_live=fps_data.live;
    fps_tick(&fps_data,now,sm62_diag_screen(),sm62_local_display_settings()->mode);
    if(was_live && fps_data.done) {
        __atomic_store_n(&fps_active,0,__ATOMIC_RELEASE);
        fps_data.words[33]=__atomic_load_n(&fps_diag_calls,__ATOMIC_RELAXED)-fps_diag_base;
        __atomic_store_n(&fps_ready,1,__ATOMIC_RELEASE);
    }
}
void sm62_fps_frame(uint8_t screen) { fps_frame(&fps_data,timer_read32(),screen); }
void sm62_fps_capture(uint32_t start) { fps_duration(&fps_data,start,timer_read32(),27); }
