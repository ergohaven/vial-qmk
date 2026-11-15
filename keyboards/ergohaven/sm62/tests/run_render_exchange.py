#!/usr/bin/env python3
"""Production snapshot capture/exchange with synthetic native-state readers.

Backend work is counted, not timed. No rendering, UART or USB latency proof.
"""
import argparse
import re
import resource
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def strip(text):
    return "\n".join(line for line in text.splitlines() if not re.match(r"\s*#\s*(include|pragma once)", line))


def body(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


hid = (HERE.parent.parent / "hid.h").read_text()
hid = hid[hid.index("typedef struct {"):hid.index("hid_data_t* get_hid_data")]
header = '''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_exchange.h"
#define SM62_RENDER_NATIVE_TEST
#define SM62_DUALCORE_RENDER
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
typedef struct {uint16_t qsid;} qmk_settings_proto_t;
'''+hid+strip((HERE.parent / "sm62_settings.h").read_text())+'\n'+strip((HERE.parent / "render_owner.h").read_text())
fixture = r'''
static uint16_t code=7;
static uint32_t clock_ms, native_reads, effective_reads;
static hid_data_t hid;
static sm62_side_settings_t settings;
bool sm62_render_test_owner(void) { return false; }
void sm62_render_test_lock(void) {}
void sm62_render_test_unlock(void) {}
void backlight_init(void) {}
void backlight_level_noeeprom(uint8_t x) { (void)x; }
void sm62_dc_trace_key(void) {}
static uint32_t timer_read32(void) { return clock_ms++; }
static bool is_keyboard_left(void) { return true; }
static bool is_keyboard_master(void) { return true; }
static uint8_t get_current_layer(void) { return 0; }
static uint8_t get_cur_lang(void) { return 0; }
static hid_data_t *get_hid_data(void) { return &hid; }
static bool is_hid_active(void) { return true; }
static bool is_hid_layout_valid(void) { return true; }
static bool is_hid_time_active(void) { return true; }
static bool is_hid_volume_active(void) { return true; }
const sm62_side_settings_t *sm62_local_display_settings(void) { return &settings; }
const sm62_side_settings_t *sm62_remote_display_settings(void) { return &settings; }
bool sm62_local_date_enabled(void) { return true; }
bool sm62_local_language_enabled(void) { return true; }
bool sm62_local_stars_enabled(void) { return true; }
bool sm62_local_screens_off(void) { return false; }
bool sm62_local_mode_transition_pending(void) { return false; }
uint16_t sm62_local_keymap_hold_ms(void) { return 200; }
uint32_t sm62_local_orbit_idle_ms(uint32_t n) { return n; }
static bool eh_keycode_str_uses_mac_modifiers(void) { return false; }
static uint32_t last_matrix_activity_time(void) { return 0; }
static uint32_t last_matrix_activity_elapsed(void) { return 0; }
uint32_t sm62_display_boot_elapsed(void) { return clock_ms; }
static uint8_t get_split_lcd_brightness(void) { return 10; }
static uint32_t get_split_lcd_timeout_ms(void) { return 600000; }
static uint8_t matrix_get_row(uint8_t r) { (void)r; return 0; }
static const char *sm62_effective_layer_name(uint8_t l) { (void)l; return "fixture"; }
static uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c) {
    (void)l;(void)r;(void)c; native_reads++; return code;
}
static uint16_t sm62_effective_keycode(uint8_t l,uint8_t r,uint8_t c) {
    (void)l;(void)r;(void)c; effective_reads++; return code;
}
static bool hid_app_layout_get_visual(uint8_t l,uint8_t c,uint8_t *v,uint8_t *s) {
    (void)l;(void)c;*v=1;*s=2;return true;
}
static uint32_t hid_app_layout_visual_generation(void) { return 1; }
static uint8_t get_display_accent_red(void) { return 1; }
static uint8_t get_display_accent_green(void) { return 2; }
static uint8_t get_display_accent_blue(void) { return 3; }
static uint8_t get_display_background_red(void) { return 4; }
static uint8_t get_display_background_green(void) { return 5; }
static uint8_t get_display_background_blue(void) { return 6; }
static void nvm_dynamic_keymap_erase(void) { code=0; }
static uint16_t keycode_at_keymap_location_raw(unsigned l,unsigned r,unsigned c) { (void)l;(void)r;(void)c; return 15; }
static void nvm_dynamic_keymap_update_keycode(uint8_t l,uint8_t r,uint8_t c,uint16_t k) {
    (void)l;(void)r;(void)c;code=k;
}
static void nvm_dynamic_keymap_update_buffer(uint16_t o,uint16_t n,uint8_t *b) {
    (void)o;if(n)code=*b;
}
'''
owner = (HERE.parent / "render_owner.c").read_text().split("/* Renderers may clear")[0]
owner = strip(owner)
if "void sm62_render_keymap_changed(" not in owner:
    owner += "\nvoid sm62_render_keymap_changed(void) {}\n"
dynamic = (ROOT / "quantum/dynamic_keymap.c").read_text()
mutations = body(dynamic, "void dynamic_keymap_set_keycode(")+"\n"+body(dynamic, "void dynamic_keymap_set_buffer(")
reset_start = dynamic.index("    // Erase the keymaps, if necessary.")
reset_end = dynamic.index("#ifdef SM62_VIAL_LEGACY_EXTENSION\n#if defined(KEYBOARD_ergohaven_sm62)\n    if (sm62_flash_ok()", reset_start)
mutations += "\nvoid dynamic_keymap_reset(void) {\n"+dynamic[reset_start:reset_end]+"\n}\n"
cases = r'''
static void step(void) {
    uint32_t n=native_reads, e=effective_reads;
    sm62_render_capture();
    assert(native_reads-n<=8 && effective_reads-e<=8);
}
static sm62_render_snapshot_t *publish(void) {
    uint32_t before=dc_exchange.written;
    for (unsigned i=0; dc_exchange.written==before; i++) { assert(i<1000); step(); }
    sm62_render_snapshot_t *s=sm62_exchange_adopt(&dc_exchange);
    assert(s);
    for (unsigned l=0;l<16;l++) for(unsigned r=0;r<10;r++) for(unsigned c=0;c<7;c++) {
        assert(s->native_codes[l][r][c]==code && s->effective_codes[l][r][c]==code);
    }
    return s;
}
int main(void) {
    sm62_render_snapshot_t *first=publish();
    /* A consumer lease stays immutable, even while the next slot is prepared. */
    sm62_render_snapshot_t saved=*first;
    uint32_t before=dc_exchange.written;
    while(dc_exchange.written==before) step();
    assert(!memcmp(first,&saved,sizeof(saved)));
    uint32_t n=native_reads;
    for(unsigned i=0;i<10000;i++) step();
    assert(native_reads==n); /* full slots never fetch or wait */
    /* Keep the original cached generation while the mutation counter wraps. */
    dc_keymap_generation=UINT32_MAX;
    dynamic_keymap_set_keycode(0,0,0,17);
    sm62_render_input(1,2,true); sm62_render_input(1,2,false);
    assert(sm62_exchange_adopt(&dc_exchange));
    sm62_render_snapshot_t *s=publish();
    assert(s->presses==(UINT64_C(1)<<9) && s->releases==s->presses);
    assert(!memcmp(&s->matrix,(uint8_t[10]){0},10));
    /* A Vial single-key edit midway through preparation cannot mix generations. */
    dynamic_keymap_set_keycode(0,0,0,9);
    for(unsigned i=0;i<4;i++) step();
    dynamic_keymap_set_keycode(0,0,0,11);
    publish(); publish();
    uint8_t bytes[]={13}; dynamic_keymap_set_buffer(0,1,bytes); publish();
    /* Native reset uses the same production setter for every physical cell. */
    dynamic_keymap_reset();
    publish();
    puts("PASS bounded capture, full-slot input latches, immutable leases and Vial edits");
}
'''
generated = args.output / "exchange.c"
generated.write_text(header+fixture+owner+mutations+cases)
command = ["cc", "-std=gnu11", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
           "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I"+str(HERE.parent),
           str(generated), str(HERE.parent / "render_exchange.c"), "-o", str(args.output / "exchange")]
subprocess.run(command, check=True)
subprocess.run([str(args.output / "exchange")], check=True)
for name, signature in [("single-key", "void dynamic_keymap_set_keycode("),
                        ("buffer", "void dynamic_keymap_set_buffer(")]:
    original = body(mutations, signature)
    assert "sm62_render_keymap_changed();" in original
    broken = original.replace("sm62_render_keymap_changed();", "/* deliberately omitted */")
    generated.write_text(header+fixture+owner+mutations.replace(original, broken)+cases)
    subprocess.run(command, check=True)
    result = subprocess.run([str(args.output / "exchange")], capture_output=True, text=True)
    (args.output / (name+"-negative.log")).write_text(result.stdout+result.stderr)
    assert result.returncode == -6, (name, result.returncode, result.stdout, result.stderr)
    print("PASS negative control:", name, "missing invalidation rejected")
generated.write_text(header+fixture+owner+mutations+cases)
subprocess.run(command, check=True)
