#!/usr/bin/env python3
"""Production split backlight receiver versus actual SM62 panel policy."""
from pathlib import Path
import argparse,re,subprocess,json
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]);a=p.parse_args();R=a.source;O=a.output;O.mkdir(parents=True,exist_ok=True)
def fun(s,n):
 m=re.search(r'^(?:static )?[^\n;]+\b'+n+r'\([^;\n]*\)\s*\{',s,re.M);assert m,n
 e=s.index('{',m.start())+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
view=(R/'keyboards/ergohaven/src/display/eh_display.c').read_text();tx=(R/'quantum/split_common/transactions.c').read_text()
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
typedef uint16_t matrix_row_t;
static struct {uint8_t backlight_level;} shared,*split_shmem=&shared;
static unsigned writes,dark_pulses;static uint8_t brightness=3;
static bool is_display_on=true,painter_ready=true;static void *display=(void*)1;
static bool display_power_off,display_power_known=true,display_power_requested,display_power_request_seen;
static uint8_t display_power_attempts;static uint32_t display_power_attempted_at;
static uint32_t timer_read32(void){return 10000;}
static uint32_t timer_elapsed32(uint32_t t){return 10000-t;}
static void backlight_level_noeeprom(uint8_t n){if(brightness && !n)dark_pulses++;brightness=n;writes++;}
static uint8_t get_split_lcd_brightness(void){return 3;}
static bool qp_power(void*d,bool on){(void)d;(void)on;return true;}
static bool display_user_enabled_kb(void){return true;}
static inline void split_shared_memory_lock(void){}
static inline void split_shared_memory_unlock(void){}
void display_turn_off(void);
'''
source=pre+''.join(fun(view,n) for n in ['sm62_request_display_power','display_turn_on','display_turn_off'])+fun(tx,'backlight_handlers_slave')+r'''
int main(void){
 display_turn_on();writes=dark_pulses=0;
 for(unsigned i=0;i<20;i++){split_shmem->backlight_level=0;backlight_handlers_slave(NULL,NULL);display_turn_on();}
 printf("lit display: writes=%u dark_pulses=%u\n",writes,dark_pulses);
#ifdef KEYBOARD_ergohaven_sm62
 bool ok=dark_pulses==0;printf("%s SM62 split backlight must not override local on policy\n",ok?"PASS":"FAIL");
 display_turn_off();writes=0;split_shmem->backlight_level=3;backlight_handlers_slave(NULL,NULL);
 bool stays_off=brightness==0&&writes==0;printf("%s SM62 explicit off must not be undone by remote brightness\n",stays_off?"PASS":"FAIL");return !(ok&&stays_off);
#else
 bool ok=dark_pulses==20;printf("%s unrelated boards retain generic split backlight behavior\n",ok?"PASS":"FAIL");return !ok;
#endif
}
'''
c=O/'backlight.c';c.write_text(source);results=[]
for target in [True,False]:
 exe=O/('sm62' if target else 'other');cmd=['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=undefined','-fno-sanitize-recover=all',*(['-DKEYBOARD_ergohaven_sm62'] if target else []),str(c),'-o',str(exe)];subprocess.run(cmd,check=True);r=subprocess.run([str(exe)],capture_output=True,text=True);results.append({'target':target,'exit':r.returncode,'output':r.stdout+r.stderr});print(r.stdout+r.stderr)
(O/'results.json').write_text(json.dumps(results,indent=2));raise SystemExit(any(x['exit'] for x in results))
