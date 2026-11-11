#!/usr/bin/env python3
"""Source-backed service guards and painter lifecycle, with guard mutations."""
from pathlib import Path
import re,subprocess,json,resource
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT.parent/'regressions/guards';OUT.mkdir(parents=True,exist_ok=True)
def body(s,sig):
 start=s.index(sig);a=s.index('{',start);n=1;b=a+1
 while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[start:b]+'\n'
service=(ROOT/'keyboards/ergohaven/sm62/display_input.c').read_text().replace('#include "keyboard.h"','')
flush=body((ROOT/'quantum/painter/lvgl/qp_lvgl.c').read_text(),'void qp_lvgl_flush(')
base='''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {int x1,x2,y1,y2;} lv_area_t;
typedef unsigned lv_color_t;
typedef struct {bool last;} lv_disp_drv_t;
static void *selected_display=(void*)1;
static unsigned calls,depth,max_depth,frames,ready,flushes;
static bool locked;
void qp_lvgl_flush_input_service(void);
void protocol_keyboard_task(void);
static void keyboard_task(void){assert(!locked);calls++;depth++;if(depth>max_depth)max_depth=depth;assert(depth==1);qp_lvgl_flush_input_service();protocol_keyboard_task();depth--;}
static void qp_viewport(void*d,int a,int b,int c,int e){(void)d;(void)a;(void)b;(void)c;(void)e;assert(!locked);locked=true;locked=false;}
static void qp_pixdata(void*d,void*p,uint32_t n){(void)d;(void)p;(void)n;assert(!locked);locked=true;locked=false;}
static void qp_flush(void*d){(void)d;assert(!locked);locked=true;flushes++;locked=false;}
static bool lv_disp_flush_is_last(lv_disp_drv_t*d){return d->last;}
static void display_frame_completed(void){assert(!locked);frames++;}
static void lv_disp_flush_ready(lv_disp_drv_t*d){(void)d;ready++;}
'''
main='''int main(int argc,char**argv){(void)argc;unsigned mode=atoi(argv[1]);
if(mode==0){qp_lvgl_flush_input_service();assert(!calls);puts("PASS initialization");return 0;}
protocol_keyboard_task();assert(calls==1 && max_depth==1 && sm62_input_initialized && !sm62_input_running);
puts("PASS ordinary-task + callback and wrapper reentry");
lv_area_t a={0,239,0,3};lv_color_t p[960];lv_disp_drv_t d={0};
qp_lvgl_flush(&d,&a,p);assert(calls==2 && flushes==1 && !frames && ready==1);
d.last=true;qp_lvgl_flush(&d,&a,p);assert(calls==3 && flushes==2 && frames==1 && ready==2);
selected_display=0;qp_lvgl_flush(&d,&a,p);assert(calls==3 && flushes==2 && frames==1 && ready==2);
puts("PASS attached intermediate + final exactly once + detached + unlocked service");return 0;}
'''
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
results=[]
for name,src,arg,expected in [('normal-init',service,0,0),('normal-lifecycle',service,1,0),('mutation-init',service.replace('!sm62_input_initialized || ',''),0,-6),('mutation-reentry',service.replace('    if (sm62_input_running) return;',''),1,-6)]:
 c=OUT/(name+'.c');c.write_text(base+src+flush+main);exe=OUT/name
 r=subprocess.run(['gcc','-include','/home/agent/.hermes/profiles/ben/cache/scratch/sm62-test16-final/firmware/quantum/sm62_trace.h','-std=c11','-O1','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],capture_output=True,text=True);assert r.returncode==0,r.stderr
 r=subprocess.run([str(exe),str(arg)],capture_output=True,text=True);(OUT/(name+'.log')).write_text(r.stdout+r.stderr)
 assert r.returncode==expected,(name,r.returncode,r.stderr)
 results.append({'name':name,'exit':r.returncode});print(name,r.returncode,r.stdout,r.stderr)
(OUT/'results.json').write_text(json.dumps(results,indent=2))
