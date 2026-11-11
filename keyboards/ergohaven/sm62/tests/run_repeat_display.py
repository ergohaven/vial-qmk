#!/usr/bin/env python3
"""Production SM62 dispatch + full KEYMAP view + pinned LVGL + native Vial TD.
Synthetic boundaries: matrix, clock, EEPROM values, USB report sink, panel flush.
No physical scan/split transport or real HID/companion traffic is claimed.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / 'tests/ergohaven'))
from macropad_native_support import validate_lvgl
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
validate_lvgl(ROOT / 'lib/lvgl')
# Reuse the existing extraction, not a second implementation of dispatch.
ns = {'__file__': str(HERE / 'run_pipeline.py')}
saved = sys.argv
sys.argv = [str(HERE/'run_pipeline.py'), '--firmware', str(ROOT), '--output', str(a.output)]
exec(compile((HERE/'run_pipeline.py').read_text().split("cmd = ['gcc'")[0], str(HERE/'run_pipeline.py'), 'exec'), ns)
sys.argv = saved
body, stripped, source = (ns[k] for k in ['body', 'stripped', 'source'])
header = ns['header']
for name in ['vial_keycode_down', 'vial_keycode_up', 'vial_keycode_tap', 'process_record_vial', 'display_process_keyevent', 'read_source_layers_cache', 'get_event_keycode']:
    header = re.sub(r'^static [^\n]*\b'+name+r'\([^\n]*\n', '', header, flags=re.M)
header = header.replace('static bool keys[256];', 'static bool hid_keys[256];').replace('keys[k]', 'hid_keys[k]')
header = header.replace('#define GET_TAPPING_TERM(k,r) TAPPING_TERM', '#define GET_TAPPING_TERM(k,r) get_tapping_term(k,r)')
header += '''
#include "lvgl.h"
#include "src/display/eh_symbols.h"
#include "src/display/application_icons.h"
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define SM62_FAST_RETURN 1
#define SM62_EFFECT 0
#define VIAL_TAP_DANCE_ENTRIES 32
#define VIAL_TAP_DANCE_ENABLE
#define TAPPING_TERM_PER_KEY
#define VIAL_MATRIX_MAGIC 254
static uint16_t get_tapping_term(uint16_t, keyrecord_t *);
static uint16_t QS_tap_code_delay;
static void qs_wait_ms(uint16_t t) { now+=t; }
static void action_exec(keyevent_t e) { (void)e; assert(!"complex native Vial action is outside fixture"); }
static void vial_keycode_down(uint16_t);
static void vial_keycode_up(uint16_t);
static void vial_keycode_tap(uint16_t);
static bool process_record_vial(uint16_t, keyrecord_t *);
static uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c) { return defaults[l][r][c]; }
static const char *hid_sm62_default_layer_name(uint8_t l) { (void)l; return "Default"; }
'''
cache = source('quantum/action_layer.c')
header += '#include <limits.h>\n#define MAX_LAYER_BITS 5\n'
header += cache[cache.index('uint8_t source_layers_cache['):cache.index(';',cache.index('uint8_t source_layers_cache['))+1]+'\n'
header += ''.join(body(cache, sig) for sig in ['void update_source_layers_cache_impl(', 'uint8_t read_source_layers_cache_impl(', 'void update_source_layers_cache(', 'uint8_t read_source_layers_cache('])
# Layer selection remains a boundary (fixtures use the single base layer).
# Cache storage/update/read and quantum get_event_keycode are actual production.
header += 'static bool disable_action_cache;\nstatic uint8_t layer_switch_get_layer(keypos_t p) { (void)p; return get_highest_layer(layer_state|default_layer_state); }\n'
header += '#undef NO_ACTION_LAYER\n'+body(ns['quantum'], 'uint16_t get_event_keycode(')+'\n#define NO_ACTION_LAYER\n'
vial = source('quantum/vial.c')
# EEPROM is injected as explicit native TD configurations, callbacks stay real.
vialh = source('quantum/vial.h')
match = re.search(r'typedef struct[^;]*?\{[^}]*\}\s*vial_tap_dance_entry_t;', vialh, re.S)
if not match:
    raise RuntimeError('native TD entry declaration not found')
header += match.group(0) + '''
static vial_tap_dance_entry_t native_entries[VIAL_TAP_DANCE_ENTRIES];
static int dynamic_keymap_get_tap_dance(uint8_t i,vial_tap_dance_entry_t *v) { if(i>=VIAL_TAP_DANCE_ENTRIES)return -1;*v=native_entries[i];return 0; }
'''
headers = ''.join(stripped(source(f'quantum/process_keycode/process_{n}.h')) for n in ['key_override','tap_dance','combo'])
fixture = (HERE/'pipeline_fixture.inc').read_text()
fixture = fixture[:fixture.index('static tap_dance_action_t dance;')] + fixture[fixture.index('static const uint16_t chord[]'):]
# Real native Vial action table/getter is used instead of the generic TD pair.
native = vial[vial.index('enum {', vial.index('#ifdef VIAL_TAP_DANCE_ENABLE', vial.index('void vial_keycode_tap'))):vial.index('\n#endif', vial.index('static void reload_tap_dance(void) {'))]
native = stripped(native)
native += body(vial, 'uint16_t tap_dance_count(') + body(vial, 'tap_dance_action_t* tap_dance_get(')
native += body(vial, 'uint16_t get_tapping_term(').replace('uint16_t get_tapping_term(', 'static uint16_t get_tapping_term(')
native += body(vial, 'bool process_record_vial(').replace('bool process_record_vial(', 'static bool process_record_vial(')
# Definition must follow QMK processor declarations and precede native callback use.
native_prefix = 'void process_tap_dance_action_on_dance_finished(tap_dance_action_t *);\n'
native_prefix += vial[vial.index('uint16_t g_vial_magic_keycode_override;'):vial.index('\n#ifdef VIAL_TAP_DANCE_ENABLE', vial.index('uint16_t g_vial_magic_keycode_override;'))]
processors = ''.join(stripped(source(f'quantum/process_keycode/process_{n}.c')) for n in ['key_override','tap_dance','combo'])
display = '''
typedef struct {void (*init)(void);void (*load)(void);void (*housekeep)(void);} eh_screen_t;
static bool side_left=true;
static bool matrix[10][7];
static bool is_keyboard_left(void) { return side_left; }
static bool matrix_is_on(uint8_t r,uint8_t c) { return matrix[r][c]; }
static uint8_t get_current_layer(void) { return get_highest_layer(layer_state|default_layer_state); }
static struct {uint8_t mode,cyrillic_enabled;} local_settings={3,1};
static const typeof(local_settings) *sm62_local_display_settings(void) { return &local_settings; }
static uint32_t sm62_local_keymap_hold_ms(void) { return 200; }
static lv_obj_t *display_view_create(void) { return lv_obj_create(NULL); }
static void display_show_view(lv_obj_t *o) { lv_scr_load(o); }
'''
# Compile the actual formatter; only hardware-dependent includes are replaced.
formatter = stripped(source('keyboards/ergohaven/src/display/eh_keycode_str.c'))
# Ergohaven custom keycodes are needed by the complete formatter.
ergoh = source('keyboards/ergohaven/ergohaven.h')
custom = ergoh[ergoh.index('enum custom_keycodes'):ergoh.index('};', ergoh.index('enum custom_keycodes'))+2] if 'enum custom_keycodes' in ergoh else ''
ruen = source('keyboards/ergohaven/src/eh_ruen.h')
ruen = ruen[ruen.index('enum ruen_custom_keycodes'):ruen.index('};')+2]
pointing = source('keyboards/ergohaven/src/eh_pointing.h')
pointing = pointing[pointing.index('enum {'):pointing.index('};', pointing.index('enum {'))+2]
display += '#define MOD_MASK_CTRL 0x11\n#define MOD_MASK_SHIFT 0x22\n#define MOD_MASK_ALT 0x44\n#define MOD_MASK_GUI 0x88\n' + custom + '\n' + ruen + '\n' + pointing + '\n' + formatter
# Actual display event hook is intentionally weak/no-op on SM62; observer is matrix-driven.
display += 'void display_process_keyevent(uint8_t row, uint8_t col, bool pressed);\n'
display += stripped(source('keyboards/ergohaven/sm62/application_display.c'))
display += stripped(source('keyboards/ergohaven/sm62/screen_keymap.c'))
cases = (HERE/'repeat_display_cases.inc').read_text()
text = header + ns['production'] + headers + fixture + native_prefix + native + processors + display + ns['pre'] + ns['kb'] + body(ns['quantum'], 'uint16_t get_record_keycode(') + body(ns['quantum'], 'bool pre_process_record_quantum(') + ns['chain'] + cases
src = a.output/'repeat_display.c'
src.write_text(text)
fonts = [ROOT/'keyboards/ergohaven/src/display/fonts'/f'eh_font_iosevka_{n}.c' for n in [18,28]]
fonts += [ROOT/'keyboards/ergohaven/macropad'/f'sm62_font_zen_medium_{n}.c' for n in [16,17,20,28]]
cmd = ['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-type-limits','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_ASSERT_MALLOC=1',*['-I'+str(x) for x in [ROOT/'lib/lvgl',ROOT/'quantum',ROOT/'quantum/sequencer',ROOT/'quantum/keymap_extras',ROOT/'tmk_core/common',ROOT/'keyboards/ergohaven']],str(src),*map(str,fonts),*sorted(str(f) for f in (ROOT/'lib/lvgl/src').rglob('*.c')),'-lm','-o',str(a.output/'repeat_display')]
r = subprocess.run(cmd,capture_output=True,text=True)
(a.output/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:
    print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r = subprocess.run([str(a.output/'repeat_display')],capture_output=True,text=True)
(a.output/'results.log').write_text(r.stdout+r.stderr)
print(r.stdout+r.stderr,end='')
(a.output/'generated.sha256').write_text(hashlib.sha256(src.read_bytes()).hexdigest()+'  repeat_display.c\n')
raise SystemExit(r.returncode)
