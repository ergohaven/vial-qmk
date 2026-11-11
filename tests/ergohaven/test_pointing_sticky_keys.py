#!/usr/bin/env python3
"""Exercise production pointing key handlers with host settings/timer substitutes."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"^(?:static )?(?:bool|void|pointing_mode_t) " + name + r"\([^\n]*\) \{", source, re.M)
    if match is None:
        raise ValueError(f"Missing production function: {name}")
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PREAMBLE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define QK_KB 0x7e00
#define QK_USER 0x7e40
#define EH_RSRV2 (QK_KB + 51)
#define EH_RSRV7 (QK_KB + 56)
typedef enum {POINTING_MODE_NORMAL, POINTING_MODE_SNIPER, POINTING_MODE_SCROLL,
              POINTING_MODE_TEXT, POINTING_MODE_USR1, POINTING_MODE_USR2, POINTING_MODE_USR3} pointing_mode_t;
typedef enum {SPLIT_POINTING_SIDE_LEFT, SPLIT_POINTING_SIDE_RIGHT, SPLIT_POINTING_SIDE_COUNT} split_pointing_side_t;
typedef struct { struct { bool pressed; } event; } keyrecord_t;
static bool sticky[2];
static unsigned writes;
static uint16_t now, mode_pressed_at;
static pointing_mode_t pointing_mode;
static bool split_pointing_mode_override_active[2];
static pointing_mode_t split_pointing_mode_override[2];
static bool get_sticky_mode(void) { return sticky[0]; }
static void set_sticky_mode(bool enabled) { sticky[0] = enabled; writes++; }
static bool get_split_pointing_side_sticky_mode(split_pointing_side_t side) { return sticky[side]; }
static void set_split_pointing_side_sticky_mode(split_pointing_side_t side, bool enabled) { sticky[side] = enabled; writes++; }
static void set_pointing_mode(pointing_mode_t mode) { pointing_mode = mode; }
static uint16_t timer_read(void) { return now; }
static uint16_t timer_elapsed(uint16_t start) { return now - start; }
static uint16_t get_tapping_term(uint16_t code, keyrecord_t *record) { (void)code; (void)record; return 200; }
static bool get_led_blinks(void) { return false; }
static void set_led_blinks(bool value) { (void)value; }
'''

SCENARIOS = r'''
static void key(uint16_t code, bool pressed) {
    keyrecord_t record = {.event = {.pressed = pressed}};
    assert(!process_record_pointing(code, &record));
}
int main(void) {
    (void)mode_pressed_at;
    key(EH_STK_ON, true); assert(sticky[0]);
    unsigned count = writes;
    key(EH_STK_ON, false); assert(writes == count);
    key(EH_STK_ON, true); assert(writes == count);
    key(EH_STK_TG, true); assert(!sticky[0]);
    key(EH_STK_TG, false); assert(!sticky[0]);
    key(EH_STK_TG, true); assert(sticky[0]);
    key(EH_STK_OFF, true); assert(!sticky[0]);
    now = 100;
    key(EH_SCR, true); now += 10; key(EH_SCR, false);
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    assert(!split_pointing_mode_override_active[0]);
    key(EH_L_STK_ON, true); assert(sticky[0] && !sticky[1]);
    now += 10; key(EH_L_SCR, true); now += 10; key(EH_L_SCR, false);
    assert(split_pointing_mode_override_active[0]);
    key(EH_R_STK_ON, true); assert(sticky[0] && sticky[1]);
    key(EH_L_STK_OFF, true);
    assert(!sticky[0] && sticky[1] && !split_pointing_mode_override_active[0]);
    key(EH_STK_TG, true); assert(sticky[0] && sticky[1]);
    key(EH_R_STK_TG, true); assert(sticky[0] && !sticky[1]);
    key(EH_STK_ON, true);
    now += 10; key(EH_L_SCR, true); now += 10; key(EH_L_SCR, false);
    now += 10; key(EH_L_SNP, true);
    key(EH_L_STK_OFF, true);
    now += 10; key(EH_L_SNP, false);
    assert(!split_pointing_mode_override_active[0]);
    assert(sticky[1]);
#else
    assert(pointing_mode == POINTING_MODE_NORMAL);
    key(EH_STK_ON, true);
    now += 10; key(EH_SCR, true); now += 10; key(EH_SCR, false);
    assert(pointing_mode == POINTING_MODE_SCROLL);
    key(EH_STK_OFF, true); assert(pointing_mode == POINTING_MODE_NORMAL);
#endif
    puts("PASS sticky setting keys, release/idempotence, and mode reset");
}
'''


class StickyKeys(unittest.TestCase):
    def test_handlers(self):
        source = (ROOT / "keyboards/ergohaven/src/eh_pointing.c").read_text()
        header = (ROOT / "keyboards/ergohaven/src/eh_pointing.h").read_text()
        codes = re.search(r"enum \{\s*EH_L_SNP.*?\n\};", header, re.S).group()
        helpers = "\n".join(function(source, name) for name in (
            "sanitize_split_pointing_mode", "set_split_pointing_side_mode_override",
            "clear_split_pointing_side_mode_override", "restore_split_pointing_side_mode_override",
            "toggle_split_pointing_side_sticky_override"))
        handlers = "\n".join(function(source, name) for name in (
            "process_pointing_sticky_key", "process_record_pointing"))
        for split in (False, True):
            with self.subTest(split=split), tempfile.TemporaryDirectory(prefix="sticky-keys-") as tmp:
                path = Path(tmp)
                (path / "test.c").write_text(PREAMBLE + codes + helpers + handlers + SCENARIOS)
                command = shlex.split(os.environ.get("CC", "cc")) + [
                    "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-Wno-unused-variable", "-fsanitize=undefined", "-fno-sanitize-recover=all"]
                if split:
                    command.append("-DEH_KEYBOARD_SPLIT_POINTING_V2")
                subprocess.run(command + [str(path / "test.c"), "-o", str(path / "test")], check=True)
                subprocess.run([str(path / "test")], check=True)

    def test_catalog_preserves_existing_keycode_slots(self):
        for suffix in ("", "_phenom_micro", "_phenom_rev3"):
            source = '{' + (ROOT / f"keyboards/ergohaven/src/json/keycodes_all{suffix}.json").read_text() + '"end": 0}'
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "catalog.json"
                path.write_text(source)
                result = subprocess.run(["cc", "-E", "-P", "-x", "c", "-I", str(ROOT), str(path)],
                                        capture_output=True, text=True, check=True)
            codes = json.loads(result.stdout)["customKeycodes"]
            self.assertEqual([entry["name"] for entry in codes[58:64]],
                             ["EH_SNP", "EH_SCR", "EH_TXT", "EH_USR1", "EH_USR2", "EH_USR3"])
            self.assertEqual([entry["name"] for entry in codes[64:67]],
                             ["EH_STK_TG", "EH_STK_ON", "EH_STK_OFF"])
            self.assertEqual(len(codes), 73 if suffix else 67)


if __name__ == "__main__":
    unittest.main()
