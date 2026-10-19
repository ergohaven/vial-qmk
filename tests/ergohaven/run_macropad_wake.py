#!/usr/bin/env python3
"""Production HID/display/screen bodies + real LVGL; USB/clock/storage/panel boundaries injected."""
from pathlib import Path
import argparse, re, subprocess
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('output', type=Path)
p.add_argument('--lvgl', type=Path, required=True)
p.add_argument('--stubs', type=Path, required=True, help='Hardware headers from focused run_macropad_bugs.py --only 004')
p.add_argument('--ref', help='Baseline firmware source revision (tests remain current)')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
def source(path):
    if a.ref:
        return subprocess.check_output(['git', 'show', f'{a.ref}:{path}'], cwd=ROOT, text=True)
    return (ROOT/path).read_text()
def function(src, name):
    m = re.search(r'^(?:static )?[a-zA-Z_][^\n;]*\b'+name+r'\([^;\n]*\)\s*\{', src, re.M)
    assert m, name
    end = src.index('{', m.start())+1
    depth = 1
    while depth:
        depth += (src[end]=='{')-(src[end]=='}')
        end += 1
    return src[m.start():end]+'\n'
hid = source('keyboards/ergohaven/hid.c')
hid = hid[hid.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):hid.index('\nhid_data_t *get_hid_data')].rsplit('#endif', 1)[0]
modes = source('keyboards/ergohaven/macropad/display_modes.c')
state = modes[modes.index('static uint32_t screen_timer'):modes.index('bool display_should_wake_on_usb_resume')]
bodies = ''.join(function(modes,n) for n in ['apply_screen_state','display_process_matrix_press','display_process_keyevent','volume_direction_for_keycode','display_process_encoder_event','display_housekeeping_task'])
if 'void hid_app_layout_changed_kb(' in modes:
    bodies += function(modes,'hid_app_layout_changed_kb')
layout = source('keyboards/ergohaven/macropad/screen_layout.c')
labels = source('keyboards/ergohaven/src/display/eh_keycode_str.c')
screen = labels[labels.index('// Application-layout labels'):]
screen += layout[layout.index('static lv_obj_t *screen_layout;'):layout.index('enum {')]
screen += layout[layout.index('static bool     key_pressed'):layout.index('static uint8_t  button_style')]
screen += layout[layout.index('static uint8_t prev_layer ='):layout.index('static uint8_t screen_layout_control_for_label')]
for name in ['get_keycode','get_encoder_keycode','screen_layout_control_for_label','set_key_pressed','finish_key_press_animations','screen_layout_set_layer_name','screen_layout_header_name','screen_layout_builtin_key_icon','screen_layout_set_key_content','screen_layout_refresh_key_content','screen_layout_load','screen_layout_housekeep']:
    screen += function(layout,name)
text = (ROOT/'tests/ergohaven/macropad_wake.c').read_text().replace('SCREEN_STATE',state).replace('HID_PRODUCTION',hid).replace('DISPLAY_PRODUCTION',bodies).replace('LAYOUT_PRODUCTION',screen)
src = a.output/'wake.c'
src.write_text(text)
exe = a.output/'wake'
includes = [a.stubs,a.lvgl,ROOT/'keyboards/ergohaven',ROOT/'keyboards/ergohaven/macropad',ROOT,ROOT/'quantum',ROOT/'quantum/keymap_extras',ROOT/'quantum/sequencer']
fonts = []
for size in [20,28]:
    font = a.output/f'eh_font_montserrat_{size}.c'
    font.write_text(source(f'keyboards/ergohaven/src/display/fonts/eh_font_montserrat_{size}.c'))
    fonts.append(str(font))
cmd = ['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-type-limits','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DEH_APP_LAYOUT_ENABLE','-DOS_DETECTION_ENABLE','-DLV_USE_LOG=1','-DLV_USE_ASSERT_MALLOC=1']
subprocess.run([*cmd,*['-I'+str(d) for d in includes],str(src),*fonts,*sorted(str(f) for f in (a.lvgl/'src').rglob('*.c')),'-lm','-o',str(exe)],check=True)
print(exe)
