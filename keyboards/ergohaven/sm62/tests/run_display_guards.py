#!/usr/bin/env python3
"""Source-backed service guards and painter lifecycle, with guard mutations."""
from pathlib import Path
import argparse,subprocess,json,resource
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
OUT=parser.parse_args().output.resolve();OUT.mkdir(parents=True,exist_ok=True)
def body(s,sig):
 start=s.index(sig);a=s.index('{',start);n=1;b=a+1
 while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[start:b]+'\n'
service=(ROOT/'keyboards/ergohaven/sm62/display_input.c').read_text().replace('#include "keyboard.h"','')
painter=(ROOT/'quantum/painter/lvgl/qp_lvgl.c').read_text()
flush=body(painter,'void qp_lvgl_flush(')
qp_flush=body((ROOT/'quantum/painter/qp.c').read_text(),'bool qp_flush(')
# Compile the weak default separately to exercise real weak/strong linkage.
default=OUT/'default_hook.c'
default.write_text(body(painter,'__attribute__((weak)) void qp_lvgl_flush_input_service('))
base='''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {int x1,x2,y1,y2;} lv_area_t;
typedef unsigned lv_color_t;
typedef struct {bool last;} lv_disp_drv_t;
typedef void *painter_device_t;
typedef struct {bool (*flush)(painter_device_t);} painter_vtable_t;
typedef struct {bool validate_ok;const painter_vtable_t *driver_vtable;} painter_driver_t;
static bool driver_flush(painter_device_t d);
static const painter_vtable_t vtable={driver_flush};
static painter_driver_t device={true,&vtable};
static painter_device_t selected_display=&device;
#define qp_dprintf(...) ((void)0)
static unsigned calls,depth,max_depth,frames,ready,flushes;
static bool locked;
void qp_lvgl_flush_input_service(void);
void protocol_keyboard_task(void);
static void keyboard_task(void){assert(!locked);calls++;depth++;if(depth>max_depth)max_depth=depth;assert(depth==1);qp_lvgl_flush_input_service();protocol_keyboard_task();depth--;}
static void qp_viewport(void*d,int a,int b,int c,int e){(void)d;(void)a;(void)b;(void)c;(void)e;assert(!locked);}
static void qp_pixdata(void*d,void*p,uint32_t n){(void)d;(void)p;(void)n;assert(!locked);}
static bool qp_comms_start(painter_device_t d){(void)d;assert(!locked);locked=true;return true;}
static void qp_comms_stop(painter_device_t d){(void)d;assert(locked);locked=false;}
static bool driver_flush(painter_device_t d){(void)d;assert(locked);flushes++;return true;}
static bool lv_disp_flush_is_last(lv_disp_drv_t*d){return d->last;}
static void display_frame_completed(void){assert(!locked);frames++;}
static void lv_disp_flush_ready(lv_disp_drv_t*d){(void)d;ready++;}
'''
main='''int main(int argc,char**argv){(void)argc;unsigned mode=atoi(argv[1]);
if(mode==0){qp_lvgl_flush_input_service();assert(!calls);puts("PASS initialization");return 0;}
protocol_keyboard_task();assert(calls==1 && max_depth==1 && sm62_input_initialized && !sm62_input_running);
puts("PASS ordinary-task + callback and wrapper reentry");
lv_area_t a={0,239,0,3};lv_color_t p[960];lv_disp_drv_t d={0};
unsigned extra=1;
#ifdef SM62_DUALCORE_RENDER
extra=0;
#endif
qp_lvgl_flush(&d,&a,p);assert(calls==1+extra && flushes==1 && !frames && ready==1);
d.last=true;qp_lvgl_flush(&d,&a,p);assert(calls==1+2*extra && flushes==2 && frames==1 && ready==2);
selected_display=0;qp_lvgl_flush(&d,&a,p);assert(calls==1+2*extra && flushes==2 && frames==1 && ready==2);
puts("PASS attached intermediate + final exactly once + detached + unlocked service");return 0;}
'''
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
generic_base=base.replace('qp_lvgl_flush_input_service();protocol_keyboard_task();','')
generic_main='''int main(void){
(void)keyboard_task;lv_area_t a={0,239,0,3};lv_color_t p[960];lv_disp_drv_t d={0};
qp_lvgl_flush(&d,&a,p);assert(!calls && flushes==1 && !frames && ready==1);
d.last=true;qp_lvgl_flush(&d,&a,p);assert(!calls && flushes==2 && frames==1 && ready==2);
puts("PASS weak no-op without board override");return 0;}
'''
results=[]
for name,src,arg,expected in [('normal-init',service,0,0),('normal-lifecycle',service,1,0),('dualcore', '#define SM62_DUALCORE_RENDER\n'+service,1,0),('generic',None,0,0),('mutation-bus',service,1,-6),('mutation-init',service.replace('!sm62_input_initialized || ',''),0,-6),('mutation-reentry',service.replace('    if (sm62_input_running) return;',''),1,-6)]:
 transfer=qp_flush
 if name=='mutation-bus':
  assert transfer.count('    qp_comms_stop(device);')==1
  transfer=transfer.replace('    qp_comms_stop(device);','    qp_lvgl_flush_input_service();\n    qp_comms_stop(device);')
 c=OUT/(name+'.c');c.write_text(generic_base+transfer+flush+generic_main if src is None else base+src+transfer+flush+main);exe=OUT/name
 r=subprocess.run(['gcc','-std=c11','-O1','-Wall','-Wextra','-Werror',str(c),str(default),'-o',str(exe)],capture_output=True,text=True);assert r.returncode==0,r.stderr
 r=subprocess.run([str(exe),str(arg)],capture_output=True,text=True);(OUT/(name+'.log')).write_text(r.stdout+r.stderr)
 assert r.returncode==expected,(name,r.returncode,r.stderr)
 results.append({'name':name,'exit':r.returncode});print(name,r.returncode,r.stdout,r.stderr)
(OUT/'results.json').write_text(json.dumps(results,indent=2))
