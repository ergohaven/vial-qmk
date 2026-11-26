#!/usr/bin/env python3
"""Focused real-LVGL SM62 grid update probe; no device I/O or timing claim.

Uses the production keymap renderer, formatter, QP flush and strip input service.
Matrix/clock/panel and native keymap reads are injected. Factory keycodes are read
from the specified retained ARM ELF. Optional --source freezes changed C owners.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
from macropad_native_support import validate_lvgl

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--elf', type=Path, required=True)
p.add_argument('--source', type=Path, default=ROOT)
p.add_argument('--golden', type=Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
validate_lvgl(ROOT/'lib/lvgl')
def read(path):
    owner = a.source/path
    return (owner if owner.is_file() else ROOT/path).read_text()
def stripped(text):
    return re.sub(r'^\s*#\s*(?:include|pragma once)[^\n]*', '', text, flags=re.M)
def function(text, name):
    m=re.search(r'^(?:__attribute__\(\(weak\)\) )?(?:static )?[^\n;]+\b'+name+r'\([^;\n]*\)\s*\{', text, re.M)
    assert m, name
    end=text.index('{',m.start())+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]+'\n'
tool=Path('/home/agent/projects/dash/.toolchains/qmk-arm-gcc15.2.0-1/bin')
nm=subprocess.check_output([str(tool/'arm-none-eabi-nm'),'-S','--defined-only',str(a.elf)],text=True)
symbol=next(l.split() for l in nm.splitlines() if l.split()[-1]=='keymaps')
raw=subprocess.check_output([str(tool/'arm-none-eabi-objcopy'),'-O','binary',str(a.elf),'/dev/stdout'])
address,size=int(symbol[0],16)-0x10000000,int(symbol[1],16)
assert size%(10*7*2)==0 and address>=0
codes=struct.unpack('<'+'H'*(size//2),raw[address:address+size])
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lvgl.h"
#include "quantum/quantum_keycodes.h"
#include "quantum/modifiers.h"
#include "src/display/eh_symbols.h"
#include "src/display/application_icons.h"
#include "keyboards/ergohaven/sm62/application_geometry.h"
#define EH_APP_LAYOUT_ENABLE
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define SM62_EFFECT 0
#define SM62_FAST_RETURN 1
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
typedef struct {void(*init)(void),(*load)(void),(*housekeep)(void);} eh_screen_t;
typedef void *painter_device_t;
static painter_device_t selected_display=(void*)1;
static bool left=true,matrix[10][7];
static uint32_t now=100;
static struct {uint8_t mode,cyrillic_enabled;} settings={3,1};
static unsigned flushes,pixels,input_scans,frames;
static lv_area_t area,trace[512];
static uint16_t panel[240*280];
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static bool is_keyboard_left(void){return left;}
static uint8_t get_current_layer(void){return 0;}
static bool matrix_is_on(uint8_t r,uint8_t c){return matrix[r][c];}
static const typeof(settings)*sm62_local_display_settings(void){return &settings;}
static uint32_t sm62_local_keymap_hold_ms(void){return 350;}
static uint32_t hid_app_layout_visual_generation(void){return 0;}
static bool hid_app_layout_get_visual(uint8_t l,uint8_t c,uint8_t*v,uint8_t*s){(void)l;(void)c;(void)v;(void)s;return false;}
static lv_obj_t*display_view_create(void){return lv_obj_create(NULL);}
static void display_show_view(lv_obj_t*v){lv_scr_load(v);}
static void keyboard_task(void){input_scans++;}
static void display_frame_completed(void){frames++;}
static bool qp_viewport(painter_device_t d,uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2){(void)d;area=(lv_area_t){x1,y1,x2,y2};return true;}
static bool qp_pixdata(painter_device_t d,const void*data,uint32_t n){
 (void)d;const lv_color_t *colors=data;unsigned i=0;
 for(int y=area.y1;y<=area.y2;y++)for(int x=area.x1;x<=area.x2;x++)panel[y*240+x]=colors[i++].full;
 assert(i==n);pixels+=n;return true;
}
static bool qp_flush(painter_device_t d){(void)d;assert(flushes<ARRAY_SIZE(trace));trace[flushes++]=area;return true;}
'''
pre += 'static const uint16_t native_codes[]={' + ','.join(map(str,codes)) + '};\n'
pre += r'''
static uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c){unsigned i=(l*10+r)*7+c;return i<ARRAY_SIZE(native_codes)?native_codes[i]:KC_NO;}
static uint16_t sm62_effective_keycode(uint8_t l,uint8_t r,uint8_t c){return hid_sm62_default_keycode(l,r,c);}
'''
for path,start in [('keyboards/ergohaven/ergohaven.h','enum custom_keycodes'),('keyboards/ergohaven/src/eh_ruen.h','enum ruen_custom_keycodes'),('keyboards/ergohaven/src/eh_pointing.h','enum {\n    EH_L_SNP')]:
    text=read(path);pos=text.index(start);pre+=text[pos:text.index('};',pos)+2]+'\n'
formatter=stripped(read('keyboards/ergohaven/src/display/eh_keycode_str.c'))
# These are declared by the formatter's owning header in the normal build.
pre+='void get_keycode_str(char*,uint16_t);\nconst char*keycode_to_str(uint16_t);\nbool eh_keycode_str_uses_mac_modifiers(void);\n'
qp=function(read('quantum/painter/lvgl/qp_lvgl.c'),'qp_lvgl_flush')
input_source=stripped(read('keyboards/ergohaven/sm62/display_input.c'))
keymap=stripped(read('keyboards/ergohaven/sm62/screen_keymap.c'))
body=r'''
static void render(void){lv_tick_inc(32);lv_refr_now(NULL);assert(input_scans==flushes);}
static void reset_trace(void){flushes=pixels=input_scans=frames=0;memset(trace,0,sizeof(trace));}
static void press(uint8_t row,uint8_t col){uint8_t gr=left?row:row+5;matrix[gr][col]=true;display_process_keyevent(gr,col,true);sm62_keymap_observe_matrix();screen_keymap_housekeep();render();}
static void settle(void){memset(matrix,0,sizeof(matrix));now+=3000;sm62_keymap_observe_matrix();screen_keymap_housekeep();render();}
static void save(const char *label,const char *out){
 char path[2048];snprintf(path,sizeof(path),"%s/%s-%u.bin",out,label,left);
 FILE*f=fopen(path,"wb");assert(f);assert(fwrite(panel,1,sizeof(panel),f)==sizeof(panel));fclose(f);
 printf("CASE {\"label\":\"%s\",\"left\":%s,\"flushes\":%u,\"pixels\":%u,\"input_scans\":%u,\"frames\":%u,\"rectangles\":[",label,left?"true":"false",flushes,pixels,input_scans,frames);
 for(unsigned i=0;i<flushes;i++)printf("%s[%d,%d,%d,%d]",i?",":"",trace[i].x1,trace[i].y1,trace[i].x2,trace[i].y2);
 puts("]}");
}
int main(int argc,char **argv){
 assert(argc==3);left=atoi(argv[1]);protocol_keyboard_task();input_scans=0;
 lv_init();static lv_disp_draw_buf_t buf;static lv_color_t colors[240*28];lv_disp_draw_buf_init(&buf,colors,NULL,ARRAY_SIZE(colors));
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.draw_buf=&buf;drv.flush_cb=qp_lvgl_flush;lv_disp_drv_register(&drv);
 eh_screen_keymap.init();eh_screen_keymap.load();render();settle();
 reset_trace();press(2,2);save("middle",argv[2]);settle();
 reset_trace();press(2,1);save("inside",argv[2]);settle();
 reset_trace();press(0,6);save("outer-jump",argv[2]);settle();
 reset_trace();press(4,0);save("thumb-jump",argv[2]);
}
'''
unit=pre+formatter+input_source+qp+keymap+body
src=a.output/'grid.c';src.write_text(unit)
exe=Path(os.environ['AGENT_BUILD_CACHE'])/'sm62-grid-flush-probe'
fonts=[ROOT/f'keyboards/ergohaven/src/display/fonts/eh_font_iosevka_{n}.c' for n in [18,28]]
fonts += [ROOT/f'keyboards/ergohaven/macropad/sm62_font_zen_medium_{n}.c' for n in [16,17,20,28]]
command=['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-type-limits','-DLV_CONF_SKIP','-DLV_MEM_CUSTOM=1','-DLV_COLOR_DEPTH=16','-DLV_COLOR_16_SWAP=1','-DLV_USE_FONT_COMPRESSED=1','-DLV_USE_LOG=0',*['-I'+str(d) for d in [ROOT,ROOT/'lib/lvgl',ROOT/'quantum',ROOT/'quantum/keymap_extras',ROOT/'quantum/sequencer',ROOT/'keyboards/ergohaven']],str(src),*map(str,fonts),*sorted(map(str,(ROOT/'lib/lvgl/src').rglob('*.c'))),'-lm','-o',str(exe)]
compiled=subprocess.run(command,text=True,capture_output=True);(a.output/'build.log').write_text(compiled.stdout+compiled.stderr)
result={'command':command,'build_exit':compiled.returncode,'fixture_sha256':hashlib.sha256(unit.encode()).hexdigest(),'baseline_elf':str(a.elf),'baseline_elf_sha256':hashlib.sha256(a.elf.read_bytes()).hexdigest(),'physical_timing_or_tearing_proven':False,'cases':[]}
if compiled.returncode:
    print(compiled.stdout+compiled.stderr)
else:
    for left in [1,0]:
        run=subprocess.run([str(exe),str(left),str(a.output)],text=True,capture_output=True)
        (a.output/f'run-{left}.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr)
        assert run.returncode==0,run.returncode
        for line in run.stdout.splitlines():
            if line.startswith('CASE '):
                c=json.loads(line[5:]);panel_path=a.output/f"{c['label']}-{int(c['left'])}.bin"
                c['panel_sha256']=hashlib.sha256(panel_path.read_bytes()).hexdigest();result['cases'].append(c)
    assert len(result['cases'])==8
    if a.golden:
        golden=json.loads((a.golden/'result.json').read_text());old={(c['label'],c['left']):c for c in golden['cases']}
        for c in result['cases']:
            prior=old[(c['label'],c['left'])]
            assert c['panel_sha256']==prior['panel_sha256'],('changed rendered result',c['label'],c['left'])
            assert c['input_scans']==c['flushes'] and c['frames']==1
            assert c['pixels']<=prior['pixels'],('extra pixel work',c['label'],c['left'])
            if c['label'] in ['outer-jump','thumb-jump']:
                assert c['pixels']<prior['pixels'],('large jump still repaints unchanged screen margins',c['label'],c['left'])
        result['golden_panels_unchanged']=True
(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
raise SystemExit(bool(compiled.returncode))
