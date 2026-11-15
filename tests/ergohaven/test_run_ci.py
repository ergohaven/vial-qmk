"""Offline contract tests for the native CI command inventory."""
import importlib.util
from pathlib import Path
import sys
import unittest
from test_native_fixture_boundaries import NativeFixtureBoundaries

PATH = Path(__file__).with_name('run_ci.py')


def load_runner():
    assert PATH.is_file(), 'native CI runner must exist'
    spec = importlib.util.spec_from_file_location('ergohaven_run_ci', PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class NativeCI(unittest.TestCase):
    def test_complete_command_inventory(self):
        runner = load_runner()
        self.assertIsNotNone(runner, 'native CI runner must exist')
        commands = runner.commands(Path('/external/output'), Path('/external/lvgl'))
        expected = [
            ['tests/ergohaven/run_native.py', '/external/output/native'],
            ['tests/ergohaven_app_combo.py'],
            ['tests/ergohaven_display/run.py', 'all'],
            ['tests/ergohaven/test_phenom_settings_sync.py'],
            ['tests/ergohaven/test_split_state_sync.py'],
            ['tests/ergohaven/test_rgblight_sync.py'],
            ['tests/ergohaven/test_macropad_product_name.py'],
            ['tests/ergohaven/run_macropad_bugs.py', '/external/output/encoder', '--only', '002'],
            ['tests/ergohaven/run_macropad_bugs.py', '/external/output/labels-default', '--only', '004', '--no-app-layout', '--lvgl', '/external/lvgl'],
            ['tests/ergohaven/run_macropad_bugs.py', '/external/output/labels-app', '--only', '004', '--lvgl', '/external/lvgl'],
            ['tests/ergohaven/run_macropad_wake.py', '/external/output/wake', '--lvgl', '/external/lvgl'],
        ]
        self.assertEqual(commands[:11], [[sys.executable, *args] for args in expected])

    def test_production_sm62_qualification_is_in_official_wave(self):
        runner = load_runner()
        commands = runner.commands(Path('/external/output'), Path('/external/lvgl'))
        scripts = [command[1] for command in commands[11:]]
        expected = [
            'run_sm62_layout.py', 'run_sm62_split.py', 'run_sm62_cached_split.py',
            'run_sm62_delta_split.py', 'run_sm62_rle_split.py', 'run_sm62_defaults.py',
            'run_sm62_layout_reserve.py', 'run_sm62_peer_backlight.py', 'run_sm62_peer_power.py',
            'run_sm62_peer_recovery.py', 'run_sm62_rpc_recovery.py', 'run_sm62_startup.py',
            'run_sm62_raw_fifo.py', 'run_sm62_storage.py', 'run_sm62_post_init_preservation.py',
            'run_sm62_screen.py', 'run_sm62_player_cjk.py', 'run_resident100.py',
            'sm62_interop/fixtures.py', 'test_compact_usb_compat.py', 'run_resident100_peer.py',
            'test_resident100_restart.py', 'sm62_macro_storage/test_macro_storage.py',
            'sm62_locked_boot/run.py', 'sm62_locked_boot/run_via_optional.py',
            'sm62_settings_deadline/run.py', 'sm62_usb_identity/run.py', 'sm62_interop/build.py', 'sm62_interop/selfcheck.py',
        ]
        self.assertEqual(scripts[:len(expected)], ['tests/ergohaven/' + name for name in expected])
        self.assertEqual(scripts[len(expected):], [
            '-m', '-m', '-m',
            'keyboards/ergohaven/sm62/tests/run_pipeline.py',
            'keyboards/ergohaven/sm62/tests/run_repeat_display.py',
            'keyboards/ergohaven/sm62/tests/run_scan_timing.py',
            'keyboards/ergohaven/sm62/tests/run_render_input.py',
            'keyboards/ergohaven/sm62/tests/run_display_guards.py',
            'keyboards/ergohaven/sm62/tests/run_peer_capture.py',
        ])
        for script in scripts:
            if script != '-m':
                self.assertTrue((runner.ROOT / script).is_file(), script)
        peer = next(i for i, cmd in enumerate(commands) if cmd[1].endswith('run_resident100_peer.py'))
        restart = commands[peer + 1]
        self.assertEqual(restart[2:], ['/external/output/resident-peer/resident100_peer',
                                     '/external/output/resident100/fixtures', '/external/output/resident-restart'])
        for command in commands[11:]:
            self.assertNotIn('--legacy-abort-only', command, 'retain the complete fault campaigns')
        macro = next(cmd for cmd in commands if cmd[1].endswith('sm62_macro_storage/test_macro_storage.py'))
        self.assertEqual(macro[2:], ['--source-root', str(runner.ROOT), '--output-dir', '/external/output/macro-storage'])

    def test_usb_identity_has_portable_firmware_only_mode(self):
        import subprocess
        runner = load_runner()
        command = [sys.executable, str(runner.ROOT / 'tests/ergohaven/sm62_usb_identity/run.py'), '--help']
        result = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('--firmware-only', result.stdout)
        usb = next(cmd for cmd in runner.commands(Path('/external/output'), Path('/external/lvgl'))
                   if cmd[1].endswith('sm62_usb_identity/run.py'))
        self.assertEqual(usb[2:], ['--firmware-only', '--phase', 'ci'])

    def test_child_environment_supports_managed_output_consumers(self):
        runner = load_runner()
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            code = ('import os; from pathlib import Path; '
                    'p=Path(os.environ["AGENT_BUILD_CACHE"]); '
                    'assert p == Path(' + repr(str(output)) + '); '
                    'assert Path(os.environ["TMPDIR"]).parent == p')
            self.assertEqual(runner.run([[sys.executable, '-c', code]], output), 0)

    def test_shipping_single_configuration_pins_match(self):
        import hashlib
        import json
        runner = load_runner()
        pins = json.loads((runner.ROOT / 'tests/ergohaven/sm62_macro_storage/single_config.json').read_text())
        for name, digest in pins.items():
            path, _, section = name.partition(':')
            data = (runner.ROOT / path).read_bytes()
            if section:
                data = data.split(b'void nvm_dynamic_keymap_erase(void)')[0]
            with self.subTest(owner=name):
                self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
        config = (runner.ROOT / 'keyboards/ergohaven/sm62/config.h').read_text()
        self.assertIn('#define SM62_OLD_RENDERER 1', config)
        self.assertIn('#define SM62_SINGLE_USER_HOUSEKEEPING', config)
        self.assertNotIn('#define SM62_DUALCORE_RENDER', config)
        self.assertNotIn('#define LV_USE_COOPERATIVE_REFR', config)

    def test_cli_rejects_artifacts_in_source_tree(self):
        runner = load_runner()
        self.assertTrue(hasattr(runner, 'main'), 'CLI entry point must exist')
        with self.assertRaises(SystemExit) as error:
            runner.main([str(runner.ROOT / 'ci-output'), '--lvgl', '/external/lvgl'])
        self.assertEqual(error.exception.code, 2)

    def test_cli_rejects_source_ancestors_before_writes(self):
        runner = load_runner()
        from unittest.mock import patch
        with patch.object(runner, 'run', return_value=0) as execute:
            with self.assertRaises(SystemExit) as error:
                runner.main([str(runner.ROOT.parent), '--lvgl', '/external/lvgl'])
            self.assertEqual(error.exception.code, 2)
            execute.assert_not_called()

    def test_cli_requires_fresh_wave_output(self):
        runner = load_runner()
        import tempfile
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as tmp, patch.object(runner, 'run', return_value=0) as execute:
            with self.assertRaises(SystemExit) as error:
                runner.main([tmp, '--lvgl', '/external/lvgl'])
            self.assertEqual(error.exception.code, 2)
            execute.assert_not_called()

    def test_runs_every_command_and_propagates_failures(self):
        runner = load_runner()
        self.assertTrue(hasattr(runner, 'run'), 'serial executor must exist')
        import subprocess
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            marker = output / 'continued'
            result = runner.run([
                [sys.executable, '-c', 'raise SystemExit(7)'],
                [sys.executable, '-c', f'from pathlib import Path; Path({str(marker)!r}).write_text("yes")'],
            ], output)
            self.assertEqual(result, 1)
            self.assertEqual(marker.read_text(), 'yes')
            self.assertEqual(runner.run([[sys.executable, '-c', 'pass']], output), 0)
            self.assertEqual(runner.run([['/nonexistent/ci-command']], output), 1)


if __name__ == '__main__':
    unittest.main()
