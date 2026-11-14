#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
static unsigned parts, finals, start_parts, mutations, pending_mutations, spawned_calls;
static lv_color_t panel[240*280];
static bool expired(void){return parts-start_parts >= 4;}
static void mutate(lv_timer_t *t){
 mutations++;if(lv_disp_refr_pending(NULL))pending_mutations++;
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0xff0000),0);
 lv_timer_del(t);
}
static void spawned(lv_timer_t*t){spawned_calls++;lv_timer_del(t);}
static void start_and_delete(lv_timer_t*t){
 lv_timer_ready(lv_timer_create(spawned,1,NULL));
 lv_refr_now(NULL);if(t->repeat_count != 0)lv_timer_del(t);
}
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){
 unsigned w=lv_area_get_width(a),h=lv_area_get_height(a);
 for(unsigned y=0;y<h;y++)memcpy(panel+(a->y1+y)*240+a->x1,p+y*w,w*sizeof(*p));
 parts++;if(lv_disp_flush_is_last(d))finals++;lv_disp_flush_ready(d);
}
int main(int argc,char**argv){
 assert(argc==2);unsigned mode=atoi(argv[1]);lv_init();lv_timer_create(mutate,1,NULL);
 static lv_color_t pixels[960];static lv_disp_draw_buf_t b;lv_disp_draw_buf_init(&b,pixels,NULL,960);
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.draw_buf=&b;drv.flush_cb=flush;drv.refr_slice_cb=expired;
 lv_disp_t*d=lv_disp_drv_register(&drv);lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x0000ff),0);
 if(mode){lv_timer_pause(d->refr_timer);lv_timer_t*t=lv_timer_create(start_and_delete,1,NULL);if(mode==2)lv_timer_set_repeat_count(t,1);}
 lv_tick_inc(40);lv_timer_handler();
 printf("mode=%u first: parts=%u pending=%d mutations=%u pending_mutations=%u spawned=%u\n",mode,parts,lv_disp_refr_pending(d),mutations,pending_mutations,spawned_calls);
 assert(parts==4&&lv_disp_refr_pending(d)&&mutations==0&&spawned_calls==0);
 for(unsigned i=0;i<10;i++){lv_timer_handler();assert(mutations==0&&spawned_calls==0);}
 while(lv_disp_refr_pending(d)){start_parts=parts;lv_refr_now(d);assert(mutations==0&&spawned_calls==0);}
 unsigned blue=0;for(unsigned i=0;i<240*280;i++)blue+=panel[i].full==lv_color_hex(0x0000ff).full;
 assert(blue==67200&&parts==70&&finals==1);
 lv_timer_pause(d->refr_timer);lv_timer_handler();assert(mutations==1&&pending_mutations==0&&spawned_calls==(mode!=0));
 lv_timer_handler();assert(mutations==1);printf("PASS uniform_blue=%u final=%u deferred_mutation=%u pending_mutations=%u spawned=%u; epilogue released\n",blue,finals,mutations,pending_mutations,spawned_calls);
}
