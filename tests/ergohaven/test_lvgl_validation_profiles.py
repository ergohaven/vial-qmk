#!/usr/bin/env python3
"""Validator unit controls (synthetic blobs), plus actual pinned-base proof.

No LVGL source is changed. Synthetic data tests the profile selector, not LVGL
rendering; the ordinary native CI exercises the actual library and renderers.
"""
from pathlib import Path
import hashlib
import json
import tempfile
import unittest
from unittest.mock import patch

import macropad_native_support as support

ROOT = Path(__file__).resolve().parents[2]
OVERLAY = ('src/core/lv_refr.c', 'src/core/lv_refr.h',
           'src/hal/lv_hal_disp.c', 'src/hal/lv_hal_disp.h', 'src/misc/lv_timer.c')


def blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


class ValidationProfiles(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='lvgl-profile-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.lvgl = self.directory / 'lvgl'
        self.files = (*OVERLAY, *(f'src/control_{i}.c' for i in range(601)))
        self.base = b'/* synthetic pinned base */\n'
        self.patched = b'/* synthetic exact cooperative overlay */\n'
        for filename in self.files:
            target = self.lvgl / filename
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(self.base)
        (self.directory / 'sm62-lvgl-blobs.json').write_text(json.dumps({
            'revision': 'e19410f8f8a256609da72cff549598e0df6fa4cf',
            'blobs': {filename: blob(self.base) for filename in self.files},
        }))
        self.overlay = self.directory / 'sm62-lvgl-cooperative-blobs.json'
        self.overlay.write_text(json.dumps({filename: {
            'base': blob(self.base), 'patched': blob(self.patched),
        } for filename in OVERLAY}))
        self.context = patch.object(support, '__file__', str(self.directory / 'support.py'))
        self.context.start()
        self.addCleanup(self.context.stop)

    def install_overlay(self):
        for filename in OVERLAY:
            (self.lvgl / filename).write_bytes(self.patched)

    def test_base_is_default_even_with_overlay_manifest(self):
        support.validate_lvgl(self.lvgl)

    def test_exact_cooperative_requires_explicit_selection(self):
        self.install_overlay()
        support.validate_lvgl(self.lvgl, profile='cooperative')
        with self.assertRaisesRegex(AssertionError, OVERLAY[0]):
            support.validate_lvgl(self.lvgl)

    def test_base_is_not_a_cooperative_fallback(self):
        with self.assertRaisesRegex(AssertionError, OVERLAY[0]):
            support.validate_lvgl(self.lvgl, profile='cooperative')

    def test_each_overlay_hash_is_required(self):
        self.install_overlay()
        for filename in OVERLAY:
            with self.subTest(filename=filename):
                (self.lvgl / filename).write_bytes(self.base)
                with self.assertRaisesRegex(AssertionError, filename):
                    support.validate_lvgl(self.lvgl, profile='cooperative')
                (self.lvgl / filename).write_bytes(self.patched)

    def test_nonoverlay_hash_is_required_in_both_profiles(self):
        filename = self.files[-1]
        (self.lvgl / filename).write_bytes(b'corrupt')
        with self.assertRaisesRegex(AssertionError, filename):
            support.validate_lvgl(self.lvgl)
        self.install_overlay()
        with self.assertRaisesRegex(AssertionError, filename):
            support.validate_lvgl(self.lvgl, profile='cooperative')

    def test_missing_overlay_is_not_optional(self):
        self.overlay.unlink()
        with self.assertRaises(FileNotFoundError):
            support.validate_lvgl(self.lvgl, profile='cooperative')

    def test_unknown_profile_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, 'unknown LVGL profile'):
            support.validate_lvgl(self.lvgl, profile='either')


class ActualPinnedBase(unittest.TestCase):
    def test_actual_606_pinned_blobs_and_no_cooperative_fallback(self):
        support.validate_lvgl(ROOT / 'lib/lvgl', profile='base')
        with self.assertRaisesRegex(AssertionError, OVERLAY[0]):
            support.validate_lvgl(ROOT / 'lib/lvgl', profile='cooperative')


if __name__ == '__main__':
    unittest.main()
