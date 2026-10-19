#!/usr/bin/env python3
"""Focused native production-source probes, following run_native.py conventions."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('output', type=Path)
p.add_argument('--only', choices=['002', '004'], required=True)
p.add_argument('--ref', help='Read production source from this Git revision')
p.add_argument('--lvgl', type=Path, help='LVGL source at the pinned lib/lvgl revision')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def source(path):
    if a.ref:
        return subprocess.check_output(['git', 'show', f'{a.ref}:{path}'], cwd=ROOT, text=True)
    return (ROOT / path).read_text()

def function(path, name):
    src = source(path)
    m = re.search(r'^(?:static )?[a-zA-Z_][^\n;]*\b' + name + r'\([^;\n]*\)\s*\{', src, re.M)
    assert m, name
    start = m.start()
    end = src.index('{', start) + 1
    depth = 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end] + '\n'

extra = []
includes = [ROOT/'tests/ergohaven', ROOT, ROOT/'quantum', ROOT/'quantum/keymap_extras', ROOT/'quantum/sequencer']
if a.only == '002':
    hid = source('keyboards/ergohaven/hid.c')
    production = hid[hid.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):hid.index('\nhid_data_t *get_hid_data')].rsplit('#endif', 1)[0]
    if 'hid_app_layout_changed_kb' in production:
        # This encoder-only harness has no standby screens; keep the new
        # cross-component notification inert here. run_macropad_wake covers it.
        production = 'void hid_app_layout_changed_kb(void) {}\n' + production
    production += function('keyboards/ergohaven/ergohaven_main.c', 'get_current_layer')
    production += function('keyboards/ergohaven/macropad/display_modes.c', 'apply_screen_state')
    for name in ['volume_direction_for_keycode', 'display_process_encoder_event']:
        production += function('keyboards/ergohaven/macropad/display_modes.c', name)
    production += function('keyboards/ergohaven/ergohaven_main.c', 'pre_process_record_kb')
    events = source('quantum/keyboard.h')
    events = events[events.index('/* key matrix position */'):events.index('/* Common keypos_t object factory */')]
    modes = source('keyboards/ergohaven/macropad/display_modes.c')
    states = modes[modes.index('typedef enum {'):modes.index('eh_screen_t current_screen;')]
    text = (ROOT/'tests/ergohaven/macropad_encoder.c').read_text().replace('PRODUCTION', production).replace('EVENT_TYPES', events).replace('SCREEN_STATE', states)
else:
    if not a.lvgl or not (a.lvgl/'lvgl.h').is_file():
        p.error('--only 004 requires --lvgl at e19410f8f8a256609da72cff549598e0df6fa4cf')
    pinned = 'e19410f8f8a256609da72cff549598e0df6fa4cf'
    tree = subprocess.check_output(['git', '-C', str(a.lvgl), 'ls-tree', '-rz', '--full-tree', pinned])
    checked = 0
    for entry in tree.split(b'\0'):
        if not entry:
            continue
        metadata, filename = entry.split(b'\t')
        _, kind, oid = metadata.split()
        if kind != b'blob' or not filename.endswith((b'.c', b'.h')):
            continue
        data = (a.lvgl/filename.decode()).read_bytes()
        actual = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        assert actual == oid.decode(), f'LVGL source differs from pinned revision: {filename!r}'
        checked += 1
    print(f'LVGL pinned source: {checked} C/header blobs verified', flush=True)
    stubs = a.output/'stubs'
    stubs.mkdir(exist_ok=True)
    (stubs/'os_detection.h').write_text('''#pragma once
typedef enum { OS_UNSURE, OS_LINUX, OS_MACOS, OS_IOS } os_variant_t;
static os_variant_t test_os=OS_LINUX;
static os_variant_t detected_host_os(void){return test_os;}
''')
    (stubs/'quantum.h').write_text('''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "quantum/keycodes.h"
#include "quantum/quantum_keycodes.h"
#define PROGMEM
#define MOD_MASK_CTRL 0x11
#define MOD_MASK_SHIFT 0x22
#define MOD_MASK_ALT 0x44
#define MOD_MASK_GUI 0x88
#define MOD_LSFT 2
#define MOD_LCTL 1
typedef struct keyrecord keyrecord_t;
''')
    labels = source('keyboards/ergohaven/src/display/eh_keycode_str.c')
    production = labels[labels.index('// Application-layout labels'):]
    layout = source('keyboards/ergohaven/macropad/screen_layout.c')
    production += '#define NLABELS 15\n'
    names = ['key_labels', 'key_icons', 'key_icon_dsc', 'key_icon_colors', 'key_icon_bits',
             'label_visual', 'label_execution_state', 'label_animation_frame', 'label_text', 'key_pressed']
    for name in names:
        declaration = re.search(r'^static [^\n]*\b'+name+r'\[[^\n]+;', layout, re.M)
        assert declaration, name
        production += declaration[0]+'\n'
    production += function('keyboards/ergohaven/macropad/screen_layout.c', 'screen_layout_builtin_key_icon')
    production += function('keyboards/ergohaven/macropad/screen_layout.c', 'screen_layout_set_key_content')
    text = (ROOT/'tests/ergohaven/macropad_labels.c').read_text().replace('PRODUCTION', production)
    includes = [stubs, a.lvgl, ROOT/'keyboards/ergohaven', ROOT/'keyboards/ergohaven/macropad', *includes]
    extra = sorted(str(f) for f in (a.lvgl/'src').rglob('*.c'))
    for size in [20, 28]:
        font = a.output/f'eh_font_montserrat_{size}.c'
        font.write_text(source(f'keyboards/ergohaven/src/display/fonts/eh_font_montserrat_{size}.c'))
        extra.append(str(font))
    # QMK's unsigned >= 0 range macro triggers GCC's type-limits warning.
    extra += ['-DLV_CONF_SKIP', '-DEH_APP_LAYOUT_ENABLE', '-DOS_DETECTION_ENABLE', '-DLV_USE_LOG=1', '-DLV_USE_ASSERT_MALLOC=1', '-Wno-type-limits', '-lm']
path = a.output / f'bug{a.only}.c'
path.write_text(text)
exe = path.with_suffix('')
cmd = ['gcc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all']
cmd += ['-I'+str(d) for d in includes]
subprocess.run([*cmd, str(path), *extra, '-o', str(exe)], check=True)
r = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(a.output/'results.txt').write_text(r.stdout)
print(r.stdout, end='')
r.check_returncode()
