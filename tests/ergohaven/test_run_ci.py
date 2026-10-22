"""Offline contract tests for the native CI command inventory."""
import importlib.util
from pathlib import Path
import sys
import unittest

PATH = Path(__file__).with_name('run_ci.py')


def load_runner():
    if not PATH.exists():
        return None
    spec = importlib.util.spec_from_file_location('ergohaven_run_ci', PATH)
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
            ['tests/ergohaven/test_macropad_product_name.py'],
            ['tests/ergohaven/run_macropad_bugs.py', '/external/output/encoder', '--only', '002'],
            ['tests/ergohaven/run_macropad_bugs.py', '/external/output/labels-default', '--only', '004', '--no-app-layout', '--lvgl', '/external/lvgl'],
            ['tests/ergohaven/run_macropad_bugs.py', '/external/output/labels-app', '--only', '004', '--lvgl', '/external/lvgl'],
            ['tests/ergohaven/run_macropad_wake.py', '/external/output/wake', '--lvgl', '/external/lvgl'],
        ]
        self.assertEqual(commands, [[sys.executable, *args] for args in expected])

    def test_cli_rejects_artifacts_in_source_tree(self):
        runner = load_runner()
        self.assertTrue(hasattr(runner, 'main'), 'CLI entry point must exist')
        with self.assertRaises(SystemExit) as error:
            runner.main([str(runner.ROOT / 'ci-output'), '--lvgl', '/external/lvgl'])
        self.assertEqual(error.exception.code, 2)

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
