#!/usr/bin/env python3
"""Actual split dispatcher/hooks/capture; cooked debounce output and I/O modeled.
No direct hook calls, LVGL execution or host-HID/physical correctness claim.
"""
from pathlib import Path
import argparse
import subprocess
import resource
import json

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, default=ROOT.parent / 'regressions/peer-capture-dispatch')
OUT = parser.parse_args().out
OUT.mkdir(parents=True, exist_ok=True)


def body(s, sig):
    start = s.index(sig)
    a = s.index('{', start)
    n, b = 1, a + 1
    while n:
        n += (s[b] == '{') - (s[b] == '}')
        b += 1
    return s[start:b] + '\n'


screen = (ROOT / 'keyboards/ergohaven/sm62/screen_keymap.c').read_text()
capture = screen[screen.index('static struct {\n    bool pending, released;'):screen.index('static bool scan_keys(')]
common = (ROOT / 'quantum/matrix_common.c').read_text()
post = body(common, 'bool matrix_post_scan(')
getter = body(common, 'inline matrix_row_t matrix_get_row(').replace('inline ', '')
weak = body(common, '__attribute__((weak)) void matrix_slave_scan_kb(')
current = (ROOT / 'keyboards/ergohaven/ergohaven_main.c').read_text()
before = (ROOT.parent / 'review-fix-before/keyboards/ergohaven/ergohaven_main.c').read_text()
baseline = (ROOT.parent.parent / 'sm62-test14-display-cooperative/firmware/keyboards/ergohaven/ergohaven_main.c').read_text()
head = '''#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define ROWS_PER_HAND (MATRIX_ROWS / 2)
#define SPLIT_KEYBOARD
#define EH_HAS_DISPLAY
#define KEYBOARD_ergohaven_sm62
#define KC_LGUI 0
#define KC_LALT 1
typedef uint8_t matrix_row_t;
static matrix_row_t matrix[MATRIX_ROWS];
static bool left,master,is_alt_tab_active;
static uint8_t thisHand,thatHand,layer;
static uint16_t alt_tab_timer;
static uint32_t now;
static unsigned kb_calls,slave_calls,wake_calls,master_transport,slave_transport;
static struct {bool swap_lctl_lgui;} keymap_config;
static bool is_keyboard_left(void){return left;}
static bool is_keyboard_master(void){return master;}
static uint8_t get_current_layer(void){return layer;}
static uint32_t timer_read32(void){return now;}
static uint16_t timer_elapsed(uint16_t t){return now-t;}
static void unregister_code(uint8_t k){(void)k;}
static void display_process_matrix_press(void){wake_calls++;}
static void matrix_scan_user(void){kb_calls++;}
static void matrix_slave_scan_user(void){slave_calls++;}
static bool transport_master_if_connected(matrix_row_t *a,matrix_row_t *b){
assert(a==matrix+thisHand);(void)b;master_transport++;return false;}
static void transport_slave(matrix_row_t *a,matrix_row_t *b){
assert(a==matrix+thatHand && b==matrix+thisHand);slave_transport++;}
'''
main = '''int main(int argc,char **argv){assert(argc==3);
left=atoi(argv[1]);master=atoi(argv[2]);
/* Production matrix_init offsets; debounce writes cooked rows at thisHand. */
thisHand=left?0:ROWS_PER_HAND;thatHand=ROWS_PER_HAND-thisHand;
unsigned row=thisHand+1,remote=thatHand+1;
layer=2;now=100;matrix[row]=4;matrix_post_scan();
if(!master){assert(captured_press.pending && captured_press.row==1 && captured_press.col==2 && captured_press.layer==2 && captured_press.pressed_at==100);}
else assert(!captured_press.pending);
layer=3;now=110;matrix[row]=12;matrix_post_scan();
if(!master){assert(captured_press.col==3 && captured_press.layer==3 && captured_press.pressed_at==110);}
layer=4;now=120;matrix[row]=0;matrix_post_scan();
if(!master){assert(captured_press.released && captured_press.released_at==120 && captured_press.layer==3);}
else assert(!captured_press.pending);
/* No display observation has run: the brief tap must still be retained.
 * Transported/nonlocal rows must not replace the local capture. */
now=125;matrix[remote]=32;matrix_post_scan();matrix[remote]=0;matrix_post_scan();
if(!master){assert(captured_press.col==3 && captured_press.released_at==120);assert(slave_calls==5 && kb_calls==0 && slave_transport==5 && master_transport==0 && wake_calls==0);}
else {assert(kb_calls==5 && slave_calls==0 && master_transport==5 && slave_transport==0 && wake_calls==3);}
printf("PASS left=%u master=%u capture=%u released=%u kb_user=%u slave_user=%u\\n",left,master,captured_press.pending,captured_press.released,kb_calls,slave_calls);
return 0;}
'''


def hooks(source):
    kb = body(source, 'void matrix_scan_kb(')
    slave = body(source, 'void matrix_slave_scan_kb(') if 'void matrix_slave_scan_kb(' in source else weak
    return kb + slave


resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
variants = [('current', hooks(current), False),
            ('baseline-unreachable', hooks(before), True),
            ('baseline-peer-missing', hooks(baseline), True)]
if 'void matrix_slave_scan_kb(' in current:
    variants += [
        ('mutation-slave-hook-missing', body(current, 'void matrix_scan_kb(') + weak, True),
        ('mutation-peer-release', hooks(current).replace('current ^ previous_display_matrix[row]', 'current & ~previous_display_matrix[row]'), True),
        ('mutation-slave-user-missing', hooks(current).replace('matrix_slave_scan_user();', ''), True),
    ]
results = []
for name, source, peripheral_failure in variants:
    c = OUT / (name + '.c')
    c.write_text(head + getter + capture + source + post + main)
    exe = OUT / name
    build = subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', str(c), '-o', str(exe)], capture_output=True, text=True)
    (OUT / (name + '-build.log')).write_text(build.stdout + build.stderr)
    assert build.returncode == 0, build.stderr
    for left in (0, 1):
        for master in (0, 1):
            run = subprocess.run([str(exe), str(left), str(master)], capture_output=True, text=True)
            expected = -6 if peripheral_failure and not master else 0
            log = f'{name}-left{left}-master{master}.log'
            (OUT / log).write_text(run.stdout + run.stderr)
            result = dict(name=name, left=left, master=master, build_exit=build.returncode, exit=run.returncode, expected=expected, passed=run.returncode == expected, log=log)
            results.append(result)
            print(json.dumps(result), run.stdout.strip(), run.stderr.strip())
(OUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
assert all(r['passed'] for r in results), 'production split-dispatch regression failed'
print(f'PASS {len(results)} split-dispatch scenarios (including expected mutation failures)')
