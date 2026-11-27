"""Narrow host regression; no firmware/toolchain or hardware required.

Run inside agent-storage. Production functions are extracted verbatim; only
settings application and non-encoder dependencies are simulated.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "keyboards/ergohaven/src/eh_pointing.c"
DESCRIPTOR = "keyboards/ergohaven/src/json/settings_phenom_modules_invert_axes.json"
MODELS = ("phenom", "phenom_mini", "phenom_micro")


def function(text, name):
    match = re.search(r"^(?:static )?(?:__attribute__\(\(weak\)\) )?[^\n;{}]+\b" + name + r"\([^;]*?\) \{", text, re.M)
    if not match:
        raise AssertionError(f"missing production function {name}")
    end = match.end()
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[match.start():end]


class PhenomEncoderStepOnly(unittest.TestCase):
    def test_production_delay_and_packed_step(self):
        source = SOURCE.read_text()
        header = (SOURCE.with_suffix('.h')).read_text()
        struct = header[header.index('typedef struct {'):header.index('} kb_settings_split_pointing_t;') + len('} kb_settings_split_pointing_t;')]
        constants = source[source.index('#define PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK'):source.index('static uint8_t phenom_auto_mouse_mode_mask')]
        # The independent single-module table has no extracted caller here.
        constants = re.sub(r'^static const uint8_t module_encoder_interval_ms_table.*\n', '', constants, flags=re.M)
        names = ('get_split_pointing_settings_default', 'config_all_zero',
                 'kb_settings_split_pointing_sanitize',
                 'get_split_pointing_side_encoder_interval_idx',
                 'set_split_pointing_side_encoder_interval_idx',
                 'get_split_pointing_side_encoder_interval_ms',
                 'get_split_pointing_side_encoder_steps',
                 'set_split_pointing_side_encoder_steps',
                 'split_pointing_encoder_side', 'encoder_map_key_delay', 'encoder_map_steps')
        production = '\n'.join(function(source, name) for name in names)
        prefix = '''#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define EH_KEYBOARD_SPLIT_POINTING_V2
#define EH_SPLIT_POINTING_INVERT_AXES
#define SPLIT_KEYBOARD
#define NUM_ENCODERS_LEFT 1
#define AUTO_MOUSE_DEFAULT_LAYER 4
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
enum { ROT_0, ROT_90, ROT_180, ROT_270 };
enum { POINTING_MODE_NORMAL, POINTING_MODE_SNIPER, POINTING_MODE_SCROLL, POINTING_MODE_TEXT, POINTING_MODE_USR3 };
enum { SPLIT_POINTING_DEVICE_COUNT = 4 };
typedef enum { SPLIT_POINTING_SIDE_LEFT, SPLIT_POINTING_SIDE_RIGHT, SPLIT_POINTING_SIDE_COUNT } split_pointing_side_t;
static bool get_sticky_mode(void) { return true; }
'''
        boundary = '''
static kb_settings_split_pointing_t kb_settings_phenom_devices;
static void kb_settings_split_pointing_update(kb_settings_split_pointing_t c) { kb_settings_phenom_devices = c; }
'''
        cases = r'''
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    const uint8_t delays[] = {0, 5, 10, 15, 20, 30, 40, 60, 80, 100};
    for (unsigned step = 1; step <= 8; ++step) {
        for (unsigned idx = 0; idx < 16; ++idx) {
            kb_settings_phenom_devices = get_split_pointing_settings_default();
            for (unsigned side = 0; side < 2; ++side)
                kb_settings_phenom_devices.encoder_interval_idx[side] = ((step - 1) << 4) | idx;
            kb_settings_phenom_devices = kb_settings_split_pointing_sanitize(kb_settings_phenom_devices);
            kb_settings_split_pointing_t before = kb_settings_phenom_devices;
            for (unsigned side = 0; side < 2; ++side) {
                unsigned effective_idx = idx < 10 ? idx : PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
                CHECK(get_split_pointing_side_encoder_interval_idx(side) == effective_idx);
                CHECK(encoder_map_steps(side) == step);
                CHECK(encoder_map_key_delay(side) == (EXPECT_ZERO ? 0 : delays[effective_idx]));
            }
            CHECK(memcmp(&before, &kb_settings_phenom_devices, sizeof(before)) == 0);
            for (unsigned side = 0; side < 2; ++side) {
                set_split_pointing_side_encoder_interval_idx(side, 9); // old client/preset
                CHECK(get_split_pointing_side_encoder_steps(side) == step);
                CHECK(encoder_map_key_delay(side) == (EXPECT_ZERO ? 0 : 100));
                set_split_pointing_side_encoder_steps(side, 9 - step);
                CHECK(get_split_pointing_side_encoder_interval_idx(side) == 9);
                CHECK(encoder_map_steps(side) == 9 - step);
            }
        }
    }
    kb_settings_split_pointing_t zero = {0};
    kb_settings_phenom_devices = kb_settings_split_pointing_sanitize(zero);
    CHECK(encoder_map_steps(0) == 1 && encoder_map_steps(1) == 1);
    CHECK(encoder_map_key_delay(0) == (EXPECT_ZERO ? 0 : 20));
    // Both older, unpacked versions migrate to step 1 without an EEPROM reset.
    for (unsigned version = PHENOM_SPLIT_POINTING_PREVIOUS_VERSION; version <= PHENOM_SPLIT_POINTING_UNPACKED_VERSION; ++version) {
        kb_settings_phenom_devices = get_split_pointing_settings_default();
        kb_settings_phenom_devices.version = version;
        kb_settings_phenom_devices.axis[0] = ROT_270;
        kb_settings_phenom_devices.encoder_interval_idx[0] = 9;
        kb_settings_phenom_devices.encoder_interval_idx[1] = 0;
        kb_settings_phenom_devices = kb_settings_split_pointing_sanitize(kb_settings_phenom_devices);
        CHECK(kb_settings_phenom_devices.axis[0] == ROT_270);
        CHECK(encoder_map_steps(0) == 1 && encoder_map_steps(1) == 1);
        CHECK(encoder_map_key_delay(0) == (EXPECT_ZERO ? 0 : 100));
        CHECK(encoder_map_key_delay(1) == 0);
    }
    puts("PASS: actual delay/callback/step/setters/sanitize/default; both sides, all 128 packed combinations, legacy writes and migration");
    return 0;
}
'''
        cache = Path(os.environ['AGENT_BUILD_CACHE']) / 'phenom-encoder-step-only'
        cache.mkdir(exist_ok=True)
        harness = cache / 'probe.c'
        harness.write_text(prefix + struct + '\n' + constants + boundary + production + '\n' + cases)
        for model in (*MODELS, 'hpd'):
            with self.subTest(model=model):
                binary = cache / 'probe'
                subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                f'-DKEYBOARD_ergohaven_{model}_rev' + ('3' if model == 'hpd' else '1'),
                                f'-DEXPECT_ZERO={int(model in MODELS)}', str(harness), '-o', str(binary)], check=True)
                result = subprocess.run([str(binary)], text=True, capture_output=True)
                print(model, result.stdout, result.stderr, flush=True)
                self.assertEqual(result.returncode, 0)

    def test_descriptor_only_removes_interval_and_exact_consumers(self):
        old = subprocess.check_output(['git', 'show', 'HEAD:' + DESCRIPTOR], cwd=ROOT, text=True)
        expected = re.sub(r'^.*"title": "Encoder interval".*\n', '', old, flags=re.M)
        expected = expected.replace('"1250 ms", "1500 ms"]', '"1250 ms", "1500 ms", "1750 ms", "2000 ms", "2250 ms", "2500 ms", "2750 ms", "3000 ms"]')
        actual = (ROOT / DESCRIPTOR).read_text()
        self.assertEqual(actual, expected)
        groups = json.loads('[' + actual + ']')
        for group, qsid in zip(groups[:2], (376, 377)):
            fields = group['fields']
            steps = [f for f in fields if f['title'] == 'Encoder steps']
            self.assertEqual(steps, [{'type': 'select', 'title': 'Encoder steps', 'qsid': qsid, 'variants': list('12345678')}])
            self.assertFalse(any(f['qsid'] in (325, 326) for f in fields))
        tracked = subprocess.check_output(['git', 'ls-files', 'keyboards'], cwd=ROOT, text=True).splitlines()
        consumers = {p for p in tracked if p.endswith('.json') and DESCRIPTOR in (ROOT / p).read_text()}
        self.assertEqual(consumers, {f'keyboards/ergohaven/{m}/keymaps/v1/vial.json' for m in MODELS})
        for path in sorted(consumers):
            with self.subTest(descriptor=path):
                definition = json.loads(subprocess.check_output(
                    ['cpp', '-P', '-x', 'c', '-I', str(ROOT), str(ROOT / path)], text=True))
                all_fields = [f for tab in definition['settings'] for f in tab['fields']]
                self.assertFalse(any(f['qsid'] in (325, 326) for f in all_fields))
                self.assertEqual([f['qsid'] for f in all_fields if f['title'] == 'Encoder steps'], [376, 377])
                # Layout/picker/direction data and all other authored includes stay unchanged.
                self.assertEqual((ROOT / path).read_bytes(), subprocess.check_output(
                    ['git', 'show', 'HEAD:' + path], cwd=ROOT))


if __name__ == '__main__':
    unittest.main(verbosity=2)
