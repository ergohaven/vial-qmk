#!/usr/bin/env python3
"""Focused production-backed locked assignment/publication regression.

No MCU, flashing, reset, or real bootloader. Outputs must be in managed cache.
Extract complete production functions, not copied dispatcher case bodies.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

P = argparse.ArgumentParser(description=__doc__)
P.add_argument('--output-dir', type=Path, required=True)
a = P.parse_args()
S = Path(__file__).resolve().parents[3]
O = a.output_dir.resolve()
if sys.flags.optimize:
    P.error('Python assertions must remain enabled')
if not a.output_dir.is_absolute() or os.path.lexists(a.output_dir) or O.exists() or not O.parent.is_dir() or O == S or S in O.parents or O in S.parents:
    P.error('output must be fresh, absolute, with an existing parent, outside source tree and its ancestors')
O.mkdir()

def function(path, name):
    src = (S / path).read_text()
    match = re.search(r'^(?:__attribute__\(\(unused\)\) )?(?:static )?(?:inline )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', src, re.M)
    assert match, name
    start = src.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[match.start():end] + '\n'

def literal(path, name):
    for node in ast.parse((S / path).read_text()).body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == name for t in node.targets):
            if isinstance(node.value, ast.Constant) and isinstance(node.value.value, str):
                return node.value.value
    raise AssertionError(name)

inputs = ['quantum/via.c', 'quantum/via.h', 'quantum/vial.c', 'quantum/vial.h',
          'quantum/dynamic_keymap.c', 'quantum/nvm/eeprom/nvm_dynamic_keymap.c',
          'keyboards/ergohaven/hid.c', 'keyboards/ergohaven/sm62/compact_usb.h',
          'keyboards/ergohaven/sm62/resident_cache.h',
          'keyboards/ergohaven/sm62/resident_cache_flash.h',
          'keyboards/ergohaven/sm62/application_geometry.h',
          'tests/ergohaven/sm62_locked_boot/run.py',
          'tests/ergohaven/sm62_locked_boot/cases.c',
          'tests/ergohaven/sm62_macro_storage/test_macro_storage.py']
before = {p: hashlib.sha256((S / p).read_bytes()).hexdigest() for p in inputs}
# Reuse only boundary declarations from the established harness; never run or
# modify that package, refresh its pins, or replace production ingress logic.
stubs = literal('tests/ergohaven/sm62_macro_storage/test_macro_storage.py', 'stubs')
stubs = re.sub(r'^static uint16_t dynamic_keymap_get_keycode.*?\n', '', stubs, flags=re.M)
stubs = re.sub(r'^static void dynamic_keymap_set_keycode.*?\n', '', stubs, flags=re.M)
stubs = stubs.replace('assert(0);}\nstatic const uint8_t keyboard_definition', 'boot_count++;}\nstatic const uint8_t keyboard_definition')
stubs = stubs.replace('static void raw_hid_receive_kb(uint8_t*p,unsigned n){(void)p;(void)n;assert(0);}',
                      'static void raw_hid_receive_kb(uint8_t*p,unsigned n){assert(hid_app_layout_process_packet(p,n));}')
# Unused feature declarations do not supply behavior to any exercised path.
pre = '''#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define VIAL_ENABLE
#define KC_NO 0
#define KC_TRNS 1
#define QK_BOOT 0x7c00
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define DYNAMIC_KEYMAP_EEPROM_ADDR 0
#define VIA_PROTOCOL_VERSION 9
#define VIA_FIRMWARE_VERSION 0
#define VIAL_HID_MAGIC 0xa3fb1cbc0abd6125ull
bool vial_unlocked;
static bool vial_unlock_in_progress;
static unsigned boot_count, sent, output_chars;
static uint8_t eeprom[8192],reply[32];
static uint8_t eeprom_read_byte(const void *p){assert((uintptr_t)p<8192);return eeprom[(uintptr_t)p];}
static void eeprom_update_byte(void *p,uint8_t v){assert((uintptr_t)p<8192);eeprom[(uintptr_t)p]=v;}
bool hid_app_layout_process_packet(uint8_t*,uint8_t);
'''
# Full application engine and compact parser, same extraction seam as the
# existing SM62 native layout qualification. Only hardware/time/action sinks.
app = (S / 'keyboards/ergohaven/hid.c').read_text()
app = app[app.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):app.index('\nhid_data_t *get_hid_data')].rsplit('#endif', 1)[0]
app_boundary = '''
static uint32_t now=100,layer_state=1,default_layer_state=1;
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static void layer_clear(void){layer_state=1;}
static void hid_app_layout_changed_kb(void){}
'''
stubs = stubs.replace('return 0;}\nstatic uint32_t via_get_layout_options', 'return now;}\nstatic uint32_t via_get_layout_options')
# Out-of-scope commands must abort rather than inventing successful behavior.
stubs += '''
static void dynamic_keymap_reset(void){assert(0);}
static unsigned dynamic_keymap_macro_get_count(void){assert(0);return 0;}
static unsigned dynamic_keymap_macro_get_buffer_size(void){assert(0);return 0;}
static void dynamic_keymap_macro_get_buffer(unsigned o,unsigned n,uint8_t*p){(void)o;(void)n;(void)p;assert(0);}
static void dynamic_keymap_macro_set_buffer(unsigned o,unsigned n,uint8_t*p){(void)o;(void)n;(void)p;assert(0);}
static void dynamic_keymap_macro_reset(void){assert(0);}
'''
enums = '\n'.join(re.findall(r'enum via_(?:command_id|keyboard_value_id)\s*\{.*?\};', (S / 'quantum/via.h').read_text(), re.S))
enums += '\n' + '\n'.join(re.findall(r'enum\s*\{.*?\};', (S / 'quantum/vial.h').read_text(), re.S)[:2])
code = pre + enums + app_boundary + stubs
# Compile the real exported function separately, with its real declaration.
declaration = re.search(r'^uint16_t vial_keycode_firewall\(uint16_t in\);', (S / 'quantum/vial.h').read_text(), re.M)
assert declaration, 'public firewall declaration missing'
code += declaration.group() + '\n'
firewall = '#include <stdint.h>\n#include <stdbool.h>\n#define QK_BOOT 0x7c00\nextern bool vial_unlocked;\n'
firewall += declaration.group() + '\n' + function('quantum/vial.c', 'vial_keycode_firewall')
(O / 'firewall.c').write_text(firewall)
code += app
code += '''
static uint8_t flash[SM62_CACHE_RECORDS][8192];
static unsigned flash_erases, flash_programs;
const uint8_t *sm62_cache_flash_record(unsigned r){return r<SM62_CACHE_RECORDS?flash[r]:NULL;}
bool sm62_cache_flash_erase(unsigned r){assert(r<SM62_CACHE_RECORDS);memset(flash[r],255,8192);flash_erases++;return true;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t*p){assert(r<SM62_CACHE_RECORDS && off<=8192-256 && off%256==0);for(unsigned i=0;i<256;i++){assert((flash[r][off+i]&p[i])==p[i]);flash[r][off+i]&=p[i];}flash_programs++;return true;}
'''
for name in ['dynamic_keymap_key_to_eeprom_address', 'nvm_dynamic_keymap_read_keycode', 'nvm_dynamic_keymap_update_keycode', 'nvm_dynamic_keymap_update_buffer']:
    code += function('quantum/nvm/eeprom/nvm_dynamic_keymap.c', name)
for name in ['dynamic_keymap_get_keycode', 'dynamic_keymap_set_keycode']:
    code += function('quantum/dynamic_keymap.c', name)
code += function('quantum/vial.c', 'vial_handle_cmd')
# KEYBOARD_ergohaven_sm62 is intentionally defined after raw VIA extraction:
# build VIA with native EEPROM storage, app parser with shipping SM62 SINGLE8.
code = code.replace('#    define EH_APP_LAYOUT_PROTOCOL_VERSION', '#define KEYBOARD_ergohaven_sm62\n#    define EH_APP_LAYOUT_PROTOCOL_VERSION', 1)
code += '\n#undef KEYBOARD_ergohaven_sm62\n' + function('quantum/via.c', 'raw_hid_receive')
code += (Path(__file__).parent / 'cases.c').read_text()
(O / 'test.c').write_text(code)
cmd = ['gcc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
       '-Wno-unused-function', '-fsanitize=undefined', '-fno-sanitize-recover=all',
       '-I' + str(S / 'keyboards/ergohaven'), str(O / 'test.c'), str(O / 'firewall.c'), '-o', str(O / 'test')]
r = subprocess.run(cmd, capture_output=True, text=True)
(O / 'compile.log').write_text(r.stdout + r.stderr)
print(r.stdout + r.stderr, end='')
if r.returncode:
    sys.exit(r.returncode)
results = []
for case in ['native', 'application', 'ingress', 'resident']:
    r = subprocess.run([str(O / 'test'), case], capture_output=True, text=True)
    print(f'CASE {case} exit={r.returncode}\n{r.stdout}{r.stderr}', end='', flush=True)
    results.append({'case': case, 'exit': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr})
assert before == {p: hashlib.sha256((S / p).read_bytes()).hexdigest() for p in inputs}
(O / 'result.json').write_text(json.dumps({'sources': before, 'compile': cmd, 'cases': results}, indent=2) + '\n')
sys.exit(int(any(r['exit'] for r in results)))
