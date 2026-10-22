"""Guard QMK CI trigger scope and execute the actual required-check gate."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / '.github/workflows/build-ergohaven.yml'


class QMKCIContract(unittest.TestCase):
    def test_automatic_build_push_is_production_only(self):
        header = WORKFLOW.read_text().split('\njobs:', 1)[0]
        self.assertIn('push:\n    branches: [ergohaven]', header)
        self.assertIn('workflow_dispatch:', header)
        self.assertIn('workflow_call:', header)
        self.assertIn('default: true', header)
        uid = (ROOT / '.github/workflows/ci.yml').read_text().split('\njobs:', 1)[0]
        self.assertIn('push:\n    branches: [ergohaven]', uid)
        self.assertIn('pull_request:', uid)
        self.assertIn('workflow_dispatch:', uid)

    def test_every_policy_and_workflow_test_runs_in_plan(self):
        text = WORKFLOW.read_text()
        self.assertIn("unittest discover -s .github/scripts -p 'test_*.py'", text)
        self.assertIn('ci_policy.py plan --profile firmware', text)
        self.assertIn('ci_policy.py plan --profile native', text)
        self.assertIn('name: Ergohaven required checks', text)
        self.assertIn('git submodule update --init --depth 1 -- lib/lvgl', text)

    def test_required_gate_rejects_incomplete_coverage(self):
        text = WORKFLOW.read_text()
        match = re.search(r'          set -euo pipefail\n(.*?)^      - uses: actions/checkout@v4', text, re.M | re.S)
        if match is None:
            self.fail('Required-check shell gate is missing')
        script = 'set -euo pipefail\n' + textwrap.dedent(match.group(1))
        good = dict(PLAN_RESULT='success', SCOPE_RESULT='success', BUILD_RUN='true', NATIVE_RUN='true',
                    BUILD_RESULT='success', NATIVE_RESULT='success', MERGE_RESULT='success',
                    BUILD_REASON='full', NATIVE_REASON='full', BUILD_SOURCE='', NATIVE_SOURCE='')
        cases = [(good, 0), ({**good, 'BUILD_RUN':'false', 'BUILD_RESULT':'skipped', 'MERGE_RESULT':'skipped'}, 0),
                 ({**good, 'BUILD_RUN':'false', 'BUILD_RESULT':'skipped', 'MERGE_RESULT':'skipped',
                   'NATIVE_RUN':'false', 'NATIVE_RESULT':'skipped'}, 0)]
        for key in ('PLAN_RESULT', 'SCOPE_RESULT', 'BUILD_RESULT', 'NATIVE_RESULT', 'MERGE_RESULT'):
            cases.append(({**good, key:'failure'}, 1))
            cases.append(({**good, key:'skipped'}, 1))
        for key in ('BUILD_RUN', 'NATIVE_RUN'):
            cases.append(({**good, key:''}, 1))
        with tempfile.TemporaryDirectory() as tmp:
            for index, (variables, expected) in enumerate(cases):
                with self.subTest(case=index):
                    env = dict(os.environ, **variables, GITHUB_STEP_SUMMARY=str(Path(tmp) / 'summary'))
                    result = subprocess.run(['bash', '-c', script], env=env, capture_output=True, text=True)
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
