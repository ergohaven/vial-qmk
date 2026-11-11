#!/usr/bin/env python3
"""Host regression for split RGB layer redraws; run with python3.

Extracts the production receiver and timer task verbatim. Rendering and clock
are spies/stubs; this does not test serial transport, LEDs or firmware builds.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"^void " + name + r"\([^\n]*\) \{", source, re.M)
    if match is None:
        raise ValueError(f"Missing production function: {name}")
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


SUPPORT = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define RGBLIGHT_LAYERS
#define RGBLIGHT_SPLIT
#define RGBLIGHT_USE_TIMER
#define RGBLIGHT_SPLIT_NO_ANIMATION_SYNC
#define RGBLIGHT_STATUS_CHANGE_MODE 1
#define RGBLIGHT_STATUS_CHANGE_HSVS 2
#define RGBLIGHT_STATUS_CHANGE_TIMER 4
#define RGBLIGHT_STATUS_CHANGE_LAYERS 16
/* Host substitutes for QMK configuration/status and animation state. */
typedef struct { bool enable; uint8_t mode, hue, sat, val; } config_t;
typedef struct {
    uint16_t enabled_layer_mask;
    uint8_t change_flags, base_mode;
    bool timer_enabled;
} status_t;
typedef struct { config_t config; status_t status; } rgblight_syncinfo_t;
typedef struct {
    uint16_t last_timer, pos16;
    uint8_t delta;
    bool restart;
} animation_t;
typedef void (*effect_func_t)(animation_t *);
static config_t rgblight_config = {.enable = true, .mode = 1};
static status_t rgblight_status;
static animation_t animation_status;
static bool deferred_set_layer_state;
static uint16_t displayed_mask;
static unsigned redraws;
static void rgblight_set(void) {
    displayed_mask = rgblight_status.enabled_layer_mask;
    ++redraws;
}
static void rgblight_mode_eeprom_helper(uint8_t mode, bool persist) {
    (void)mode; (void)persist;
    if (rgblight_config.enable) rgblight_set();
}
static void rgblight_mode_noeeprom(uint8_t mode) {
    rgblight_mode_eeprom_helper(mode, false);
}
static void rgblight_sethsv_eeprom_helper(uint8_t h, uint8_t s, uint8_t v, bool p) {
    (void)h; (void)s; (void)v; (void)p;
    if (rgblight_config.enable) rgblight_set();
}
static void rgblight_disable_noeeprom(void) { rgblight_config.enable = false; }
static void rgblight_timer_enable(void) { rgblight_status.timer_enabled = true; }
static void rgblight_timer_disable(void) { rgblight_status.timer_enabled = false; }
static uint16_t sync_timer_read(void) { return 0; }
static bool timer_expired(uint16_t now, uint16_t deadline) { return now >= deadline; }
static void rgblight_effect_dummy(animation_t *state) { (void)state; }
'''

SCENARIOS = r'''
static void receive(uint16_t mask, uint8_t flags) {
    rgblight_syncinfo_t packet = {
        .config = {.enable = true, .mode = 1},
        .status = {.enabled_layer_mask = mask, .change_flags = flags},
    };
    rgblight_update_sync(&packet, false);
}
int main(void) {
    /* Enter auto/symbol layers, then return to base without MODE/HSV changes. */
    const uint16_t masks[] = {1u << 4, 1, 1u << 2, 1, 1u << 15, 0};
    for (unsigned i = 0; i < sizeof(masks) / sizeof(masks[0]); ++i) {
        unsigned before = redraws;
        receive(masks[i], RGBLIGHT_STATUS_CHANGE_LAYERS);
        assert(rgblight_status.enabled_layer_mask == masks[i]);
        assert(redraws == before); /* Receiver defers rendering. */
        rgblight_timer_task();
        if (redraws != before + 1 || displayed_mask != masks[i]) {
            fprintf(stderr, "FAIL: layer-only sync did not redraw mask %u\n", masks[i]);
            return 1;
        }
        assert(!deferred_set_layer_state);
        assert(rgblight_status.change_flags == 0); /* No outbound layer echo. */
        rgblight_timer_task();
        assert(redraws == before + 1);
    }
    /* Several packets before the task runs must paint only the newest state. */
    unsigned before = redraws;
    receive(16, RGBLIGHT_STATUS_CHANGE_LAYERS);
    receive(4, RGBLIGHT_STATUS_CHANGE_LAYERS);
    receive(1, RGBLIGHT_STATUS_CHANGE_LAYERS);
    rgblight_timer_task();
    assert(redraws == before + 1 && displayed_mask == 1);
    /* No layer flag must neither replace the mask nor queue a redraw. */
    before = redraws;
    receive(16, 0);
    rgblight_timer_task();
    assert(redraws == before && rgblight_status.enabled_layer_mask == 1);
    receive(4, RGBLIGHT_STATUS_CHANGE_LAYERS | RGBLIGHT_STATUS_CHANGE_MODE);
    rgblight_timer_task();
    assert(displayed_mask == 4 && !deferred_set_layer_state);
    /* Preserve RGB-off behavior, including the explicit layer override. */
    rgblight_config.enable = false;
    before = redraws;
    receive(1, RGBLIGHT_STATUS_CHANGE_LAYERS);
    rgblight_timer_task();
#ifdef RGBLIGHT_LAYERS_OVERRIDE_RGB_OFF
    assert(redraws == before + 1 && displayed_mask == 1);
#else
    assert(redraws == before && !rgblight_config.enable);
#endif
    puts("PASS: layer-only redraw, coalescing, flag isolation, combined flags and RGB-off");
}
'''


class RgbLightSyncTests(unittest.TestCase):
    def test_received_layers_redraw_on_next_task(self):
        source = (ROOT / "quantum/rgblight/rgblight.c").read_text()
        harness = "\n".join([SUPPORT, function(source, "rgblight_update_sync"),
                              function(source, "rgblight_timer_task"), SCENARIOS])
        with tempfile.TemporaryDirectory(prefix="rgblight-sync-") as tmp:
            path = Path(tmp)
            (path / "test.c").write_text(harness)
            for override_off in (False, True):
                with self.subTest(override_off=override_off):
                    defines = ["-DRGBLIGHT_LAYERS_OVERRIDE_RGB_OFF"] if override_off else []
                    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
                        "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", "-fno-sanitize-recover=all", *defines,
                        str(path / "test.c"), "-o", str(path / "test"),
                    ], check=True)
                    result = subprocess.run([str(path / "test")], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    print(result.stdout.strip())


if __name__ == "__main__":
    unittest.main(verbosity=2)
