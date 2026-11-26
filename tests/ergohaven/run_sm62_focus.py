#!/usr/bin/env python3
"""Focused SM62 viewport/press/layer regression; no LVGL or device I/O.

Extracts the actual screen geometry, focus, matrix/captured-event and housekeeping
owners. Only the matrix, clock, effective keycodes and paint sink are injected.
Run through agent-storage: python3 -B tests/ergohaven/run_sm62_focus.py OUTPUT
Optional --source reads a frozen owner tree for an unchanged negative control.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--source', type=Path, default=ROOT)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
source = (args.source / 'keyboards/ergohaven/sm62/screen_keymap.c').read_text()

def function(name):
    match = re.search(r'^(?:static )?[^\n;]+\b' + name + r'\([^;\n]*\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'

prefix = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quantum/quantum_keycodes.h"
#define SM62_EFFECT 0
#define SM62_FAST_RETURN 1
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
static bool left = true, matrix[10][7];
static uint8_t layer;
static uint16_t keycodes[16][10][7];
static uint32_t now = 100;
static uint16_t hold_ms = 200;
static struct { uint8_t mode, cyrillic_enabled; } settings;
static struct { bool icon_running; } keys[9];
static unsigned paints;
static bool is_keyboard_left(void) { return left; }
static uint8_t get_current_layer(void) { return layer; }
static bool matrix_is_on(uint8_t r,uint8_t c) { return matrix[r][c]; }
static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t t) { return now-t; }
static const typeof(settings) *sm62_local_display_settings(void) { return &settings; }
static uint32_t sm62_local_keymap_hold_ms(void) { return hold_ms; }
static uint32_t hid_app_layout_visual_generation(void) { return 0; }
static uint16_t sm62_effective_keycode(uint8_t l,uint8_t r,uint8_t c) { return keycodes[l][r][c]; }
static void refresh_grid(bool side,uint8_t l,bool content) { (void)side;(void)l;(void)content;paints++; }
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d %s (view %u,%u)\n",__func__,__LINE__,#c,first_row,first_visual_col);exit(1); } } while(0)
'''
globals_source = source[source.index('static bool old_pressed'):source.index('#if SM62_EFFECT > 0', source.index('static bool old_pressed'))]
timing_source = source[source.index('#define KEYMAP_RELEASE_FADE_MS'):source.index('#define KEYMAP_GRID_LINE_SIZE')]
visual_type = source[source.index('typedef enum {'):source.index('} key_visual_state_t;') + len('} key_visual_state_t;')]
view_duration = function('keymap_release_view_ms') if re.search(r'\bkeymap_release_view_ms\([^;]*\)\s*\{', source) else ''
geometry = ''.join(function(name) for name in [
    'matrix_col_from_visual', 'visual_col_from_matrix', 'visual_row_from_matrix',
    'matrix_row_from_visual', 'global_row', 'physical_key_exists', 'visual_key_exists',
    'keycode_is_layer_switch', 'reset_focus', 'focus_key',
    'keymap_release_fade_ms']) + view_duration + ''.join(function(name) for name in [
    'keymap_release_pending', 'key_visual_state', 'sm62_keymap_visual_active'])
input_source = source[source.index('/* Main-loop capture only'):source.index('#if defined(SM62_DUALCORE_RENDER)\nvoid sm62_keymap_owner_feedback')]
tests = r'''
static void view(uint8_t r,uint8_t c) { CHECK(first_row==r && first_visual_col==c); }
static void press(uint8_t r,uint8_t c,bool captured) {
 uint8_t gr=global_row(left,r);matrix[gr][c]=true;
 if(captured) display_process_keyevent(gr,c,true);
 sm62_keymap_observe_matrix();screen_keymap_housekeep();now++;
 matrix[gr][c]=false;if(captured)display_process_keyevent(gr,c,false);
 sm62_keymap_observe_matrix();screen_keymap_housekeep();now++;
}
static void init(bool side) {
 left=side;previous_side_left=side;reset_focus(side);
 screen_keymap_housekeep();
}
static void sticky(void) {
 init(true);press(2,2,true);view(1,3); // WER / SDF / XCV, D centered.
 press(2,1,true);view(1,3); // F changes only highlight.
 press(2,0,true);view(1,4); // G scrolls one column, stays at right edge.
 press(2,1,true);view(1,4);press(2,2,true);view(1,4);
 press(2,3,true);view(1,3); // S scrolls one column back, at left edge.
 press(0,2,true);view(0,3); // up: selected key stays top.
 press(3,2,true);view(1,3); // down: selected key stays bottom.
 press(4,2,true);view(2,3); // thumb row, bottom edge.
 CHECK(paints>0);puts("D-F-G, reverse, up/down and thumb minimal panning: PASS");
}
static void within(bool side) {
 init(side);press(2,2,false);
 uint8_t fr=first_row,fc=first_visual_col;
 for(uint8_t r=fr;r<fr+3;r++)for(uint8_t c=fc;c<fc+3;c++) {
  uint8_t mc=matrix_col_from_visual(side,c),mr=matrix_row_from_visual(side,r,mc);
  if(physical_key_exists(side,mr,mc)) {press(mr,mc,true);view(fr,fc);}
 }
 puts("every visible key leaves viewport unchanged, scan+capture: PASS");
}
static void after_layer(bool simultaneous,bool quick) {
 init(left);press(0,2,true);CHECK(first_row==0);
 if(simultaneous) {
  layer=1;uint8_t gr=global_row(left,1);matrix[gr][2]=true;
  display_process_keyevent(gr,2,true);sm62_keymap_observe_matrix();screen_keymap_housekeep();
 } else {
  layer=1;screen_keymap_housekeep();
  if(quick) {
   display_process_keyevent(global_row(left,1),2,true);now++;
   display_process_keyevent(global_row(left,1),2,false);
   sm62_keymap_observe_matrix();screen_keymap_housekeep();
  } else press(1,2,true);
 }
 CHECK(first_row==1);CHECK(!focus_next_press_after_layer_change);
 puts("first QWERT-row press after layer is top, including same-scan/short tap: PASS");
}
static void layer_switch_first(void) {
 init(left);press(0,2,true);layer=1;screen_keymap_housekeep();
 keycodes[1][global_row(left,4)][1]=QK_MOMENTARY+2;
 press(4,1,true);CHECK(focus_next_press_after_layer_change);
 press(1,2,true);CHECK(first_row==1 && !focus_next_press_after_layer_change);
 puts("layer control does not consume next ordinary-key anchor: PASS");
}
static void jumps(bool side) {
 init(side);press(2,2,true);
 // Jump between both physical sides, number-row corners and short thumb row.
 press(0,6,true);view(1,side?0:4);
 press(0,0,true);view(0,side?4:0);
 press(4,3,true);CHECK(first_row==2);
 press(0,6,true);view(1,side?0:4);
 CHECK(visual_key_exists(side,first_row,matrix_col_from_visual(side,first_visual_col)) ||
       visual_key_exists(side,first_row+1,matrix_col_from_visual(side,first_visual_col)));
 puts("jump-to-corner/outer keys anchored inward on both halves: PASS");
}
static void geometry(bool side) {
 init(side);
 unsigned count=0;
 for(uint8_t r=0;r<5;r++)for(uint8_t c=0;c<7;c++) {
  if(!physical_key_exists(side,r,c))continue;
  reset_focus(side);focus_key(side,r,c);
  uint8_t vr=visual_row_from_matrix(side,r,c),vc=visual_col_from_matrix(side,c);
  CHECK(first_row<=2 && first_visual_col<=4);
  CHECK(vr>=first_row && vr<=first_row+2 && vc>=first_visual_col && vc<=first_visual_col+2);
  uint8_t fr=first_row,fc=first_visual_col;focus_key(side,r,c);view(fr,fc);count++;
 }
 CHECK(count==31);puts("all 31 physical positions including shifted side keys fit stable 3x3: PASS");
}
static void snappy_fade(void) {
 const uint16_t holds[]={0,200,350,400,401,1000,10000};
 const uint8_t ages[]={0,10,25,40,49};
 const uint8_t mixes[]={255,163,63,10,0};
 for(unsigned mode=0;mode<4;mode++)for(unsigned h=0;h<ARRAY_SIZE(holds);h++) {
  settings.mode=mode;hold_ms=holds[h];CHECK(keymap_release_fade_ms()==50);
  old_pressed[2][2]=true;uint8_t mix=99;
  CHECK(key_visual_state(2,2,&mix)==KEY_VISUAL_PRESSED && mix==0);
  old_pressed[2][2]=false;released_fading[2][2]=true;released_at[2][2]=now;
  uint32_t start=now;
  for(unsigned a=0;a<ARRAY_SIZE(ages);a++) {
   now=start+ages[a];mix=99;
   CHECK(key_visual_state(2,2,&mix)==KEY_VISUAL_RELEASED && mix==mixes[a]);
  }
  now=start+50;mix=99;CHECK(key_visual_state(2,2,&mix)==KEY_VISUAL_REST && mix==0);
 }
 puts("instant pressed state, fixed 50ms quadratic release, independent mode/hold: PASS");
}
static void unchanged_view_lease(void) {
 const uint16_t holds[]={0,200,350,400,401,1000,10000};
 const uint16_t leases[]={166,166,166,166,166,310,2470};
 for(unsigned mode=0;mode<4;mode++)for(unsigned h=0;h<ARRAY_SIZE(holds);h++) {
  settings.mode=mode;hold_ms=holds[h];memset(released_fading,0,sizeof(released_fading));
  released_fading[2][2]=true;released_at[2][2]=now;
  uint32_t start=now;uint16_t lease=mode==3?166:leases[h];
  now=start+lease-1;CHECK(sm62_keymap_visual_active());
  now=start+lease;CHECK(!sm62_keymap_visual_active());
 }
 puts("original return/view lease at every boundary, all modes and saved holds: PASS");
}
int main(int argc,char **argv) {
 CHECK(argc==2);unsigned n=atoi(argv[1]);left=(n&1)==0;
 switch(n){case 0:sticky();break;case 1:case 2:within(left);break;
 case 3:case 4:after_layer(false,false);break;case 5:case 6:after_layer(true,false);break;
 case 7:case 8:after_layer(false,true);break;case 9:case 10:layer_switch_first();break;
 case 11:case 12:jumps(left);break;case 13:case 14:geometry(left);break;
 case 15:snappy_fade();break;case 16:unchanged_view_lease();break;default:return 2;}
}
'''
translation_unit = prefix + timing_source + visual_type + globals_source + geometry + input_source + function('screen_keymap_housekeep') + tests
(args.output/'focus.c').write_text(translation_unit)
exe = Path(os.environ['AGENT_BUILD_CACHE'])/'sm62-focus-probe'
command = ['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'quantum'),'-I'+str(ROOT/'quantum/keymap_extras'),'-I'+str(ROOT/'quantum/sequencer'),str(args.output/'focus.c'),'-o',str(exe)]
compiled = subprocess.run(command, text=True, capture_output=True)
(args.output/'build.log').write_text(compiled.stdout+compiled.stderr)
result = {'command':command,'build_exit':compiled.returncode,'owner_sha256':hashlib.sha256(source.encode()).hexdigest(),'fixture_sha256':hashlib.sha256(translation_unit.encode()).hexdigest(),'cases':[]}
if compiled.returncode:
    print(compiled.stdout+compiled.stderr)
else:
    logs=[]
    for case in range(17):
        run=subprocess.run([str(exe),str(case)],text=True,capture_output=True)
        result['cases'].append({'id':case,'exit':run.returncode})
        logs.append(f'case {case}: exit {run.returncode}\n'+run.stdout+run.stderr)
    (args.output/'run.log').write_text('\n'.join(logs))
    print('\n'.join(logs))
(args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
raise SystemExit(bool(compiled.returncode or any(c['exit'] for c in result['cases'])))
