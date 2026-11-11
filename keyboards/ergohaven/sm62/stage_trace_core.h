/* One local KEYMAP frame; unsigned microsecond subtraction is wrap safe.
 * All mutation is on the ordinary main thread, never the serial worker. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define ST_PARTS 96U
#define ST_META 24U
#define ST_STAGES 12U
#define ST_WORDS (ST_META+ST_PARTS*8U+ST_STAGES*3U)
#define ST_PAGES (ST_WORDS/6U)
#define ST_TIMEOUT 8000000U
/* w: 0 schema,1 epoch,2 frozen,3 parts,4 drops,5 complete,6 flags,
 * 7 screen start,8 screen end,9 mode,10 frame start,11 frame end,
 * 12 loops,13 user house,14 app RPCs,15 actual HID reports,
 * 16 QP-return gap sum,17 max,18 count,19 capture serial,
 * 20 timer Hz,21 words,22 pages,23 reserved.
 * part[8]: start,elapsed,viewport,pixdata,flush,outer gap,xy1,xy2.
 * stats[12][3]: count,sum,max; PRE,KEY,POST,HID,QP,DEFER,HOUSE,
 * USERHOUSE,SPLIT,SETTINGS,CLEANUP,reserved. Overlap is intentional. */
typedef struct {
 uint32_t w[ST_WORDS],epoch,start,stage[ST_STAGES],qp_return,gap,stage_valid;
 bool live,started,in_part,finished,qp_seen;
} st_state;
static inline void st_arm(st_state*s,uint32_t now){memset(s,0,sizeof(*s));s->live=true;s->epoch=now;s->w[0]=0x160001;s->w[1]=now;s->w[20]=1000000;s->w[21]=ST_WORDS;s->w[22]=ST_PAGES;}
static inline void st_stat(st_state*s,unsigned id,uint32_t d){if(id>=ST_STAGES)return;uint32_t*p=&s->w[ST_META+ST_PARTS*8+id*3];p[0]++;p[1]+=d;if(d>p[2])p[2]=d;}
static inline void st_edge(st_state*s,uint8_t id,bool begin,uint32_t now){
 if(!s->live)return;
 if(begin){s->stage_valid|=1U<<id;s->stage[id]=now;if(id==4 && s->qp_seen){s->gap=now-s->qp_return;s->w[16]+=s->gap;if(s->gap>s->w[17])s->w[17]=s->gap;s->w[18]++;}}
 else if(s->stage_valid&(1U<<id)){s->stage_valid&=~(1U<<id);st_stat(s,id,now-s->stage[id]);if(id==4){s->qp_return=now;s->qp_seen=true;}}
}
static inline void st_part_begin(st_state*s,uint32_t now,bool first,uint8_t screen,uint32_t rect){
 if(!s->live||s->finished)return;
 if(!s->started){if(!first||screen!=2)return;s->started=true;s->w[10]=now;}
 if(screen!=2)s->w[6]|=4;
 s->in_part=true;s->start=now;unsigned i=s->w[3];
 if(i<ST_PARTS){uint32_t*p=&s->w[ST_META+i*8];p[0]=now;p[5]=s->gap;p[6]=rect;}
 s->gap=0;
}
static inline void st_panel(st_state*s,uint8_t id,uint32_t d){if(s->in_part && s->w[3]<ST_PARTS && id<3)s->w[ST_META+s->w[3]*8+2+id]+=d;}
static inline void st_part_end(st_state*s,uint32_t now,bool last){
 if(!s->in_part)return;
 unsigned i=s->w[3]++;if(i<ST_PARTS)s->w[ST_META+i*8+1]=now-s->start;else s->w[4]++;
 s->in_part=false;if(last){s->w[11]=now;s->w[5]=1;s->finished=true;}
}
