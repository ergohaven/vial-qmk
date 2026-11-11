#!/usr/bin/env python3
"""Production format + real backend/guard + actual NVM/dispatcher, NOR/MMIO only substituted."""
from pathlib import Path
import os,re,subprocess,json,sys,hashlib
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', required=True, type=Path)
parser.add_argument('--output-dir', required=True, type=Path)
parser.add_argument('--legacy-abort-only', action='store_true', help='Run only the legacy interrupted-import regression; default retains all campaigns')
a = parser.parse_args()
if sys.flags.optimize:
 parser.error('Python assertions must be enabled (no -O / PYTHONOPTIMIZE)')
R = Path(__file__).resolve().parent
S = a.source_root.resolve()
if not S.is_dir():
 parser.error('--source-root must be an existing QMK source directory')
if not a.output_dir.is_absolute():
 parser.error('--output-dir must be absolute')
O = a.output_dir.resolve()
# Resolve symlinks before containment checks, including ancestors of a fresh path.
protected = {S, R}
for base in (S, R):
 for ancestor in (base, *base.parents):
  if (ancestor / '.git').exists():
   protected.add(ancestor)
   break
if any(O == p or p in O.parents or O in p.parents for p in protected):
 parser.error('--output-dir must be outside source repository/package and cannot contain them')
if os.path.lexists(a.output_dir) or O.exists() or not O.parent.is_dir():
 parser.error('--output-dir must be fresh, with an existing parent')
# Strict qualification fixture: fail before output creation if selected SINGLE
# declarations/configuration differ. These pins do NOT derive expected values
# from dispatcher replies or the journal under test.
pins = json.loads((R / 'single_config.json').read_text())
identity = {}
try:
 for name, expected in pins.items():
  path, _, section = name.partition(':')
  data = (S / path).read_bytes()
  if section:
   data = data.split(b'void nvm_dynamic_keymap_erase(void)')[0]
  actual = hashlib.sha256(data).hexdigest()
  if actual != expected:
   parser.error('unsupported SINGLE configuration/declarations: ' + name)
  identity[name] = actual
except OSError as e:
 parser.error(str(e))
# Native dispatcher geometry is the real split matrix, including column 6.
geometry = json.loads((S / 'keyboards/ergohaven/sm62/keyboard.json').read_text())
assert len(geometry['matrix_pins']['rows']) * 2 == 10
assert len(geometry['matrix_pins']['cols']) == 7
# Record every direct production input, plus transitive headers in this boundary.
inputs = [S / p for p in pins if ':' not in p]
inputs += list((S / 'keyboards/ergohaven/sm62').glob('*.h'))
inputs += [S / p for p in (
 'quantum/via.h', 'quantum/via.c', 'quantum/vial.c', 'quantum/keycodes.h',
 'quantum/dynamic_keymap.c', 'quantum/nvm/eeprom/nvm_dynamic_keymap.c',
 'quantum/send_string/send_string_keycodes.h',
 'keyboards/ergohaven/sm62/macro_storage.c',
 'keyboards/ergohaven/sm62/macro_storage_flash.c',
 'keyboards/ergohaven/sm62/flash_guard.c',
 'keyboards/ergohaven/sm62/flash_gate.c')]
try:
 production_before = {str(p.relative_to(S)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
except OSError as e:
 parser.error(str(e))
O.mkdir()  # first write, only after all path/config/input validation
(O / 'source-identity.json').write_text(json.dumps({
 'source_root': str(S), 'production_sha256': production_before,
 'package_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in R.iterdir() if p.is_file()},
 'selected_config_sha256': identity,
}, indent=2) + '\n')

K=S/'keyboards/ergohaven/sm62'
def function(path,name):
 s=path.read_text();m=re.search(r'^(?:static )?(?:inline )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 i=s.index('{',m.start());d=1;e=i+1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
(O/'hardware').mkdir(exist_ok=True)
(O/'hardware/flash.h').write_text('#pragma once\n#include <stdint.h>\n#define FLASH_SECTOR_SIZE 4096u\n#define FLASH_PAGE_SIZE 256u\n#define XIP_BASE 0x10000000u\nvoid flash_range_erase(uint32_t,unsigned);\nvoid flash_range_program(uint32_t,const uint8_t*,unsigned);\n')
(O/'hardware/sync.h').write_text('#pragma once\n#include <stdint.h>\nuint32_t save_and_disable_interrupts(void);\nvoid restore_interrupts(uint32_t);\n')
(O/'compiler_support.h').write_text('#pragma once\n#define STATIC_ASSERT(c,m) _Static_assert(c,m)\n')
# Actual public command IDs from the same production header, no copied numbers.
vh=(S/'quantum/via.h').read_text();enums='\n'.join(re.findall(r'enum via_(?:command_id|keyboard_value_id)\s*\{.*?\};',vh,re.S))
enums+='\n'+'\n'.join(re.findall(r'enum\s*\{.*?\};',(S/'quantum/vial.h').read_text(),re.S)[:2])
pre='''#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <setjmp.h>
#include "macro_storage.h"
#include "flash_guard.h"
#include "vial_extension.h"
#define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR 2968u
#define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE 5224u
#define DYNAMIC_KEYMAP_MACRO_COUNT 109u
#define VIA_PROTOCOL_VERSION 9
#define VIA_FIRMWARE_VERSION 0
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define VIAL_HID_MAGIC 0xa3fb1cbc0abd6125ull
static uint8_t eeprom[8192],original[8192],reply[32];
static bool vial_unlocked=true,vial_unlock_in_progress;
static unsigned sent,io_calls,irq_depth,events,bank_reads,output_chars;
static int cut=-1,fail_call=-1,silent_call=-1,read_fail=-1,read_error_at=-1;
static bool context_ok=true;
static jmp_buf powercut;
uint32_t sm62_flash_test_time_us(void){return 0;}
bool sm62_flash_test_context_valid(void){return context_ok;}
static uint8_t eeprom_read_byte(const void *a){unsigned o=(uintptr_t)a;assert(o<8192);return eeprom[o];}
uint32_t save_and_disable_interrupts(void){assert(!irq_depth);irq_depth=1;return 0;}
void restore_interrupts(uint32_t i){assert(i==0 && irq_depth==1);irq_depth=0;}
static void step(void){if(cut>=0 && events++==(unsigned)cut)longjmp(powercut,1);}
void flash_range_erase(uint32_t off,unsigned n){
 assert(irq_depth==1 && n==4096 && off>=SM62_MACRO_FLASH_BASE && off+n<=SM62_MACRO_FLASH_BASE+2*SM62_MACRO_BANK_SIZE && off%4096==0);
 unsigned call=io_calls++;if((int)call==silent_call)return;
 for(unsigned i=0;i<n;i++){step();((uint8_t*)0x10000000)[off+i]=255;}
}
void flash_range_program(uint32_t off,const uint8_t*p,unsigned n){
 assert(irq_depth==1 && n==256 && off>=SM62_MACRO_FLASH_BASE && off+n<=SM62_MACRO_FLASH_BASE+2*SM62_MACRO_BANK_SIZE && off%256==0);
 assert((uintptr_t)p<0x10000000 || (uintptr_t)p>=0x10400000); // SRAM, never XIP
 unsigned call=io_calls++;if((int)call==silent_call)return;
 for(unsigned i=0;i<n;i++){step();((uint8_t*)0x10000000)[off+i]&=p[i];}
}
'''
# Native fault interceptors call the REAL production backend for every admitted I/O.
backend='''
#define sm62_macro_flash_bank macro_hw_bank
#define sm62_macro_flash_erase macro_hw_erase
#define sm62_macro_flash_program macro_hw_program
#include "macro_storage_flash.c"
#undef sm62_macro_flash_bank
#undef sm62_macro_flash_erase
#undef sm62_macro_flash_program
const uint8_t *sm62_macro_flash_bank(unsigned b){unsigned call=bank_reads++;return read_fail==(int)b || read_error_at==(int)call?0:macro_hw_bank(b);}
bool sm62_macro_flash_erase(unsigned b){return fail_call==(int)io_calls?false:macro_hw_erase(b);}
bool sm62_macro_flash_program(unsigned b,unsigned o,const uint8_t*p){return fail_call==(int)io_calls?false:macro_hw_program(b,o,p);}
#include "macro_storage.c"
#include "flash_guard.c"
'''
nvm=S/'quantum/nvm/eeprom/nvm_dynamic_keymap.c';dk=S/'quantum/dynamic_keymap.c';via=S/'quantum/via.c'
bridge='\n'.join(function(nvm,n) for n in ['sm62_macro_legacy_read','nvm_dynamic_keymap_macro_size','nvm_dynamic_keymap_macro_read_buffer','nvm_dynamic_keymap_macro_update_buffer','nvm_dynamic_keymap_macro_reset','nvm_dynamic_keymap_macro_erase'])
bridge+='\n'.join(function(dk,n) for n in ['dynamic_keymap_macro_get_count','dynamic_keymap_macro_get_buffer_size','dynamic_keymap_macro_get_buffer','dynamic_keymap_macro_set_buffer','dynamic_keymap_macro_reset'])
stubs='''
typedef uint16_t matrix_row_t;
static matrix_row_t matrix_get_row(unsigned r){(void)r;return 0;}
static uint32_t timer_read32(void){return 0;}
static uint32_t via_get_layout_options(void){return 0;}
static void via_set_layout_options(uint32_t x){(void)x;assert(0);}
static void raw_hid_receive_kb(uint8_t*p,unsigned n){(void)p;(void)n;assert(0);}
static void raw_hid_send(uint8_t*p,unsigned n){assert(n==32);memcpy(reply,p,32);sent++;}
static uint16_t dynamic_keymap_get_keycode(unsigned l,unsigned r,unsigned c){(void)l;(void)r;(void)c;assert(0);return 0;}
static void dynamic_keymap_set_keycode(unsigned l,unsigned r,unsigned c,unsigned x){(void)l;(void)r;(void)c;(void)x;assert(0);}
bool dynamic_keymap_reset_checked(void){assert(0);return false;}
static void via_eeprom_set_valid(bool v){(void)v;assert(0);}
static unsigned dynamic_keymap_get_layer_count(void){return 16;}
static void dynamic_keymap_get_buffer(unsigned o,unsigned n,uint8_t*p){(void)o;(void)n;(void)p;assert(0);}
static void dynamic_keymap_set_buffer(unsigned o,unsigned n,uint8_t*p){(void)o;(void)n;(void)p;assert(0);}
static void wait_ms(unsigned x){(void)x;assert(0);}
static void bootloader_jump(void){assert(0);}
static const uint8_t keyboard_definition[32]={0};
static uint8_t vial_unlock_counter;
static bool vial_unlock_holding;
#define VIAL_RAW_EPSIZE 32
#define VIAL_KEYBOARD_UID {1,2,3,4,5,6,7,8}
#define VIAL_PROTOCOL_VERSION 6
#define VIAL_UNLOCK_NUM_KEYS 2
#define VIAL_UNLOCK_COUNTER_MAX 50
#define VIAL_TAP_DANCE_ENTRIES 0
#define VIAL_COMBO_ENTRIES 32
#define VIAL_KEY_OVERRIDE_ENTRIES 32
#define VIAL_ALT_REPEAT_KEY_ENTRIES 32
#define memcpy_P memcpy
static void vial_get_unlock_combo_coords(uint8_t*r,uint8_t*c,unsigned n){(void)r;(void)c;(void)n;assert(0);}
static void send_string(const char*p){(void)p;assert(0);}
static void send_string_with_delay(const char*p,unsigned n){assert(n==1 && strlen(p)==1);output_chars++;}
static void vial_keycode_tap(uint16_t k){(void)k;assert(0);}
static void vial_keycode_down(uint16_t k){(void)k;assert(0);}
static void vial_keycode_up(uint16_t k){(void)k;assert(0);}
#define DYNAMIC_KEYMAP_MACRO_DELAY 1

'''
# This includes the COMPLETE raw_hid_receive switch and its real mutation classifier.
constants='\n'.join(line for f in [S/'quantum/vial.h',S/'quantum/send_string/send_string_keycodes.h'] for line in f.read_text().splitlines() if re.match(r'#define (?:SS_QMK_PREFIX|SS_TAP_CODE|SS_DOWN_CODE|SS_UP_CODE|SS_DELAY_CODE|VIAL_MACRO_EXT_)',line))+'\n'
send_tail=dk.read_text().split('#ifdef SM62_MACRO_OVERLAY\n#    include "keyboards/ergohaven/sm62/macro_storage.h"',1)[1]
keycodes=(S/'quantum/keycodes.h').read_text()
boot_declarations=[]
for name in ['QK_BOOTLOADER','QK_BOOT']:
 matches=re.findall(r'^\s*'+name+r'\s*=\s*[^,\n]+,',keycodes,re.M)
 assert len(matches)==1, name
 boot_declarations.append(matches[0])
boot_constants='enum {\n'+ '\n'.join(boot_declarations)+'\n};\n'
bridge+=stubs+constants+boot_constants+function(dk,'decode_keycode')+function(dk,'dynamic_keymap_read_byte')+'\n#ifdef SM62_MACRO_OVERLAY\n'+send_tail+'\n'+function(S/'quantum/vial.c','vial_keycode_firewall')+function(S/'quantum/vial.c','vial_handle_cmd')+function(via,'sm62_packet_mutates_storage')+function(via,'raw_hid_receive')
text=pre+enums+backend+bridge+(R/'test_macro_storage.c').read_text()
p=O/'test.c';p.write_text(text)
flags=['-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DSM62_FLASH_NATIVE_TEST','-DSM62_MACRO_OVERLAY','-DSM62_OLD_RENDERER=1','-DKEYBOARD_ergohaven_sm62','-DSM62_VIAL_LEGACY_EXTENSION','-DVIAL_ENABLE','-DPICO_FLASH_SIZE_BYTES=4194304','-DWEAR_LEVELING_BACKING_SIZE=16384','-DSM62_CODE_FLASH_LIMIT=0x1f2000','-I'+str(O),'-I'+str(K),'-I'+str(S/'platforms/chibios/drivers/wear_leveling')]
cmd=['gcc',*flags,str(p),str(K/'flash_gate.c'),'-o',str(O/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);(O/'compile.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);assert r.returncode==0
runs=[]
for args in ([['--legacy-abort-only']] if a.legacy_abort_only else [[],['--save',str(O/'reboot.bin')],['--load',str(O/'reboot.bin')]]):
 r=subprocess.run([str(O/'test'),*args],capture_output=True,text=True);print(r.stdout+r.stderr);runs.append({'args':args,'exit':r.returncode,'output':r.stdout+r.stderr});assert r.returncode==0
result={'command':cmd,'executable_sha256':hashlib.sha256((O/'test').read_bytes()).hexdigest(),'runs':runs}
production_after = {p: hashlib.sha256((S/p).read_bytes()).hexdigest() for p in production_before}
assert production_before == production_after, 'production inputs changed during run'
result['production_unchanged'] = True
(O/'result.json').write_text(json.dumps(result,indent=2)+'\n')
