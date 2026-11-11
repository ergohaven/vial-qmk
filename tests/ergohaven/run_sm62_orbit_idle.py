#!/usr/bin/env python3
"""Real settings SET/GET, split packets, QMK shared clock and display policy.
Only hardware/view boundaries are injected; no timer policy is copied.
"""
import argparse, json, re, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
from macropad_native_support import write_stubs
# This SM62 target includes the already approved timer-fairness overlay.
# Keep the shared historical Macropad validator unchanged; use the complete
# SM62 manifest validator rather than weakening a pinned-blob check.
inputs=Path(__file__).resolve().parent/'sm62_test19/inputs'
scope={'INPUTS':inputs}
exec(compile((inputs/'validate_lvgl.py').read_text(),'validate_lvgl.py','exec'),scope)
lvgl=a.source/'lib/lvgl';scope['validate_lvgl'](lvgl);write_stubs(a.output/'stubs')
def stripped(n):
    return re.sub(r'^#(?:include[^\n]*|pragma once)','',(a.source/n).read_text(),flags=re.M)
pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "lvgl.h"
#define SPLIT_KEYBOARD
#define TIMER_DIFF_32(a,b) ((uint32_t)((a)-(b)))
#define TIMER_DIFF_16(a,b) ((uint16_t)((a)-(b)))
uint32_t sync_timer_read32(void);
#define KB_SETTINGS_SM62_SIZE 18
#define KB_SETTINGS_LCD_SIZE 3
#define KB_SETTINGS_SM62_OFFSET 32
#define KB_SETTINGS_LCD_OFFSET 29
#define RPC_SM62_SETTINGS 3
#define SM62_FAST_RETURN 1
#define SM62_DISPLAY_SYNC_DELAY_MS 2500U
static uint32_t now,wall,clock_offset,matrix_idle;
static bool master=true,key_change,panel_on;
static uint8_t side;
static unsigned writes;
static uint8_t eeprom[128],packet[128];static uint8_t packet_len;
static void(*receiver)(uint8_t,const void*,uint8_t,void*);
static uint32_t timer_read32(void){return now;}
static uint16_t timer_read(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint16_t timer_elapsed(uint16_t t){return now-t;}
static bool is_keyboard_master(void){return master;}
static uint8_t sm62_local_side_index(void){return side;}
static void eeconfig_read_kb_datablock(void*p,unsigned o,size_t n){assert(o+n<=sizeof(eeprom));memcpy(p,eeprom+o,n);}
static void eeconfig_update_kb_datablock(const void*p,unsigned o,size_t n){assert(o+n<=sizeof(eeprom));memcpy(eeprom+o,p,n);writes++;}
static void transaction_register_rpc(int id,void(*cb)(uint8_t,const void*,uint8_t,void*)){assert(id==RPC_SM62_SETTINGS);receiver=cb;}
static bool transaction_rpc_send(int id,uint8_t n,const void*p){assert(id==RPC_SM62_SETTINGS&&n<=sizeof(packet));packet_len=n;memcpy(packet,p,n);return true;}
typedef struct{uint16_t qsid;}qmk_settings_proto_t;
static void hid_housekeeping_task(void){}
static bool is_display_enabled(void){return true;}
static bool display_init_kb(void){return true;}
static void display_turn_off(void){panel_on=false;}
static void display_turn_on(void){panel_on=true;}
static void display_load_root(void){}
static void display_refresh(void){}
static uint32_t get_split_lcd_timeout_ms(void){return matrix_idle==UINT32_MAX?1:0;}
static uint32_t last_matrix_activity_elapsed(void){return matrix_idle;}
static bool sm62_keymap_observe_matrix(void){bool v=key_change;key_change=false;return v;}
static bool sm62_keymap_visual_active(void){return false;}
void sm62_main_frame_completed(void){}
void sm62_player_frame_completed(void){}
void sm62_orbit_frame_completed(void){}
void sm62_orbit_restart(void){}
void sm62_diag_frame(void){}
void sm62_fps_frame(uint8_t n){(void)n;}
uint32_t sm62_display_boot_elapsed(void){return 5000;}
typedef struct{void(*init)(void),(*load)(void),(*housekeep)(void);}eh_screen_t;
static void noop(void){}
const eh_screen_t eh_screen_clock={noop,noop,noop},eh_screen_player={noop,noop,noop},eh_screen_keymap={noop,noop,noop},eh_screen_orbit={noop,noop,noop};
static void *selected_display=(void*)1;
static void qp_viewport(void*d,int a,int b,int c,int e){(void)d;(void)a;(void)b;(void)c;(void)e;}
static void qp_pixdata(void*d,void*p,uint32_t n){(void)d;(void)p;(void)n;}
static void qp_flush(void*d){(void)d;}
static bool slice_expired(void){return true;}
'''
main=r'''
static unsigned checks;
static void tick(uint32_t t){wall=t;now=clock_offset+wall;}
static void set(unsigned q,uint8_t v){qmk_settings_proto_t p={q};assert(sm62_setting_set(&p,&v,1)==0);}
static void expect(unsigned n,const char *why){display_housekeeping_task();checks++;if(sm62_diag_screen()!=n){fprintf(stderr,"FAIL %s wall=%u expected=%u actual=%u\n",why,wall,n,sm62_diag_screen());exit(1);}}
static void render(void){lv_obj_invalidate(lv_scr_act());lv_tick_inc(32);do{lv_refr_now(NULL);}while(lv_disp_refr_pending(NULL));}
static void init(void){
 lv_init();static lv_disp_draw_buf_t buf;static lv_color_t colors[240*28];lv_disp_draw_buf_init(&buf,colors,0,240*28);
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.draw_buf=&buf;drv.refr_slice_cb=slice_expired;drv.flush_cb=qp_lvgl_flush;lv_disp_drv_register(&drv);
 memset(eeprom,255,sizeof(eeprom));kb_settings_extra_init();sm62_settings_register_rpc();/* Persisted ordinary clock on both halves, idle disabled. */for(unsigned i=0;i<2;i++){saved.side[i].mode=0;saved.side[i].ship_enabled=0;saved.side[i].ship_timeout_s=20;}display_init_screens_kb();}
int main(int argc,char**argv){
 assert(argc>=4);side=atoi(argv[2]);clock_offset=strtoul(argv[3],0,0);tick(0);init();
 unsigned en=231+side*5,to=en+1;
 if(!strcmp(argv[1],"send")){en=231+(1-side)*5;to=en+1;} /* USB master edits the physical peer. */
 if(!strcmp(argv[1],"local")){
  tick(10000);set(to,5);set(en,1);expect(0,"10s disabled then timeout 5s and enable starts fresh");
  tick(14999);expect(0,"full five seconds minus 1ms");tick(15000);expect(3,"exact restarted deadline");
 }else if(!strcmp(argv[1],"key")){
  tick(10000);set(to,5);set(en,1);tick(14000);key_change=true;expect(2,"key after enable uses independent activity timer");render();
  tick(14350);expect(0,"key hold expires independently");tick(15000);matrix_idle=0;expect(0,"shared other-half activity is not local ORBIT activity");tick(18999);expect(0,"local key restarts idle");tick(19000);expect(3,"local key exact deadline");
  tick(20000);matrix_idle=0;expect(3,"shared other-half activity cannot leave local ORBIT");
 }else if(!strcmp(argv[1],"thresholds")){
  tick(10000);set(en,1);set(to,5);
  tick(11000);set(to,2);expect(0,"timeout edit while old elapsed below new threshold");tick(12999);expect(0,"below full countdown");tick(13000);expect(3,"below exact countdown");
  set(to,3);expect(0,"timeout edit while old elapsed equals threshold");tick(15999);expect(0,"equal full countdown");tick(16000);expect(3,"equal exact countdown");
  tick(20000);set(to,1);expect(0,"timeout edit while old elapsed above lower threshold");tick(20999);expect(0,"above full countdown");tick(21000);expect(3,"above exact countdown");
 }else if(!strcmp(argv[1],"controls")){
  tick(10000);set(en,1);tick(10010);set(to,5);expect(0,"enable then timeout order");
  tick(14000);set(to,5);set(en,1);set(240+side,0);set(232+(1-side)*5,6);
  qmk_settings_proto_t p={to};uint8_t v;assert(sm62_setting_get(&p,&v,1)==0&&v==5);
  v=241;assert(sm62_setting_set(&p,&v,1)==-1);
  expect(0,"unchanged GET invalid unrelated other side do not change view");
  tick(15009);expect(0,"unchanged edits do not shorten deadline");tick(15010);expect(3,"unchanged edits do not extend deadline");
  tick(16000);set(to,2);expect(0,"editing already ORBIT returns to clock");tick(17999);expect(0,"lower timeout has full countdown");tick(18000);expect(3,"lower exact deadline");
  tick(18001);set(to,0);expect(3,"zero immediate ORBIT");set(en,0);expect(0,"disable immediately returns clock");
  tick(19000);set(to,5);tick(24000);expect(0,"disabled timeout change never enables");set(en,1);expect(0,"off to on same timeout restarts");
  tick(28999);expect(0,"reenable full period");tick(29000);expect(3,"reenable exact deadline");
  tick(30000);key_change=true;expect(2,"local key enters KEYMAP");
  /* Pinned LVGL -> real painter final strip -> production completion callback. */
  render();tick(30349);set(to,6);expect(2,"timeout does not alter local KEYMAP hold");tick(30350);expect(0,"KEYMAP hold not restarted by timeout");
  tick(36348);expect(0,"fresh timeout independent of key hold");tick(36349);expect(3,"fresh timeout deadline");
  tick(37000);set(248+side,1);expect(4,"persistent off precedence");set(to,2);tick(38000);expect(4,"edits never wake persistent off");set(248+side,0);expect(0,"wake retains elapsed new countdown");tick(39000);expect(3,"wake deadline");
  tick(40000);matrix_idle=UINT32_MAX;expect(4,"matrix sleep precedence");set(to,5);tick(42000);matrix_idle=0;expect(0,"sleep edits do not fake matrix activity");tick(45000);expect(3,"sleep countdown exact deadline");
  tick(46000);set(230+side*5,2);tick(50000);expect(3,"permanent mode 2");set(to,20);expect(3,"permanent mode unaffected by timeout");set(en,0);expect(3,"permanent mode unaffected by idle off");
  set(230+side*5,0);tick(55000);expect(0,"leaving permanent disables idle");
 }else if(!strcmp(argv[1],"send")){
  assert(argc==5);tick(10000);set(to,5);set(en,1);tick(11000);sm62_settings_sync_task();assert(packet_len);FILE*f=fopen(argv[4],"wb");assert(f);assert(fwrite(packet,1,packet_len,f)==packet_len);fclose(f);
  tick(12000);set(232+side*5,6);tick(13000);sm62_settings_sync_task();char other[1024];snprintf(other,sizeof(other),"%s.other",argv[4]);f=fopen(other,"wb");assert(f);assert(fwrite(packet,1,packet_len,f)==packet_len);fclose(f);
  tick(16000);set(en,0);set(en,1);tick(17000);sm62_settings_sync_task();char next[1024];snprintf(next,sizeof(next),"%s.next",argv[4]);f=fopen(next,"wb");assert(f);assert(fwrite(packet,1,packet_len,f)==packet_len);fclose(f);puts("PASS actual production sender packets: peer edit plus epoch-only off/on");return 0;
 }else if(!strcmp(argv[1],"future")){
  assert(argc==7);master=false;uint32_t sender_offset=strtoul(argv[5],0,0),skew=strtoul(argv[6],0,0);
  tick(11000);sync_timer_update(sender_offset+10000-skew);FILE*f=fopen(argv[4],"rb");assert(f);packet_len=fread(packet,1,sizeof(packet),f);fclose(f);
  receiver(packet_len,packet,0,NULL);expect(0,"bounded future shared-clock epoch is not 49-day idle");
  tick(15999);sync_timer_update(sender_offset+wall);receiver(packet_len,packet,0,NULL);expect(0,"future clamp retained in local clock across shared-clock correction");
  tick(16000);expect(3,"bounded future epoch conservative full countdown expires");
 }else if(!strcmp(argv[1],"receive")){
  assert(argc==7);master=false;uint32_t sender_offset=strtoul(argv[5],0,0),arrival=strtoul(argv[6],0,0);/* Real QMK conversion with independent local offset and delayed delivery. */
  tick(arrival);sync_timer_update(sender_offset+wall);FILE*f=fopen(argv[4],"rb");assert(f);packet_len=fread(packet,1,sizeof(packet),f);fclose(f);
  unsigned before=writes;receiver(packet_len,packet,0,NULL);expect(0,"delayed peer settings start from edit not arrival");
  char other[1024];snprintf(other,sizeof(other),"%s.other",argv[4]);f=fopen(other,"rb");assert(f);packet_len=fread(packet,1,sizeof(packet),f);fclose(f);
  tick(14000);sync_timer_update(sender_offset+wall);receiver(packet_len,packet,0,NULL);expect(0,"other-side epoch does not reset local countdown");
  tick(14999);sync_timer_update(sender_offset+wall);receiver(packet_len,packet,0,NULL);expect(0,"identical periodic replay does not restart");
  tick(15000);expect(3,"delayed peer shares actual sender deadline");
  tick(18000);sm62_settings_sync_task();before=writes;
  char next[1024];snprintf(next,sizeof(next),"%s.next",argv[4]);f=fopen(next,"rb");assert(f);packet_len=fread(packet,1,sizeof(packet),f);fclose(f);
  tick(18000);sync_timer_update(sender_offset+wall);receiver(packet_len,packet,0,NULL);expect(0,"epoch-only coalesced off/on restarts peer");
  tick(20999);receiver(packet_len,packet,0,NULL);expect(0,"epoch-only replay does not restart");tick(21000);expect(3,"epoch-only exact deadline");
  tick(24000);sm62_settings_sync_task();assert(writes==before);expect(3,"runtime-only update causes no EEPROM wear");
  uint8_t bad[128];memcpy(bad,packet,packet_len);bad[0]^=1;tick(25000);receiver(packet_len,bad,0,NULL);expect(3,"invalid saved settings rejected without resetting");
  receiver(packet_len-1,packet,0,NULL);expect(3,"wrong RPC length rejected without resetting");
  (void)before;
 }else abort();
 printf("PASS %u production settings/display assertions side=%u offset=%u scenario=%s\n",checks,side,clock_offset,argv[1]);return 0;
}
'''
qp=(a.source/'quantum/painter/lvgl/qp_lvgl.c').read_text();start=qp.index('void qp_lvgl_flush(');end=qp.index('\nstatic uint32_t tick_task_callback',start);qp=qp[start:end]
src=a.output/'orbit_idle.c';src.write_text(pre+stripped('quantum/sync_timer.c')+stripped('keyboards/ergohaven/sm62/sm62_settings.h')+stripped('keyboards/ergohaven/sm62/sm62_settings.c')+stripped('keyboards/ergohaven/sm62/display_modes.c')+qp+main)
exe=a.output/'orbit_idle'
subprocess.run(['gcc','-include',str(a.source/'quantum/sm62_trace.h'),'-std=gnu11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_COOPERATIVE_REFR=1','-DLV_USE_LOG=1','-DLV_USE_ASSERT_MALLOC=1','-I'+str(a.output/'stubs'),'-I'+str(lvgl),str(src),*sorted(str(f) for f in (lvgl/'src').rglob('*.c')),'-lm','-o',str(exe)],check=True)
results=[]
def run(args):
 r=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True);results.append({'args':list(map(str,args)),'exit':r.returncode,'output':r.stdout+r.stderr})
for side in (0,1):
 for offset in (0,700000,4294955000):
  for case in ('local','controls','key','thresholds'):run([case,side,offset])
 # The sender's target is the opposite physical side; USB orientation varies.
 for sender_offset in (0,4294955000,4294957296):
  packet=a.output/f'packet-{side}-{sender_offset}.bin';run(['send',1-side,sender_offset,packet])
  for peer_offset in (700000,4294955000):
   for arrival in (11000,13500):run(['receive',side,peer_offset,packet,sender_offset,arrival])
  for skew in (1,1000):run(['future',side,700000,packet,sender_offset,skew])
(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(''.join(x['output'] for x in results),end='')
raise SystemExit(any(x['exit'] for x in results))
