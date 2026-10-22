#!/usr/bin/env python3
"""Source-backed native combo regression. Run: python3 tests/ergohaven_app_combo.py

Compiles the complete HID app-layout implementation, verbatim keyboard pre-process
hook, and complete QMK combo engine/header. Only platform/peripheral boundaries
are stubbed. Generated sources/binaries stay in TMPDIR, not the repository.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
import argparse
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-ref', help='Read production source from a git ref (e.g. HEAD) without modifying files')
args = parser.parse_args()

def read_source(path):
    if args.source_ref:
        return subprocess.check_output(['git','show',f'{args.source_ref}:{path}'],cwd=ROOT,universal_newlines=True)
    return (ROOT / path).read_text()

def function(path, name):
    source = read_source(path)
    match = re.search(r"^(?:static )?[\w* ]+\b" + name + r"\([^;\n]*\)\s*\{", source, re.M)
    assert match, name
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]

def without_includes(source):
    return re.sub(r'^\s*#\s*include[^\n]*', '', source, flags=re.M)

preamble = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EH_HAS_DISPLAY
#define EH_APP_LAYOUT_ENABLE
#define COMBO_ENABLE
#define NO_ACTION_TAPPING
#define MATRIX_ROWS 5
#define MATRIX_COLS 3
#define TAPPING_TERM 200
#define TAP_CODE_DELAY 0
#define KC_NO 0
#define KC_TRNS 1
#define KC_A 4
#define KC_B 5
#define KC_7 36
#define KC_8 37
#define QK_COMBO_ON 0x7c50
#define QK_COMBO_OFF 0x7c51
#define QK_COMBO_TOGGLE 0x7c52
#define MO(n) (0x5220 + (n))
typedef struct { uint8_t row, col; } keypos_t;
typedef struct { keypos_t key; bool pressed; uint8_t type; uint16_t time; } keyevent_t;
typedef struct { keyevent_t event; uint16_t keycode; } keyrecord_t;
enum { TICK_EVENT, KEY_EVENT, COMBO_EVENT, ENCODER_EVENT };
#define IS_KEYEVENT(e) ((e).type == KEY_EVENT)
#define IS_NOEVENT(e) ((e).type == TICK_EVENT)
#define MAKE_KEYPOS(r,c) ((keypos_t){(r),(c)})
#define MAKE_COMBOEVENT(p) ((keyevent_t){.type=COMBO_EVENT,.pressed=(p)})
static uint32_t now = 100;
static uint32_t layer_state = 1, default_layer_state = 1;
uint32_t timer_read32(void) { return now; }
uint32_t timer_elapsed32(uint32_t t) { return now-t; }
uint16_t timer_read(void) { return now; }
uint16_t timer_elapsed(uint16_t t) { return (uint16_t)(now-t); }
uint8_t get_highest_layer(uint32_t state) { uint8_t n=0; while (state>>1) { state>>=1; n++; } return n; }
void layer_clear(void) { layer_state=1; }
void vial_keycode_up(uint16_t k) { (void)k; }
void vial_keycode_down(uint16_t k) { (void)k; }
void vial_keycode_tap(uint16_t k) { (void)k; }
void display_process_keyevent(uint8_t r,uint8_t c,bool p) { (void)r;(void)c;(void)p; }
static unsigned ruen_calls, user_calls;
bool pre_process_record_ruen(uint16_t k,keyrecord_t *r) { (void)k;(void)r;ruen_calls++;return true; }
bool pre_process_record_user(uint16_t k,keyrecord_t *r) { (void)k;(void)r;user_calls++;return true; }
void clear_weak_mods(void) {}
static uint16_t fallback_first=KC_7, fallback_second=KC_8;
uint16_t keymap_key_to_keycode(uint8_t layer,keypos_t pos) { (void)layer;return pos.col == 0 ? fallback_first : fallback_second; }
void process_record(keyrecord_t *record);
#define pgm_read_word(p) (*(const uint16_t *)(p))
'''

hid = read_source('keyboards/ergohaven/hid.c')
# Skip feature-gated includes, but keep the complete implementation block and hooks.
hid_start = hid.index('#ifdef EH_APP_LAYOUT_ENABLE', hid.index('static hid_data_t hid_data;'))
hid = hid[hid_start:hid.index('\nhid_data_t *get_hid_data')]
combo_header = without_includes((ROOT / 'quantum/process_keycode/process_combo.h').read_text()).replace('#pragma once', '')
combo = without_includes((ROOT / 'quantum/process_keycode/process_combo.c').read_text())
fixtures = r'''
static const uint16_t chord[] = {KC_7,KC_8,COMBO_END};
static combo_t combos[] = {COMBO(chord,MO(1))};
uint16_t combo_count(void) { return 1; }
combo_t *combo_get(uint16_t index) { return &combos[index]; }
static unsigned mo_down, mo_up, normal_down, normal_up;
void process_record(keyrecord_t *record) {
    if (record->event.type == COMBO_EVENT && record->keycode == MO(1)) {
        if (record->event.pressed) { mo_down++;layer_state |= 2; }
        else { mo_up++;layer_state &= ~2U; }
    } else if (record->event.pressed) normal_down++;
    else normal_up++;
}
'''
tests = r'''
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n", __func__,__LINE__,#c); exit(1); } } while (0)
static uint16_t event(uint8_t row,uint8_t col,bool pressed) {
    keyrecord_t record = {.event={.key={row,col},.pressed=pressed,.type=KEY_EVENT,.time=now}};
    uint16_t fallback = keymap_key_to_keycode(0,record.event.key);
    CHECK(pre_process_record_kb(fallback,&record));
    // QMK get_record_keycode's explicit-keycode override, including KC_NO sentinel.
    uint16_t effective = record.keycode ? record.keycode : fallback;
    if (process_combo(effective,&record)) process_record(&record);
    now++;
    return effective;
}
static void layout(void) {
    memset(app_layout_keycodes,0,sizeof(app_layout_keycodes));
    app_layout_active=true;app_layout_sync_time=now;layer_state=1;
    app_layout_keycodes[0][0]=KC_7;app_layout_keycodes[0][1]=KC_8;
    app_layout_keycodes[1][0]=KC_A;app_layout_keycodes[1][1]=KC_B;
}
static void activate(void) {
    CHECK(event(1,0,true)==KC_7);CHECK(event(1,1,true)==KC_8);
    now+=COMBO_TERM+1;combo_task();
    CHECK(combos[0].active);CHECK(layer_state==3);
}
static void momentary_combo_release(void) {
    layout();activate();
    uint16_t first=event(1,0,false), second=event(1,1,false);
    printf("release trace: keycodes=%u/%u combo_active=%u combo_state=%u layer_state=%u MO_down/up=%u/%u\n",first,second,combos[0].active,combos[0].state,(unsigned)layer_state,mo_down,mo_up);
    fflush(stdout);
    CHECK(!combos[0].active);CHECK(combos[0].state==0);CHECK(layer_state==1);
    CHECK(first==KC_7 && second==KC_8);
    CHECK(mo_down==1 && mo_up==1);CHECK(normal_down==0 && normal_up==0);
    // A fresh press must use L1, rather than a stale L0 cache.
    layer_state=3;CHECK(event(1,0,true)==KC_A);CHECK(event(1,0,false)==KC_A);
    puts("momentary_combo_release: PASS (L0 7/8 -> MO(1), L1 A/B, release after COMBO_TERM)");
}
static void no_key_sentinel(void) {
    layout();app_layout_keycodes[0][0]=KC_NO;
    CHECK(event(1,0,true)==UINT16_MAX);
    app_layout_keycodes[0][0]=KC_7;
    CHECK(event(1,0,false)==UINT16_MAX);
    CHECK(combos[0].state==0);CHECK(mo_down==0);
    puts("no_key_sentinel: PASS");
}
static void transparent_binding(void) {
    layout();layer_state=3;app_layout_keycodes[1][0]=KC_TRNS;
    CHECK(event(1,0,true)==KC_7);
    layer_state=1;app_layout_keycodes[0][0]=KC_A;
    CHECK(event(1,0,false)==KC_7);CHECK(combos[0].state==0);
    puts("transparent_binding: PASS");
}
static void commit_layout(bool active) {
    // Stage a complete valid snapshot, then exercise the real COMMIT handler.
    memset(&app_layout_staging,0,sizeof(app_layout_staging));
    app_layout_staging.valid=true;app_layout_staging.active=active;
    app_layout_staging.chunks=EH_APP_LAYOUT_ALL_CHUNKS;
    app_layout_staging.layer_name_chunks=UINT16_MAX;
    app_layout_staging.visual_chunks=UINT16_MAX;
    memset(app_layout_staging.stack_chunks,0x0f,sizeof(app_layout_staging.stack_chunks));
    app_layout_staging.keycodes[0][0]=KC_A;app_layout_staging.keycodes[0][1]=KC_B;
    uint16_t crc=app_layout_crc16(&app_layout_staging);
    uint8_t packet[9]={EH_APP_LAYOUT_COMMIT,EH_APP_LAYOUT_PROTOCOL_VERSION,active,0,0,0,0,crc,crc>>8};
    CHECK(hid_app_layout_process_packet(packet,sizeof(packet)));
    CHECK(!app_layout_staging.valid);CHECK(app_layout_active==active);
}
static void session_change(unsigned mode) {
    // The underlying Vial keymap deliberately differs from Entropy.
    fallback_first=KC_A;fallback_second=KC_B;
    layout();activate();
    if (mode==0) commit_layout(true);
    if (mode==1) {
        uint8_t packet[]={EH_APP_LAYOUT_DEACTIVATE,EH_APP_LAYOUT_PROTOCOL_VERSION};
        CHECK(hid_app_layout_process_packet(packet,sizeof(packet)));
    }
    if (mode==2) { now+=EH_APP_LAYOUT_TIMEOUT_MS;hid_app_layout_task();CHECK(!app_layout_active); }
    if (mode==3) commit_layout(false);
    CHECK(event(1,0,false)==KC_7);CHECK(event(1,1,false)==KC_8);
    CHECK(!combos[0].active);CHECK(combos[0].state==0);CHECK(mo_down==1 && mo_up==1);
    puts("session_change: PASS");
}
static void fallback_press(unsigned mode) {
    layout();
    if (mode==0) app_layout_active=false;
    else app_layout_keycodes[0][0]=EH_APP_LAYOUT_UNSET_KEYCODE;
    CHECK(event(1,0,true)==KC_7);
    CHECK(ruen_calls==1 && user_calls==1);
    app_layout_active=true;app_layout_keycodes[0][0]=KC_A;
    CHECK(event(1,0,false)==KC_7);
    CHECK(ruen_calls==2 && user_calls==2);CHECK(combos[0].state==0);
    puts("fallback_press: PASS");
}
static void normal_key(void) {
    layout();app_layout_keycodes[0][0]=KC_A;
    CHECK(event(1,0,true)==KC_A);layer_state=3;
    CHECK(event(1,0,false)==KC_A);
    CHECK(normal_down==1 && normal_up==1);CHECK(!combos[0].active);
    puts("normal_key: PASS");
}
static void rejected_combo(void) {
    layout();CHECK(event(1,0,true)==KC_7);
    now+=COMBO_TERM+1;combo_task();CHECK(normal_down==1);
    layer_state=3;CHECK(event(1,0,false)==KC_7);
    CHECK(normal_up==1);CHECK(combos[0].state==0);
    puts("rejected_combo: PASS");
}
static void stack_selector(void) {
    layout();app_layout_stack_counts[0]=2;
    CHECK(event(0,2,true)==UINT16_MAX);app_layout_stack_counts[0]=0;
    app_layout_keycodes[0][12]=KC_A;
    CHECK(event(0,2,false)==UINT16_MAX);
    puts("stack_selector: PASS");
}
static void passthrough_events(void) {
    layout();
    const keyevent_t events[]={
        {.type=COMBO_EVENT,.key={0,0}}, {.type=ENCODER_EVENT,.key={255,0}},
        {.type=KEY_EVENT,.key={MATRIX_ROWS,0}}, {.type=KEY_EVENT,.key={0,MATRIX_COLS}},
        {.type=KEY_EVENT,.key={0,0}}
    };
    for (unsigned i=0;i<sizeof(events)/sizeof(events[0]);i++) {
        keyrecord_t record={.event=events[i],.keycode=KC_B};
        CHECK(pre_process_record_kb(KC_B,&record));CHECK(record.keycode==KC_B);
    }
    CHECK(ruen_calls==5 && user_calls==5);
    puts("passthrough_events: PASS");
}
int main(int argc,char **argv) {
    CHECK(argc==2);unsigned test=(unsigned)atoi(argv[1]);
    switch(test) {
        case 0:momentary_combo_release();break;
        case 1:no_key_sentinel();break;
        case 2:transparent_binding();break;
        case 3:case 4:case 5:case 6:session_change(test-3);break;
        case 7:case 8:fallback_press(test-7);break;
        case 9:normal_key();break;
        case 10:rejected_combo();break;
        case 11:stack_selector();break;
        case 12:passthrough_events();break;
        default:CHECK(false);
    }
    return 0;
}
'''
source = preamble + combo_header + fixtures + hid + function('keyboards/ergohaven/ergohaven_main.c', 'pre_process_record_kb') + combo + tests
with tempfile.TemporaryDirectory(prefix='ergohaven-app-combo-', dir=os.environ.get('TMPDIR')) as directory:
    path = Path(directory)
    (path/'test.c').write_text(source)
    subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=undefined','-fno-sanitize-recover=all',str(path/'test.c'),'-o',str(path/'test')],check=True)
    cases = ['momentary_combo_release', 'no_key_sentinel', 'transparent_binding',
             'layout_replacement', 'deactivate', 'timeout', 'disabled_commit',
             'inactive_fallback', 'unset_fallback', 'normal_key', 'rejected_combo',
             'stack_selector', 'passthrough_events']
    failures = []
    for index, name in enumerate(cases):
        result = subprocess.run([str(path/'test'), str(index)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        print(f'[{name}]', flush=True)
        print(result.stdout, end='', flush=True)
        print(result.stderr, end='', flush=True)
        if result.returncode:
            failures.append(name)
    print(f'{len(cases)-len(failures)}/{len(cases)} cases passed; {len(failures)} failed', flush=True)
    raise SystemExit(bool(failures))
