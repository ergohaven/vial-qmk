#!/usr/bin/env python3
"""SM62 production mode/painter regression with pinned real LVGL.

Run with a scratch output directory and --lvgl pointing to pinned lib/lvgl.
--source can replay identical assertions against a frozen baseline. Hardware,
clocks, storage and non-keymap view callbacks are injected; mode selection,
settings, keymap rendering and the painter flush are production code. Never
call display_frame_completed from the fixture. Default mode is acceptance;
feedback is an intentionally failing diagnostic, not a master-flicker test.
"""
from pathlib import Path
from native_vial_firewall import firewall_source
import argparse, re, subprocess, sys, hashlib, json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--lvgl',type=Path,required=True);p.add_argument('--case',choices=['mode','feedback','typing'],default='mode');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);root=a.source.resolve()
sys.path.insert(0,str(root/'tests/ergohaven'))
from macropad_native_support import validate_lvgl,write_stubs
validate_lvgl(a.lvgl, profile='cooperative');write_stubs(out/'stubs')
def read(p):return (root/p).read_text()
def stripped(p):return re.sub(r'^#include[^\n]*','',read(p),flags=re.M)
def function(src,name):
 m=re.search(r'^(?:__attribute__\(\(weak\)\) )?(?:static )?[^\n;]+\b'+name+r'\([^;\n]*\)\s*\{',src,re.M);assert m,name
 e=src.index('{',m.start())+1;d=1
 while d:d+=(src[e]=='{')-(src[e]=='}');e+=1
 return src[m.start():e]+'\n'
h=read('keyboards/ergohaven/hid.c');h=h[h.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):h.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "quantum/keycodes.h"
#include "quantum/quantum_keycodes.h"
#include "lvgl.h"
#include "src/display/eh_symbols.h"
#include "src/display/application_icons.h"
#define SM62_FAST_RETURN 1
#define SM62_EFFECT 0
#define SM62_DISPLAY_SYNC_DELAY_MS 2500U
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define LAYER_LABEL_SIZE 16
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define KB_SETTINGS_SM62_SIZE 18
#define KB_SETTINGS_LCD_SIZE 3
#define KB_SETTINGS_SM62_OFFSET 0
#define KB_SETTINGS_LCD_OFFSET 20
#define RPC_SM62_SETTINGS 3
static uint32_t now=10000,layer_state=1,default_layer_state=1;
static bool left=true,master=true,matrix[10][7];
static uint32_t timer_read32(void){return now;}
static uint32_t sync_timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static bool is_keyboard_left(void){return left;}
static bool is_keyboard_master(void){return master;}
static uint8_t sm62_local_side_index(void){return left?0:1;}
static bool matrix_is_on(uint8_t r,uint8_t c){return matrix[r][c];}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static uint8_t get_current_layer(void){return get_highest_layer(layer_state|default_layer_state);}
static void layer_clear(void){layer_state=1;}
static void vial_keycode_down(uint16_t k){(void)k;}
static void vial_keycode_up(uint16_t k){(void)k;}
static void vial_keycode_tap(uint16_t k){(void)k;}
static void hid_app_layout_changed_kb(void){}
static uint16_t dynamic_keymap_get_keycode(uint8_t l,uint8_t r,uint8_t c){(void)l;(void)r;(void)c;return KC_A;}
static const char *layer_name(uint8_t l){(void)l;return "Default";}
static const char *hid_sm62_default_layer_name(uint8_t l){return layer_name(l);}
static uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c){return dynamic_keymap_get_keycode(l,r,c);}
typedef struct {uint16_t qsid;} qmk_settings_proto_t;
static void eeconfig_update_kb_datablock(const void*p,unsigned o,size_t n){(void)p;(void)o;(void)n;}
static void eeconfig_read_kb_datablock(void*p,unsigned o,size_t n){(void)p;(void)o;(void)n;}
static void transaction_register_rpc(int id,void(*cb)(uint8_t,const void*,uint8_t,void*)){(void)id;(void)cb;}
static bool transaction_rpc_send(int id,uint8_t n,const void*p){(void)id;(void)n;(void)p;return true;}
typedef struct {void(*init)(void),(*load)(void),(*housekeep)(void);} eh_screen_t;
static lv_obj_t *display_root,*display_active_view;
#define root_view display_root
static unsigned on_calls,off_calls,flushes,pixels;
static void display_diag_mark(uint32_t n){(void)n;}
DISPLAY_VIEW_PRODUCTION
static void display_turn_on(void){on_calls++;}
static void display_turn_off(void){off_calls++;}
static bool is_display_enabled(void){return true;}
static bool display_init_kb(void){return true;}
static void hid_housekeeping_task(void){}
static uint32_t get_split_lcd_timeout_ms(void){return 0;}
static uint32_t last_matrix_activity_elapsed(void){return 0;}
static uint32_t sm62_display_boot_elapsed(void){return now;}
static void sm62_main_frame_completed(void){}
static void sm62_player_frame_completed(void){}
static void sm62_orbit_frame_completed(void){}
static void sm62_orbit_restart(void){}
static unsigned completed_frames;
static void sm62_diag_frame(void){completed_frames++;}
#include "keyboards/ergohaven/sm62/fps_measure.h"
static fps_state frame_measurement;
static void sm62_fps_frame(uint8_t screen){fps_frame(&frame_measurement,now,screen);}
static void noop(void){}
const eh_screen_t eh_screen_clock={noop,noop,noop},eh_screen_player={noop,noop,noop},eh_screen_orbit={noop,noop,noop};
static void get_keycode_str(char*s,uint16_t k){(void)k;strcpy(s,"A");}
'''
views=read('keyboards/ergohaven/src/display/eh_display.c')
pre=pre.replace('DISPLAY_VIEW_PRODUCTION',''.join(function(views,n) for n in ['display_view_create','display_show_view','display_load_root','display_refresh']))
settings=re.sub(r'^#pragma once','',stripped('keyboards/ergohaven/sm62/sm62_settings.h'),flags=re.M)+stripped('keyboards/ergohaven/sm62/sm62_settings.c')
k=stripped('keyboards/ergohaven/sm62/screen_keymap.c');m=stripped('keyboards/ergohaven/sm62/display_modes.c')
qp=function(read('quantum/painter/lvgl/qp_lvgl.c'),'qp_lvgl_flush')
qp_pre=r'''
typedef void*painter_device_t;
static painter_device_t selected_display=(void*)1;
static void qp_viewport(void*d,int a,int b,int c,int e){(void)d;(void)a;(void)b;(void)c;(void)e;}
static void qp_pixdata(void*d,void*p,uint32_t n){(void)d;(void)p;pixels+=n;}
static void qp_flush(void*d){(void)d;flushes++;}
static bool slice_expired(void){return true;}
'''
test=r'''
static void render(void){lv_tick_inc(32);do{lv_refr_now(NULL);}while(lv_disp_refr_pending(NULL));}
static void setup(void){
 fps_arm(&frame_measurement,now,42,0,0);
 defaults();lv_init();static lv_disp_draw_buf_t buf;static lv_color_t colors[240*28];lv_disp_draw_buf_init(&buf,colors,0,240*28);
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.draw_buf=&buf;drv.refr_slice_cb=slice_expired;drv.flush_cb=qp_lvgl_flush;lv_disp_drv_register(&drv);
 root_view=lv_obj_create(NULL);lv_scr_load(root_view);display_init_screens_kb();display_housekeeping_task();render();
}
static int check(bool b,const char *s){printf("%s %s\n",b?"PASS":"FAIL",s);return !b;}
int main(int argc,char**argv){assert(argc==4);left=atoi(argv[2]);master=atoi(argv[3]);setup();int failures=0;
 if(!strcmp(argv[1],"mode")){
  /* Exercise the actual flush boundary, never call the board callback. */
  lv_disp_drv_t *driver=lv_disp_get_default()->driver;
  lv_area_t area={0,0,0,0};lv_color_t color={0};unsigned before=completed_frames;
  driver->draw_buf->flushing_last=0;qp_lvgl_flush(driver,&area,&color);
  failures+=check(completed_frames==before,"intermediate strip must not complete a frame");
  driver->draw_buf->flushing_last=1;selected_display=NULL;qp_lvgl_flush(driver,&area,&color);
  failures+=check(completed_frames==before,"detached painter must not complete a frame");
  selected_display=(void*)1;qp_lvgl_flush(driver,&area,&color);
  failures+=check(completed_frames==before+1,"final attached strip must complete exactly one frame");
  uint32_t measured=0;for(unsigned s=0;s<5;s++)measured+=frame_measurement.words[8+s];
  failures+=check(measured==completed_frames,"real painter forwards each complete frame to production RAM measurement exactly once");
  driver->draw_buf->flushing_last=0;
  matrix[left?1:6][2]=true;display_housekeeping_task();render();
  matrix[left?1:6][2]=false;now+=1;display_housekeeping_task();render();
  now+=1000;display_housekeeping_task();render();
  printf("after physical release: active=%u waiting=%u off_calls=%u flushes=%u\n",active_screen,waiting_for_keymap_frame,off_calls,flushes);
  failures+=check(active_screen==(left?SM62_SCREEN_CLOCK:SM62_SCREEN_ORBIT),"temporary KEYMAP must return to configured idle mode after release and real painter flush");
  for(unsigned mode=0;mode<4;mode++){
   lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);assert(lv_disp_refr_pending(NULL));
   unsigned old_screen=active_screen,old_frames=completed_frames;
   qmk_settings_proto_t p={left?230:235};uint8_t value=mode,got=255;assert(sm62_setting_set(&p,&value,1)==0);assert(sm62_setting_get(&p,&got,1)==0&&got==mode);now+=300;display_housekeeping_task();
   failures+=check(active_screen==old_screen&&lv_disp_refr_pending(NULL),"mode SET/GET must not replace a partially flushed view");
   render();failures+=check(completed_frames==old_frames+1,"old view frame completes exactly once before mode publication");
   display_housekeeping_task();render();
   printf("mode SET/GET=%u active=%u waiting=%u\n",got,active_screen,waiting_for_keymap_frame);
   unsigned expected=mode==0?SM62_SCREEN_CLOCK:mode==1?SM62_SCREEN_PLAYER:mode==2?SM62_SCREEN_ORBIT:SM62_SCREEN_KEYMAP;
   failures+=check(active_screen==expected,"verified mode setting must govern idle visible view");
  }
  qmk_settings_proto_t disable={left?233:238};uint8_t zero=0;assert(sm62_setting_set(&disable,&zero,1)==0);
  qmk_settings_proto_t modep={left?230:235};uint8_t player=1;assert(sm62_setting_set(&modep,&player,1)==0);assert(sm62_setting_get(&modep,&player,1)==0);now+=300;display_housekeeping_task();render();
  failures+=check(active_screen==SM62_SCREEN_PLAYER,"disabling temporary-keymap override must expose the stored PLAYER mode");
 }
 if(!strcmp(argv[1],"feedback")){
  saved.side[left?0:1].mode=3;saved.side[left?0:1].ship_enabled=0;app_layout_active=true;app_layout_sync_time=now;app_layout_revision=42;
  for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++)app_layout_keycodes[l][c]=KC_A;
  display_housekeeping_task();render();render();pixels=flushes=0;display_housekeeping_task();render();
  failures+=check(pixels==0,"unchanged idle frame must not repaint");pixels=flushes=0;
  uint8_t packet[32]={0xe8,8,42,0,0,0,0,0,0};assert(hid_app_layout_process_packet(packet,32));display_housekeeping_task();render();
  failures+=check(pixels==0,"current Entropy receipt-only E8 state=0 must not repaint idle nonvisual controls");pixels=flushes=0;
  packet[8]=1;assert(hid_app_layout_process_packet(packet,32));display_housekeeping_task();render();
  printf("nonvisual/offscreen execution feedback: pixels=%u flushes=%u off_calls=%u\n",pixels,flushes,off_calls);
  failures+=check(pixels==0,"execution feedback for offscreen nonvisual control must not repaint unrelated visible keymap");
 }
 if(!strcmp(argv[1],"typing")){
  saved.side[left?0:1].mode=3;saved.side[left?0:1].ship_enabled=0;
  display_housekeeping_task();render();render();pixels=flushes=0;
  matrix[left?1:6][2]=true;display_housekeeping_task();render();
  printf("physical in-grid press (no app/feedback): pixels=%u flushes=%u off_calls=%u\\n",pixels,flushes,off_calls);
  failures+=check(pixels<=75U*75U,"in-grid key highlight must not repaint unrelated screen area");
 }
 return failures?1:0;
}
'''
src=out/'display_probe.c';src.write_text(pre+firewall_source(read,keycodes_included=True)+h+settings+stripped('keyboards/ergohaven/sm62/application_display.c')+k+m+qp_pre+qp+test)
fonts=['eh_font_iosevka_18','eh_font_iosevka_28'];fontpaths=[root/f'keyboards/ergohaven/src/display/fonts/{f}.c' for f in fonts]
fontpaths += [root/f'keyboards/ergohaven/macropad/sm62_font_zen_medium_{n}.c' for n in [16,17,20,28]]
cmd=['gcc','-include','/home/agent/.hermes/profiles/ben/cache/scratch/sm62-test16-final/firmware/quantum/sm62_trace.h','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_COOPERATIVE_REFR=1','-DLV_USE_LOG=1','-DLV_USE_ASSERT_MALLOC=1','-DKEYBOARD_ergohaven_sm62','-DEH_APP_LAYOUT_ENABLE',*['-I'+str(x) for x in [out/'stubs',a.lvgl,root,root/'quantum',root/'quantum/keymap_extras',root/'quantum/sequencer',root/'keyboards/ergohaven']],str(src),*map(str,fontpaths),*sorted(str(f) for f in (a.lvgl/'src').rglob('*.c')),'-lm','-o',str(out/'display_probe')]
(out/'compile-command.json').write_text(json.dumps(cmd,indent=2));subprocess.run(cmd,check=True)
logs=[];codes=[]
for left in [0,1]:
 for master in [0,1]:
  run=[str(out/'display_probe'),a.case,str(left),str(master)];r=subprocess.run(run,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);logs.append(f'GP29={left} USB={master} exit={r.returncode}\n'+r.stdout);codes.append(r.returncode)
(out/(a.case+'.log')).write_text(''.join(logs));print(''.join(logs));sys.exit(any(codes))
