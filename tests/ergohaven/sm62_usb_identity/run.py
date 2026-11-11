#!/usr/bin/env python3
"""Focused source-backed USB identity qualification; no ARM codegen/Cargo.

Run via agent-storage. --cflags is the NORMAL build's saved cflags.txt;
--build-directory replaces its expired /proc/<pid>/fd/4 alias. Only the flash
unique-ID hardware boundary is faked; formatter/provider are production source.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--cflags', required=True, type=Path)
p.add_argument('--build-directory', required=True)
p.add_argument('--compiler', required=True)
p.add_argument('--host-source', required=True, type=Path)
p.add_argument('--phase', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
out = Path(os.environ['AGENT_BUILD_CACHE']) / 'sm62-usb-identity' / a.phase
out.mkdir(parents=True, exist_ok=True)
results = []
def run(cmd, name, cwd=root):
    r = subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + '.log')).write_text(r.stdout)
    results.append({'name': name, 'command': [str(x) for x in cmd], 'exit': r.returncode})
    print(name, 'exit', r.returncode, r.stdout[-1800:])
    return r

flags = shlex.split(a.cflags.read_text())
flags = [re.sub(r'/proc/\d+/fd/4', a.build_directory, x) for x in flags]
source = root / 'tmk_core/protocol/usb_descriptor.c'
pp = run([a.compiler, *flags, '-E', '-P', str(source)], 'normal-preprocess')
mac = run([a.compiler, *flags, '-E', '-dM', str(source)], 'normal-macros')
if pp.returncode or mac.returncode:
    raise SystemExit('normal preprocessing failed, not behavioral RED')
macros = dict(re.findall(r'^#define (\w+) (.*)$', mac.stdout, re.M))
selected = 'void set_serial_number_descriptor(void)' in pp.stdout
proof = {k: macros.get(k) for k in ['SERIAL_NUMBER', 'SERIAL_NUMBER_USE_HARDWARE_ID', 'SERIAL_NUMBER_PREFIX']}
proof['hardware_formatter_selected'] = selected
print('NORMAL effective identity:', json.dumps(proof))
(out / 'normal-identity.json').write_text(json.dumps(proof, indent=2))
results.append({'name': 'normal-hardware-selection', 'exit': 0 if selected and macros.get('SERIAL_NUMBER_PREFIX') == '"vial:f64c2b3c:"' and 'SERIAL_NUMBER' not in macros else 1})

text = source.read_text()
serial = text[text.index('#if defined(SERIAL_NUMBER)\n// clang-format off'):text.index('/**\n * This function is called by the library when in device mode')]
(out / 'serial-production.inc').write_text(serial)
(out / 'ch.h').write_text('/* No hardware needed by RP2040 hardware_id.c beyond uint8_t. */\n')
common = '''#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "hardware_id.h"
#define TRUE 1
#define PROGMEM
#define DTYPE_String 3
#define MIN(a,b) ((a)<(b)?(a):(b))
#define USBSTR_IMPL(s) L##s
#define USBSTR(s) USBSTR_IMPL(s)
#define USB_DESCRIPTOR_SIZE_LITERAL_U16STRING(s) (sizeof(USB_Descriptor_Header_t) + sizeof(s) - sizeof(wchar_t))
typedef struct { uint8_t Size, Type; } USB_Descriptor_Header_t;
typedef struct { USB_Descriptor_Header_t Header; uint16_t UnicodeString[128]; } USB_Descriptor_String_t;
static uint8_t fake_id[8];
void flash_get_unique_id(uint8_t *p) { memcpy(p, fake_id, 8); }
'''
main = '''
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    for (int i=0; i<8; ++i) { unsigned x; if (sscanf(argv[1]+i*2, "%2x", &x)!=1) return 2; fake_id[i]=(uint8_t)x; }
#if !defined(SERIAL_NUMBER)
    set_serial_number_descriptor();
#endif
    const USB_Descriptor_String_t *d=(const void *)&SerialNumberString;
    size_t n=strlen(argv[2]);
    if (d->Header.Size != 2+n*2 || d->Header.Type != DTYPE_String) { fprintf(stderr,"descriptor length/type mismatch: %u expected %zu\\n",d->Header.Size,2+n*2); return 1; }
    for (size_t i=0;i<n;++i) if (d->UnicodeString[i]!=(uint8_t)argv[2][i]) { fprintf(stderr,"serial mismatch at %zu\\n",i); return 1; }
#if !defined(SERIAL_NUMBER)
    if (d->UnicodeString[n] != 0) return 1;
    uint8_t before[sizeof(SerialNumberString)]; memcpy(before,SerialNumberString,sizeof before);
    memset(fake_id,0xff,sizeof fake_id); set_serial_number_descriptor();
    if (memcmp(before,SerialNumberString,sizeof before)) return 1;
#endif
    printf("serial=%s chars=%zu descriptor_bytes=%u termination/determinism=PASS\\n",argv[2],n,d->Header.Size);
    return 0;
}
'''
for mode in ['normal', 'prefix-disabled']:
    defines = ''
    if mode == 'normal':
        for k in ['SERIAL_NUMBER', 'SERIAL_NUMBER_USE_HARDWARE_ID', 'SERIAL_NUMBER_PREFIX']:
            if k in macros: defines += '#define '+k+' '+macros[k]+'\n'
    else: defines = '#define SERIAL_NUMBER_USE_HARDWARE_ID TRUE\n'
    harness = out / (mode+'.c')
    harness.write_text(common+defines+'#include "serial-production.inc"\n'+main)
    binary = out / mode
    r=run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fshort-wchar','-DRP2040','-I'+str(out),'-I'+str(root/'platforms'),str(harness),str(root/'platforms/chibios/hardware_id.c'),'-o',str(binary)],mode+'-compile')
    if r.returncode: raise SystemExit('native compilation failure, not behavioral RED')
    for ident in ['0123456789ABCDEF', 'FEDCBA9876543210']:
        expected=('vial:f64c2b3c:' if mode=='normal' else '')+ident+'0000000000000000'
        run([str(binary),ident,expected],mode+'-'+ident)

# Extract the actual production key method, retaining its guards; unrelated HID
# transport is a small data boundary so this does not use shared Cargo outputs.
device=(a.host_source/'src/device.rs').read_text()
start=device.index('    pub(crate) fn sm62_host_rule_key(')
end=device.index('\n    pub(crate) fn application_layout_storage_key(',start)
method=device[start:end]
tests=(a.host_source/'src/device_usb_identity_tests.rs').read_text()
rust='''#![allow(dead_code)]
const ERGOHAVEN_VENDOR_ID:u16=0xE126;
const SM62_PRODUCT_ID:u16=0x0062;
fn test_device(_bus:&str,_path:&str)->Device { Device { vendor_id:0,product_id:0,serial_number:String::new(),firmware:FirmwareProtocol::Vial,bluetooth:false } }
#[derive(PartialEq)] enum FirmwareProtocol { Vial, Other }
struct Device { vendor_id:u16, product_id:u16, serial_number:String, firmware:FirmwareProtocol, bluetooth:bool }
impl Device {
fn is_ergohaven_sm62(&self)->bool { self.vendor_id==0xE126 && self.product_id==0x0062 }
fn is_bluetooth_transport(&self)->bool { self.bluetooth }
'''+method+'\n}\n'+tests
(out/'host.rs').write_text(rust)
r=run(['rustc','--edition=2021','--test',str(out/'host.rs'),'-o',str(out/'host-tests')],'host-compile')
if r.returncode: raise SystemExit('host compile failure, not behavioral RED')
run([str(out/'host-tests'),'--nocapture'],'host-tests')
manifest={'phase':a.phase,'normal_identity':proof,'results':results,'source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [source,root/'keyboards/ergohaven/sm62/config.h',root/'platforms/chibios/hardware_id.c',a.host_source/'src/device.rs']}}
(out/'result.json').write_text(json.dumps(manifest,indent=2))
print('RESULT',out/'result.json')
raise SystemExit(1 if any(x['exit'] for x in results) else 0)
