#!/usr/bin/env python3
"""Real QMK activity receiver/clock and SM62 display policy; hardware boundary counters."""
from pathlib import Path
import argparse,re,subprocess,json
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]);a=p.parse_args();R=a.source;O=a.output;O.mkdir(parents=True,exist_ok=True)
def read(n):return (R/n).read_text()
def fun(s,n):
 m=re.search(r'^(?:static )?[^\n;]+\b'+n+r'\([^;\n]*\)\s*\{',s,re.M);assert m,n
 end=s.index('{',m.start())+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[m.start():end]+'\n'
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define SM62_FAST_RETURN 1
#define SM62_DISPLAY_SYNC_DELAY_MS 2500
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TIMER_DIFF_32(a,b) ((uint32_t)((a)-(b)))
static bool master,left,persistent_off;
static uint32_t now=10000,timeout_ms,ons,offs;
static int32_t sync_timer_ms;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static bool is_keyboard_master(void){return master;}
static void hid_housekeeping_task(void){}
static bool sm62_local_mode_transition_pending(void){return false;}
static bool is_display_enabled(void){return true;}
static bool display_init_kb(void){return true;}
static bool sm62_local_screens_off(void){return persistent_off;}
static void display_turn_off(void){offs++;}
static void display_turn_on(void){ons++;}
static void display_load_root(void){}
static void display_refresh(void){}
static uint32_t get_split_lcd_timeout_ms(void){return timeout_ms;}
static uint32_t sm62_display_boot_elapsed(void){return now;}
static bool sm62_keymap_observe_matrix(void){return false;}
static bool sm62_keymap_visual_active(void){return false;}
static uint16_t sm62_local_keymap_hold_ms(void){return 500;}
static uint32_t sm62_local_orbit_idle_ms(uint32_t idle){return idle;} /* No settings edits in this power-only fixture. */
static void sm62_main_frame_completed(void){}
static void sm62_player_frame_completed(void){}
static void sm62_orbit_frame_completed(void){}
static void sm62_orbit_restart(void){}
static void sm62_diag_frame(void){}
static void sm62_fps_frame(uint8_t screen){(void)screen;}
typedef struct{void(*init)(void),(*load)(void),(*housekeep)(void);}eh_screen_t;
static void noop(void){}
const eh_screen_t eh_screen_clock={noop,noop,noop},eh_screen_player={noop,noop,noop},eh_screen_keymap={noop,noop,noop},eh_screen_orbit={noop,noop,noop};
'''
sync=read('quantum/sync_timer.c');kb=read('quantum/keyboard.c');transactions=read('quantum/split_common/transactions.c')
pre+=''.join(fun(sync,n) for n in ['sync_timer_update','sync_timer_read32','sync_timer_elapsed32'])
pre+=kb[kb.index('static uint32_t last_input_modification_time'):kb.index('// Only enable this if console')]
pre+='typedef uint16_t matrix_row_t;\nstatic struct {struct {uint32_t matrix_timestamp,encoder_timestamp,pointing_device_timestamp;}activity_sync;} shmem,*split_shmem=&shmem;\n'
pre+=fun(transactions,'activity_handlers_slave')
header=read('keyboards/ergohaven/sm62/sm62_settings.h');pre+=header[header.index('typedef struct'):header.index('const sm62_side_settings_t')]
pre+='static sm62_side_settings_t settings={0,0,0,1,0};\nstatic const sm62_side_settings_t *sm62_local_display_settings(void){return &settings;}\n'
policy=re.sub(r'^#include[^\n]*','',read('keyboards/ergohaven/sm62/display_modes.c'),flags=re.M)
test=r'''
static int failed;
static void check(bool ok,const char *name){printf("%s %s\n",ok?"PASS":"FAIL",name);failed+=!ok;}
static void receive(uint32_t activity){shmem.activity_sync.matrix_timestamp=activity;activity_handlers_slave(NULL,NULL);}
int main(int argc,char**argv){assert(argc==3);left=atoi(argv[1]);master=atoi(argv[2]);(void)left;display_init_screens_kb();display_housekeeping_task();ons=offs=0;
 timeout_ms=5000;
 for(unsigned i=0;i<20;i++){
  /* Independent split clock snapshot is 1ms behind the newly received master key timestamp. */
  sync_timer_update(now);receive(now+(master?0:1));display_housekeeping_task();now+=1;display_housekeeping_task();now+=7;
 }
 printf("activity-driven transitions: off=%u on=%u role=%u\n",offs,ons,master);
 check(offs==0,"fresh leading-half typing must not power off peer under 1ms split clock skew");
 if(!master){
  for(unsigned skew=1;skew<=1000;skew++){
   offs=0;sync_timer_update(now);receive(now+skew);display_housekeeping_task();
   if(offs){printf("FAIL bounded future skew %u\\n",skew);failed++;break;}
  }
  check(offs==0,"bounded 1..1000ms future skew does not power off peer");
 }
 /* Distinct local/shared clocks: the activity timestamp is in shared time. */
 offs=0;sync_timer_update(now-700);receive(sync_timer_read32());display_housekeeping_task();check(offs==0,"local clock offset does not become matrix idle");
 sync_timer_update(now);receive(now-0x80000001U);offs=0;display_housekeeping_task();check(offs==1,"genuine long inactivity is not clamped as clock skew");
 offs=0;sync_timer_update(now);receive(now);now+=4999;display_housekeeping_task();check(offs==0,"finite timeout stays on before deadline");now++;display_housekeeping_task();check(offs==1,"finite timeout still turns off exactly at deadline");
 receive(now);display_housekeeping_task();check(!screens_were_off,"new matrix activity wakes expired display");
 offs=0;timeout_ms=0;receive(now+1);display_housekeeping_task();check(offs==0,"Never remains on under skew");
 persistent_off=true;display_housekeeping_task();check(offs==1,"persistent off remains authoritative");persistent_off=false;
 timeout_ms=5000;offs=0;now=3;sync_timer_update(now);receive(UINT32_MAX-999U);display_housekeeping_task();check(offs==0,"real timer rollover is not early expiry");now=5000;display_housekeeping_task();check(offs==1,"real rollover still reaches timeout");
 return failed?1:0;
}
'''
source=O/'peer_policy.c';source.write_text(pre+policy+test);cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-fsanitize=undefined','-fno-sanitize-recover=all',str(source),'-o',str(O/'peer_policy')];subprocess.run(cmd,check=True);results=[]
for left in [0,1]:
 for master in [0,1]:
  r=subprocess.run([str(O/'peer_policy'),str(left),str(master)],capture_output=True,text=True);results.append({'left':left,'master':master,'exit':r.returncode,'output':r.stdout+r.stderr})
(O/'results.json').write_text(json.dumps(results,indent=2));print('\n'.join(x['output'] for x in results));raise SystemExit(any(x['exit'] for x in results))
