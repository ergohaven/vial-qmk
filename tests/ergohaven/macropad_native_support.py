"""Shared native harness prerequisites; never initializes QMK submodules."""
import hashlib
import subprocess

def validate_lvgl(lvgl, *, profile='base'):
    assert profile in {'base', 'cooperative'}, f'unknown LVGL profile: {profile}'
    pinned = 'e19410f8f8a256609da72cff549598e0df6fa4cf'
    from pathlib import Path
    import json
    fixture = Path(__file__).with_name('sm62-lvgl-blobs.json')
    if fixture.exists():
        manifest = json.loads(fixture.read_text())
        assert manifest['revision'] == pinned
        assert len(manifest['blobs']) == 606
        overlay_path = fixture.with_name('sm62-lvgl-cooperative-blobs.json')
        overlay = json.loads(overlay_path.read_text()) if profile == 'cooperative' else {}
        assert profile != 'cooperative' or set(overlay) == {'src/core/lv_refr.c', 'src/core/lv_refr.h', 'src/hal/lv_hal_disp.c', 'src/hal/lv_hal_disp.h', 'src/misc/lv_timer.c'}
        for filename, oid in manifest['blobs'].items():
            if filename in overlay:
                assert overlay[filename]['base'] == oid
                oid = overlay[filename]['patched']
            data = (lvgl/filename).read_bytes()
            actual = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            assert actual == oid, f'LVGL source differs from pinned blob: {filename}'
        print('LVGL source: 606 C/header blobs verified (pinned base' + (' + exact 5-file cooperative/cache overlay)' if overlay else ')'), flush=True)
        return
    assert profile == 'base', 'cooperative validation requires the exact overlay manifest'
    tree = subprocess.check_output(['git', '-C', str(lvgl), 'ls-tree', '-rz', '--full-tree', pinned])
    checked = 0
    for entry in tree.split(b'\0'):
        if not entry:
            continue
        metadata, filename = entry.split(b'\t')
        _, kind, oid = metadata.split()
        if kind != b'blob' or not filename.endswith((b'.c', b'.h')):
            continue
        data = (lvgl/filename.decode()).read_bytes()
        actual = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        assert actual == oid.decode(), f'LVGL source differs from pinned revision: {filename!r}'
        checked += 1
    assert checked == 606, f'expected 606 pinned LVGL blobs, got {checked}'
    print(f'LVGL pinned source: {checked} C/header blobs verified', flush=True)

def write_stubs(stubs):
    stubs.mkdir(parents=True, exist_ok=True)
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
