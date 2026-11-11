#!/usr/bin/env python3
"""Guard regressions: rejection must precede mkdir and peer creation."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('selfcheck.py')

class SelfcheckGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=os.environ['AGENT_BUILD_CACHE'])
        self.root = Path(self.temp.name)
    def tearDown(self):
        self.temp.cleanup()
    def reject(self, flags=(), overrides=None, reason='optimized Python'):
        output = self.root / 'must-not-be-created'
        env = dict(os.environ, AGENT_BUILD_CACHE=str(self.root), PYTHONOPTIMIZE='0')
        env.update(overrides or {})
        result = subprocess.run([sys.executable, *flags, str(SCRIPT), '--exe',
                                 str(self.root/'absent-peer'), '--output', str(output)],
                                env=env, text=True, capture_output=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(reason, result.stderr)
        self.assertNotIn('FileNotFoundError', result.stderr)
        self.assertFalse(output.exists())
        self.assertNotIn('PASS', result.stdout)
    def test_optimization_flags_reject_before_side_effects(self):
        for flag in ['-O', '-OO']:
            with self.subTest(flag=flag): self.reject([flag])
    def test_environment_optimization_rejects_before_side_effects(self):
        for level in ['1', '2']:
            with self.subTest(level=level): self.reject(overrides={'PYTHONOPTIMIZE':level})
    def test_normal_mode_output_containment_is_explicit(self):
        self.reject(overrides={'AGENT_BUILD_CACHE':str(self.root/'other-cache')},
                    reason='output must be inside AGENT_BUILD_CACHE')

if __name__ == '__main__':
    unittest.main()
