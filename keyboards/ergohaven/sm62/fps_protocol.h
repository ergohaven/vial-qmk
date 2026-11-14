/* Diagnostic wire v3. 32-byte raw HID, 8-byte internal RPC request.
 * q: D7 62 03 command target sequence page 00; remaining bytes zero.
 * r: D7 62 03 command sequence page (role<<4|side) status + 6 LE u32.
 * command 1=RAM ARM, 2=read frozen snapshot. target 0=USB,1=peer.
 * status 1=ARM staged, 2=frozen snapshot, 3=not ready,4=sequence mismatch.
 */
#pragma once
#include "fps_measure.h"
#define FPS_VID 0xE126
#define FPS_PID 0x00B2
#define FPS_USAGE_PAGE 0xFF60
#define FPS_USAGE 0x61
static inline bool fps_request(const uint8_t *q,unsigned n) {
    if(!q || (n!=8 && n!=32) || q[0]!=0xD7 || q[1]!=0x62 || q[2]!=3 || q[4]>1 || !q[5] || q[7]) return false;
    if((q[3]!=1 && q[3]!=2) || (q[3]==1 && q[6]) || q[6]>=FPS_PAGES) return false;
    for(unsigned i=8;i<n;i++) if(q[i]) return false;
    return true;
}
static inline uint32_t fps_get32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static inline void fps_put32(uint8_t *p,uint32_t v) { for(uint8_t i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static inline void fps_encode(uint8_t *r,const uint8_t *q,const fps_state *s,uint8_t side,uint8_t role,uint8_t status) {
    memset(r,0,32); r[0]=0xD7;r[1]=0x62;r[2]=3;r[3]=q[3];r[4]=q[5];r[5]=q[6];r[6]=(role<<4)|side;r[7]=status;
    if(status==2 && q[6]<FPS_PAGES) for(uint8_t i=0;i<6;i++) fps_put32(r+8+4*i,s->words[q[6]*6+i]);
}
static inline bool fps_response(const uint8_t *r,unsigned n,const uint8_t *q,uint8_t role,uint8_t status) {
    return n==32 && r && fps_request(q,32) && r[0]==0xD7 && r[1]==0x62 && r[2]==3 && r[3]==q[3] && r[4]==q[5] && r[5]==q[6] && (r[6]>>4)==role && (r[6]&15)>=1 && (r[6]&15)<=2 && r[7]==status;
}
