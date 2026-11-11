#!/usr/bin/env python3
"""Production SM62 startup policy with independent boot paths/clocks.

Panel, views and settings are injected. Boot elapsed and display policy are
production functions. This proves the release-policy asymmetry, not USB timing.
"""
from pathlib import Path
import argparse,re,subprocess,json
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def fun(text,name):
 m=re.search(r'^(?:static )?[^\n;]+\b'+name+r'\([^;\n]*\)\s*\{',text,re.M);assert m,name
 e=text.index('{',m.start())+1;d=1
 while d:d+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[m.start():e]+'\n'
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define SM62_FAST_RETURN 1
#define SM62_DISPLAY_SYNC_DELAY_MS 2500U
static uint32_t now,wall,diag_absolute[6],first_on=UINT32_MAX,timeout_ms;
static bool master,persistent_off,panel_on;
static unsigned off_transitions,on_transitions;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static bool is_keyboard_master(void){return master;}
static void hid_housekeeping_task(void){}
static bool sm62_local_mode_transition_pending(void){return false;}
static bool is_display_enabled(void){return true;}
static bool display_init_kb(void){return true;}
static bool sm62_local_screens_off(void){return persistent_off;}
static void display_turn_off(void){if(panel_on){off_transitions++;panel_on=false;}}
static void display_turn_on(void){if(!panel_on){panel_on=true;on_transitions++;if(first_on==UINT32_MAX)first_on=wall;}}
static void display_load_root(void){}
static void display_refresh(void){}
static uint32_t get_split_lcd_timeout_ms(void){return timeout_ms;}
static uint32_t last_matrix_activity_elapsed(void){return 0;}
static bool sm62_keymap_observe_matrix(void){return false;}
static bool sm62_keymap_visual_active(void){return false;}
static uint16_t sm62_local_keymap_hold_ms(void){return 350;}
static uint32_t sm62_local_orbit_idle_ms(uint32_t idle){return idle;} /* No settings edits in this startup-only fixture. */
static void sm62_main_frame_completed(void){}
static void sm62_player_frame_completed(void){}
static void sm62_orbit_frame_completed(void){}
static void sm62_orbit_restart(void){}
static void sm62_diag_frame(void){}
static void sm62_fps_frame(uint8_t screen){(void)screen;}
typedef struct{void(*init)(void),(*load)(void),(*housekeep)(void);}eh_screen_t;
static void noop(void){}
const eh_screen_t eh_screen_clock={noop,noop,noop},eh_screen_player={noop,noop,noop},eh_screen_keymap={noop,noop,noop},eh_screen_orbit={noop,noop,noop};
typedef struct{uint8_t mode,ship_enabled,ship_timeout_s,keymap_enabled,cyrillic_enabled;}sm62_side_settings_t;
static sm62_side_settings_t settings={0,0,0,1,0};
static const sm62_side_settings_t *sm62_local_display_settings(void){return &settings;}
'''
board=(a.source/'keyboards/ergohaven/sm62/sm62.c').read_text()
pre+=fun(board,'keyboard_pre_init_user')+fun(board,'sm62_display_boot_elapsed')
policy=re.sub(r'^#include[^\n]*','',(a.source/'keyboards/ergohaven/sm62/display_modes.c').read_text(),flags=re.M)
main=r'''
int main(int argc,char**argv){
 assert(argc==5);master=atoi(argv[1]);uint32_t ready=strtoul(argv[2],0,10),offset=strtoul(argv[3],0,10);persistent_off=atoi(argv[4]);
 now=offset;keyboard_pre_init_user();wall=ready;now=offset+wall;display_init_screens_kb();
 for(;wall<=3000;wall++){now=offset+wall;display_housekeeping_task();}
 uint32_t expected=ready>2500?ready:2500;
 if(persistent_off){assert(first_on==UINT32_MAX);persistent_off=false;wall=3100;now=offset+wall;display_housekeeping_task();expected=3100;}
 printf("role=%s ready=%u clock_offset=%u first_on=%u expected=%u on_transitions=%u off_transitions=%u\n",master?"master":"slave",ready,offset,first_on,expected,on_transitions,off_transitions);
 if(first_on!=expected||on_transitions!=1||off_transitions!=0){puts("FAIL startup release must use the same pre-init deadline on both halves without a dark pulse");return 1;}
 puts("PASS startup release, persistent-off/wake and rollover controls");return 0;
}
'''
source=a.output/'startup_policy.c';source.write_text(pre+policy+main)
subprocess.run(['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all',str(source),'-o',str(a.output/'startup_policy')],check=True)
results=[]
for master in [0,1]:
 for ready,offset,off in [(2000 if not master else 2400,0,0),(2000 if not master else 2400,700,0),(2000 if not master else 2400,4294967280,0),(2800,0,0),(2000,0,1)]:
  r=subprocess.run([str(a.output/'startup_policy'),str(master),str(ready),str(offset),str(off)],capture_output=True,text=True)
  results.append({'master':master,'ready_ms':ready,'clock_offset':offset,'persistent_off':off,'exit':r.returncode,'output':r.stdout+r.stderr})
(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(''.join(d['output'] for d in results));raise SystemExit(any(d['exit'] for d in results))
