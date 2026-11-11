#!/usr/bin/env python3
"""Source-extracted production dispatcher + journal CLI; hardware-only fixtures.
No parser/storage implementation is copied or translated.
"""
from pathlib import Path
import argparse,re,subprocess,os,json,hashlib
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
S=a.source.resolve();O=a.output.resolve();K=S/'keyboards/ergohaven/sm62'
if not a.source.is_absolute() or not (S/'quantum/via.c').is_file(): ap.error('source must be absolute production QMK root')
if not a.output.is_absolute() or os.path.lexists(a.output) or not O.parent.is_dir() or O==S or S in O.parents or O in S.parents or O==P or P in O.parents or O in P.parents: ap.error('output must be fresh absolute outside source/package, with existing parent')
cache=Path(os.environ['AGENT_BUILD_CACHE']).resolve()
if cache not in O.parents: ap.error('output must be within managed AGENT_BUILD_CACHE')
if __debug__ is False: ap.error('assertions must be enabled')
O.mkdir()
# Shipping SINGLE8 legacy EEPROM / macro overlay, TD disabled.
used={}
def read(path):
 p=S/path; blob=p.read_bytes();digest=hashlib.sha256(blob).hexdigest()
 assert path not in used or used[path]==digest, 'source drift: '+path
 used[path]=digest;return blob.decode()
def function(path,name):
 s=read(path);m=re.search(r'^(?:__attribute__\(\([^\n]*?\)\) )?(?:static )?(?:inline )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 i=s.index('{',m.start());d=1;e=i+1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
def noincludes(s):return re.sub(r'^\s*#\s*(?:include[^\n]*|pragma once)','',s,flags=re.M)
def typedef(s,name):
 return re.search(r'typedef struct(?: PACKED)? \{[^}]*\} '+name+r';',s,re.S)[0]
# Reuse the existing reviewed native boundary declarations, not its algorithms.
pre=(P/'pre.c').read_text();stubs=(P/'stubs.c').read_text();pre=pre.replace('#include <stdint.h>','#include <stdint.h>\n#include <stddef.h>')
pre+=(P/'boundary.c').read_text()
# Common configs and wire struct definitions are verbatim production source.
geometry=json.loads(read('keyboards/ergohaven/sm62/keyboard.json'))['layouts']['LAYOUT']['layout']
assert len(geometry)==62 and max(k['matrix'][0] for k in geometry)==9 and max(k['matrix'][1] for k in geometry)==6, 'unsupported controller geometry'
config=noincludes(read('keyboards/ergohaven/config.h'))+noincludes(read('keyboards/ergohaven/sm62/config.h'))+noincludes(read('keyboards/ergohaven/sm62/keymaps/v1/config.h'))
config+='\n#define MATRIX_ROWS 10\n#define MATRIX_COLS 7\n#define TOTAL_EEPROM_BYTE_COUNT 8192\n#define VIA_ENABLE\n#define VIAL_ENABLE\n#define QMK_SETTINGS\n#define EECONFIG_USER_DATA_SIZE ((size_t)0)\n#define PACKED __attribute__((packed))\n#define STATIC_ASSERT(c,m) _Static_assert(c,m)\n#define QK_BOOT 0x7c00\n#define KC_NO 0\n'
# QMK_SETTINGS storage shape participates in actual EEPROM offsets, while
# settings protocol hooks below are deliberately unsupported (not invented data).
config+='#define VIA_PROTOCOL_VERSION 9\n#define VIAL_HID_MAGIC 0xa3fb1cbc0abd6125ull\n'
config+=typedef(read('quantum/qmk_settings.h'),'qmk_settings_t')+'\n'
eh=read('quantum/nvm/eeprom/nvm_eeprom_eeconfig_internal.h'); config+=noincludes(eh)+'\n'
config+='#define VIA_EEPROM_LAYOUT_OPTIONS_SIZE 1\n#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 0\n'
config+=noincludes(read('quantum/nvm/eeprom/nvm_eeprom_via_internal.h'))+'\n'
vh=read('quantum/vial.h')
config+=re.search(r'^extern bool vial_unlocked;',vh,re.M)[0]+'\n'
for name in ['vial_tap_dance_entry_t','vial_combo_entry_t','vial_key_override_entry_t','vial_alt_repeat_key_entry_t']:config+=typedef(vh,name)+'\n'
config+='#define VIAL_COMBO_ENABLE\n#define VIAL_KEY_OVERRIDE_ENABLE\n#define VIAL_ALT_REPEAT_KEY_ENABLE\n#define VIAL_ALT_REPEAT_KEY_ENTRIES 32\n'
config+='#define VIAL_TAP_DANCE_ENTRIES 0\n_Static_assert(DYNAMIC_KEYMAP_LAYER_COUNT==16 && DYNAMIC_KEYMAP_MACRO_COUNT==109, "unsupported controller counts");\n'
# Full actual NVM TU retains all byte ordering, bounds and extension routing.
nvm=noincludes(read('quantum/nvm/eeprom/nvm_dynamic_keymap.c'))
# All dynamic wrappers used by public VIA/Vial paths.
dk='quantum/dynamic_keymap.c'; wrappers=''
for name in ['dynamic_keymap_get_layer_count','dynamic_keymap_get_keycode','dynamic_keymap_set_keycode','dynamic_keymap_get_buffer','dynamic_keymap_set_buffer','dynamic_keymap_macro_get_count','dynamic_keymap_macro_get_buffer_size','dynamic_keymap_macro_get_buffer','dynamic_keymap_macro_set_buffer','dynamic_keymap_macro_reset']:
 wrappers+=function(dk,name)
wrappers+=function(dk,'dynamic_keymap_read_byte')
for kind in ['combo','key_override','alt_repeat_key']:
 for op in ['get','set']:wrappers+=function(dk,'dynamic_keymap_'+op+'_'+kind)
# Keep only unrelated hardware/feature boundary stubs; real storage wrappers replace theirs.
for name in ['raw_hid_receive_kb','raw_hid_send','dynamic_keymap_get_keycode','dynamic_keymap_set_keycode','dynamic_keymap_get_layer_count','dynamic_keymap_get_buffer','dynamic_keymap_set_buffer']:
 stubs=re.sub(r'^static [^\n]*\b'+name+r'\([^\n]*\n','',stubs,flags=re.M)
stubs=re.sub(r'^#define VIAL_(?:TAP_DANCE|COMBO|KEY_OVERRIDE|ALT_REPEAT_KEY)_ENTRIES[^\n]*\n','',stubs,flags=re.M)
stubs=stubs.replace('static uint32_t timer_read32(void){return 0;}','static uint32_t timer_read32(void){return bridge_now;}')
stubs=re.sub(r'^#define VIAL_KEYBOARD_UID[^\n]*\n','',stubs,flags=re.M)
for name in ['vial_get_unlock_combo_coords']:
 stubs=re.sub(r'^static [^\n]*\b'+name+r'\([^\n]*\n','',stubs,flags=re.M)
for pattern in [r'^static uint8_t vial_unlock_counter;\n', r'^static bool vial_unlock_holding;\n',r'^#define VIAL_UNLOCK_(?:NUM_KEYS|COUNTER_MAX)[^\n]*\n']:
 stubs=re.sub(pattern,'',stubs,flags=re.M)
stubs=stubs.replace('static matrix_row_t matrix_get_row(unsigned r){(void)r;return 0;}', 'static matrix_row_t bridge_matrix[10];\nstatic matrix_row_t matrix_get_row(unsigned r){assert(r<10);return bridge_matrix[r];}\nstatic bool matrix_is_on(unsigned r,unsigned c){assert(r<10&&c<7);return !!(matrix_get_row(r)&(1u<<c));}\nstatic uint32_t timer_elapsed32(uint32_t t){return bridge_now-t;}')
vial_source=read('quantum/vial.c')
unlock=vial_source[vial_source.index('#define VIAL_UNLOCK_HOLD_TIME_MS'):vial_source.index('#include "qmk_settings.h"')]+function('quantum/vial.c','vial_unlock_task')
stubs+='\nvoid raw_hid_receive_kb(uint8_t*,uint8_t);\nuint16_t vial_keycode_firewall(uint16_t);\n'
stubs+='static void reload_combo(void){} static void reload_key_override(void){} static void reload_alt_repeat_key(void){}\n'
# Explicit fail-fast seams, never silently answer settings with a fabricated success.
stubs+='static void qmk_settings_query(uint16_t x,void*p,size_t n){(void)x;(void)p;(void)n;fprintf(stderr,"UNIMPLEMENTED QMK settings descriptor closure\\n");abort();}\nstatic int qmk_settings_get(uint16_t x,void*p,size_t n){(void)x;(void)p;(void)n;abort();}\nstatic int qmk_settings_set(uint16_t x,const void*p,size_t n){(void)x;(void)p;(void)n;abort();}\nstatic void qmk_settings_reset(void){abort();}\n'
enums='\n'.join(re.findall(r'enum via_(?:command_id|keyboard_value_id)\s*\{.*?\};',read('quantum/via.h'),re.S))+'\n'+'\n'.join(re.findall(r'enum\s*\{.*?\};',vh,re.S)[:2])+'\n'
constants='\n'.join(line for f in ['quantum/vial.h','quantum/send_string/send_string_keycodes.h'] for line in read(f).splitlines() if re.match(r'#define (?:SS_QMK_PREFIX|SS_TAP_CODE|SS_DOWN_CODE|SS_UP_CODE|SS_DELAY_CODE|VIAL_MACRO_EXT_)',line))+'\n'
backend=''
backend+='#include "macro_storage_flash.c"\n#include "macro_storage.c"\n'
backend+='#include "flash_guard.c"\n'
send=read(dk);send=send[send.index('static uint16_t decode_keycode'):]
# Macro playback closure ends at EOF in both snapshots.
text=pre[:pre.index('uint32_t sm62_flash_test_time_us')]+config+pre[pre.index('uint32_t sm62_flash_test_time_us'):]+enums+backend+nvm+stubs+unlock+wrappers+constants+send+function('quantum/vial.c','vial_keycode_firewall')+function('quantum/vial.c','vial_handle_cmd')
via=read('quantum/via.c')
if 'sm62_packet_mutates_storage' in via:text+=function('quantum/via.c','sm62_packet_mutates_storage')
text+=function('quantum/via.c','raw_hid_receive')+(P/'main.c').read_text()
(O/'transport.c').write_text(text)
# Resident parser/callback separate TU avoids global-symbol collisions; dispatcher
# links to the actual keyboard callback rather than a routing peer model.
h=read('keyboards/ergohaven/hid.c');prod=h[h.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):h.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
prefix=(P/'resident_prefix.c').read_text()
cb=h[h.rindex('void raw_hid_receive_kb('):h.rindex('\nvoid keyboard_post_init_hid')]
resident=prefix+prod+'\n#define EH_APP_LAYOUT_ENABLE\n#define VIAL_HID_MAGIC 0xa3fb1cbc0abd6125ull\nvoid raw_hid_send(uint8_t*,unsigned);\nstatic bool sm62_diag_raw(uint8_t*p,uint8_t n){(void)p;(void)n;return false;}\nstatic bool process_via_custom_lighting(uint8_t*p,uint8_t n){(void)p;(void)n;return false;}\nstatic bool process_raw_hid_data(uint8_t*p,uint8_t n){(void)p;(void)n;return false;}\n'+cb+(P/'resident_boundary.c').read_text()
(O/'resident.c').write_text(resident)
(O/'hardware').mkdir(exist_ok=True)
(O/'hardware/flash.h').write_text('#pragma once\n#include <stdint.h>\n#define FLASH_SECTOR_SIZE 4096u\n#define FLASH_PAGE_SIZE 256u\n#define XIP_BASE ((uintptr_t)0x10000000u)\nvoid flash_range_erase(uint32_t,unsigned);\nvoid flash_range_program(uint32_t,const uint8_t*,unsigned);\n')
(O/'hardware/sync.h').write_text('#pragma once\n#include <stdint.h>\nuint32_t save_and_disable_interrupts(void);\nvoid restore_interrupts(uint32_t);\n')
(O/'compiler_support.h').write_text('#pragma once\n#define STATIC_ASSERT(c,m) _Static_assert(c,m)\n')
flags=['-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-DSM62_FLASH_NATIVE_TEST','-DKEYBOARD_ergohaven_sm62','-DPICO_FLASH_SIZE_BYTES=4194304','-DWEAR_LEVELING_BACKING_SIZE=16384','-DSM62_CODE_FLASH_LIMIT=0x1f2000','-I'+str(O),'-I'+str(S),'-I'+str(K),'-I'+str(S/'keyboards/ergohaven'),'-I'+str(S/'platforms/chibios/drivers/wear_leveling')]
flags+=['-DSM62_OLD_RENDERER=1']
cmd=['gcc',*flags,str(O/'transport.c'),str(O/'resident.c'),str(K/'flash_gate.c'),str(K/'vial_extension.c'),str(K/'vial_extension_flash.c'),str(K/'resident_cache_flash.c'),'-o',str(O/'firmware-bridge')]
r=subprocess.run(cmd,capture_output=True,text=True);(O/'compile.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);assert r.returncode==0
deps=subprocess.check_output([*cmd[:1],*flags,'-MM',str(O/'transport.c'),str(O/'resident.c'),str(K/'flash_gate.c'),str(K/'vial_extension.c'),str(K/'vial_extension_flash.c'),str(K/'resident_cache_flash.c')],text=True)
for token in deps.replace('\\\n',' ').split():
 p=Path(token)
 if p.is_absolute() and p.is_file():
  try: rel=str(p.relative_to(S))
  except ValueError: continue
  digest=hashlib.sha256(p.read_bytes()).hexdigest()
  assert rel not in used or used[rel]==digest, 'source drift: '+rel
  used[rel]=digest
for p in P.iterdir():
 if p.is_file(): used[str(p.relative_to(S))]=hashlib.sha256(p.read_bytes()).hexdigest()
manifest={'source':str(S),'controller':'SM62 SINGLE8 legacy109/6066 TD0','source_hashes':used,'command':cmd,'executable':str(O/'firmware-bridge'),'sha256':hashlib.sha256((O/'firmware-bridge').read_bytes()).hexdigest()}
(O/'build.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest))
