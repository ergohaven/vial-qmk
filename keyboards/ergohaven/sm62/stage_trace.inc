/* Local USB-master-only v0x16. Does not enter the split RPC callback.
 * RawHID and all producers are main-thread owners. No atomics, allocation,
 * logging, persistent writes, second reader, or changes to existing v3. */
#include "stage_trace_core.h"
#include "sm62_trace.h"
#include "ch.h"
#if CH_CFG_ST_FREQUENCY != 1000000U
#error stage trace requires verified 1MHz ChibiOS monotonic clock
#endif
static st_state stage_trace;
static uint32_t stage_panel_start[3];
static uint8_t stage_seq, stage_ready;
static uint32_t stage_now(void){return (uint32_t)chVTGetSystemTimeX();}
void sm62_trace_abort(void){if(stage_trace.live && stage_trace.started && !stage_trace.finished){stage_trace.w[6]|=8;stage_trace.finished=true;stage_trace.in_part=false;}}
static void stage_freeze(uint32_t now){stage_trace.w[2]=now;stage_trace.w[8]=sm62_diag_screen();stage_trace.live=false;stage_ready=1;}
void sm62_trace_loop(void){
 st_state*s=&stage_trace;if(!s->live)return;
 uint32_t now=stage_now();
 if(s->finished || (uint32_t)(now-s->epoch)>=ST_TIMEOUT){if((uint32_t)(now-s->epoch)>=ST_TIMEOUT)s->w[6]|=1;stage_freeze(now);return;}
 s->w[12]++;
}
void sm62_trace_edge(uint8_t id,bool begin){if(stage_trace.live && id<ST_STAGES)st_edge(&stage_trace,id,begin,stage_now());}
void sm62_trace_count(uint8_t id){if(stage_trace.live && id<24)stage_trace.w[id]++;}
void sm62_trace_panel(uint8_t id,bool begin){if(id>=3||!stage_trace.in_part)return;uint32_t t=stage_now();if(begin)stage_panel_start[id]=t;else st_panel(&stage_trace,id,t-stage_panel_start[id]);}
void sm62_trace_part(bool begin,bool first,bool last,int16_t x1,int16_t y1,int16_t x2,int16_t y2){
 st_state*s=&stage_trace;if(!s->live)return;uint32_t now=stage_now();
 if(begin){st_part_begin(s,now,first,sm62_diag_screen(),(uint16_t)x1|((uint32_t)(uint16_t)y1<<16));if(s->in_part && s->w[3]<ST_PARTS)s->w[ST_META+s->w[3]*8+7]=(uint16_t)x2|((uint32_t)(uint16_t)y2<<16);}
 else st_part_end(s,now,last);
}
static bool sm62_stage_raw(uint8_t*q,uint8_t n){
 if(n<3 || q[0]!=0xD7 || q[1]!=0x62 || q[2]!=0x16)return false;
 /* Recognized malformed packets are consumed, never settings commands. */
 if(n!=32 || !q[5] || q[4] || q[7] || (q[3]!=1 && q[3]!=2) || q[6]>=ST_PAGES || (q[3]==1 && q[6]))return true;
 for(unsigned i=8;i<32;i++)if(q[i])return true;
 uint8_t r[32]={0xD7,0x62,0x16,q[3],q[5],q[6],0,0};
 r[6]=0x10|sm62_side_id();
 if(!is_keyboard_master())r[7]=5;
 else if(q[3]==1){
  if(stage_trace.live)r[7]=6;
  else if(sm62_diag_screen()==2 || sm62_diag_screen()==4)r[7]=7;
  else {stage_seq=q[5];stage_ready=0;st_arm(&stage_trace,stage_now());stage_trace.w[7]=sm62_diag_screen();stage_trace.w[9]=sm62_local_display_settings()->mode;stage_trace.w[19]=stage_seq;r[7]=1;}
 }else if(stage_trace.live || !stage_ready){r[7]=3;if(stage_trace.live)stage_trace.w[6]|=2;}
 else if(stage_seq!=q[5])r[7]=4;
 else {r[7]=2;for(unsigned i=0;i<6;i++)fps_put32(r+8+i*4,stage_trace.w[q[6]*6+i]);}
 raw_hid_send(r,32);return true; /* VIA tail deliberately emits exact request echo. */
}
