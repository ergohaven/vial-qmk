/* RAM-only diagnostic helpers; also compiled unchanged by native tests. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define FPS_WORDS 48
#define FPS_WINDOW 10000U
#define FPS_PAGES (FPS_WORDS/6)
typedef struct { uint32_t words[FPS_WORDS],last_frame,last_loop; uint8_t seq,last_screen; bool live,done,frame_seen,loop_seen; } fps_state;
static inline void fps_arm(fps_state *s,uint32_t now,uint8_t seq,uint8_t screen,uint8_t mode) {
    memset(s,0,sizeof(*s)); s->live=true; s->seq=seq;
    s->words[0]=now; s->words[1]=now+FPS_WINDOW; s->words[2]=FPS_WINDOW;
    s->words[3]=s->words[5]=screen; s->words[4]=s->words[6]=mode;
    s->last_screen=screen;
}
static inline bool fps_inside(const fps_state *s,uint32_t now) { return s->live && (uint32_t)(now-s->words[0])<FPS_WINDOW; }
static inline void fps_max(uint32_t *p,uint32_t v) { if(v>*p)*p=v; }
static inline void fps_duration(fps_state *s,uint32_t start,uint32_t end,uint8_t index) {
    if(!fps_inside(s,start)) return;
    if(!fps_inside(s,end)) { s->words[35]++; return; }
    if(index!=24 && index!=27 && index!=40) return;
    uint32_t d=end-start; s->words[index]++; s->words[index+1]+=d; fps_max(&s->words[index+2],d);
}
static inline void fps_frame(fps_state *s,uint32_t now,uint8_t screen) {
    if(!fps_inside(s,now) || screen>4) return;
    s->words[8+screen]++; s->words[7]|=1U<<screen;
    if(screen==3) {
        if(!s->words[11] || s->words[11]==1) s->words[38]=now;
        s->words[39]=now;
        if(s->frame_seen && s->last_screen==3) {
            uint32_t d=now-s->last_frame;
            static const uint16_t edges[7]={16,33,50,66,100,150,250};
            uint8_t b=0; while(b<7 && d>edges[b]) b++;
            s->words[13]++; s->words[14]+=d; fps_max(&s->words[15],d); s->words[16+b]++;
        }
    }
    s->frame_seen=true; s->last_frame=now; s->last_screen=screen;
}
static inline void fps_tick(fps_state *s,uint32_t now,uint8_t screen,uint8_t mode) {
    if(!s->live) return;
    if(!fps_inside(s,now)) { s->words[37]=now; s->live=false; s->done=true; return; }
    if(screen>4) screen=4;
    if(s->words[5]!=screen) { s->words[34]++; s->frame_seen=false; }
    s->words[5]=screen; s->words[6]=mode; s->words[7]|=1U<<screen;
    if(s->loop_seen) { uint32_t d=now-s->last_loop; s->words[30]++; s->words[31]+=d; fps_max(&s->words[32],d); }
    s->last_loop=now; s->loop_seen=true;
}
