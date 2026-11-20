"""Guard QMK CI trigger scope and execute the actual required-check gate."""
from pathlib import Path
import json
import os
import re
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / '.github/workflows/release-validation.yml'
MANUAL = ROOT / '.github/workflows/build-ergohaven.yml'


class QMKCIContract(unittest.TestCase):
    def test_automatic_checks_are_production_only_and_manual_build_is_separate(self):
        header = MANUAL.read_text().split('\njobs:', 1)[0]
        self.assertIn('workflow_dispatch:', header)
        for event in ('workflow_call:', 'pull_request:', 'push:'):
            self.assertNotIn(event, header)
        release = WORKFLOW.read_text().split('\njobs:', 1)[0]
        self.assertIn('workflow_call:', release)
        self.assertNotIn('workflow_dispatch:', release)
        uid = (ROOT / '.github/workflows/ci.yml').read_text()
        self.assertIn('push:\n    branches: [ergohaven]', uid)
        self.assertIn('pull_request:', uid)
        self.assertIn('fetch-depth: 0', uid)
        self.assertIn("unittest discover -s .github/scripts -p 'test_*.py'", uid)

    def test_release_plan_always_selects_both_full_profiles_without_api_reuse(self):
        text = WORKFLOW.read_text()
        plan = text.split('\n  verify-release-scope:', 1)[0]
        self.assertIn("unittest discover -s .github/scripts -p 'test_*.py'", plan)
        self.assertIn("echo 'run=true'", plan)
        self.assertIn('build: ${{ steps.full.outputs.run }}', plan)
        self.assertIn('native: ${{ steps.full.outputs.run }}', plan)
        self.assertNotIn('ci_policy.py plan', plan)
        self.assertNotIn('secrets.', plan)
        self.assertNotIn('        if:', plan)
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

    def test_complete_release_workers_are_moved_byte_for_byte(self):
        original = subprocess.check_output([
            'git', 'show', '088af4594448e3cc11e67626fa0553ba2cc066e2:.github/workflows/build-ergohaven.yml'
        ], cwd=ROOT)
        self.assertEqual(WORKFLOW.read_bytes().split(b'\n  verify-release-scope:', 1)[1]
                         .split(b'\n  upstream-compatibility:', 1)[0],
                         original.split(b'\n  verify-release-scope:', 1)[1])

    def test_native_uses_the_known_working_qmk_arm_toolchain(self):
        text = MANUAL.read_text()
        match = re.search(r'      - name: Install supported ARM toolchain\n(.*?)      - name: Prepare output name',
                          text, re.S)
        if match is None:
            self.fail('The native build must install the working QMK ARM toolchain')
        step = match[1]
        self.assertIn('qmk/qmk_toolchains/releases/download/v15.2.0-1/', step)
        self.assertIn('qmk_toolchains-gcc15.2.0-host_linuxX64.tar.zst', step)
        self.assertIn('6b136919e3526f3f373e91f7d011a006e968a3169262d33cf6efa3e6c605c3ca', step)
        self.assertIn('sha256sum --check --strict', step)
        self.assertIn('test "$("$toolchain/bin/arm-none-eabi-gcc" -dumpfullversion)" = 15.2.0', step)
        self.assertIn('"$toolchain/bin" >> "$GITHUB_PATH"', step)
        self.assertLess(step.index('sha256sum --check --strict'), step.index('tar --zstd'))
        self.assertLess(step.index('-dumpfullversion'), step.index('>> "$GITHUB_PATH"'))
        self.assertNotIn('gcc-arm-none-eabi', text)
        self.assertNotIn('binutils-arm-none-eabi', text)
        self.assertNotIn('libnewlib-arm-none-eabi', text)
        self.assertNotIn('FLASH_LEN', text)
        self.assertNotIn('LTO_ENABLE', text)

    def test_manual_build_has_no_container_native_campaign_or_release_proof(self):
        text = MANUAL.read_text()
        for machinery in ('container:', 'docker ', 'nix ', 'run_ci.py', 'ci_policy.py',
                          'ci-proof-', 'secrets.', 'full-validation', 'unittest discover'):
            self.assertNotIn(machinery, text)
        self.assertIn('submodules: recursive', text)
        self.assertIn('qmk compile -kb ${{ matrix.kb }} -km ${{ matrix.km }}', text)
        self.assertIn('if-no-files-found: error', text)
        self.assertIn('fail-fast: false', text)
        self.assertIn('name: ergohaven_fw', text)
        self.assertNotRegex(text, r'(?m)^\s*- \{ kb:')

    def test_real_matrix_command_emits_the_complete_existing_inventory(self):
        text = MANUAL.read_text()
        match = re.search(r'      - id: targets\n        run: \|\n(.*?)\n  build-ergohaven:', text, re.S)
        if match is None:
            self.fail('Manual matrix command is missing')
        script = textwrap.dedent(match[1])
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'matrix'
            result = subprocess.run(['bash', '-euo', 'pipefail', '-c', script], cwd=ROOT,
                                    env={**os.environ, 'GITHUB_OUTPUT': str(output)},
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            key, value = output.read_text().strip().split('=', 1)
            self.assertEqual(key, 'matrix')
            rows = json.loads(value)['include']
        self.assertEqual(len(rows), 32)
        self.assertEqual(len({(row['kb'], row['km']) for row in rows}), 32)
        scope_text = WORKFLOW.read_text()
        expected = [tuple(part.strip() for part in match.groups())
                    for line in scope_text.splitlines()
                    if (match := re.match(r'^\s*-\s*\{\s*kb:\s*([^,]+),\s*km:\s*([^}]+)\}', line))]
        self.assertEqual([(row['kb'], row['km']) for row in rows], expected)
        self.assertIn({'kb': 'ergohaven/sm62', 'km': 'v1'}, rows)

    def test_unknown_matrix_arguments_are_rejected(self):
        result = subprocess.run(['python3', 'util/ci_ergohaven_release_scope.py', '--unknown'],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)

    def test_both_release_publishers_keep_the_same_gated_payload(self):
        for name in ('release.yml', 'pre-release.yml'):
            path = '.github/workflows/' + name
            original = subprocess.check_output([
                'git', 'show', '088af4594448e3cc11e67626fa0553ba2cc066e2:' + path], cwd=ROOT)
            current = (ROOT / path).read_bytes()
            self.assertEqual(current.count(b'uses: ./.github/workflows/release-validation.yml'), 1)
            self.assertEqual(current.replace(b'uses: ./.github/workflows/release-validation.yml',
                                             b'uses: ./.github/workflows/build-ergohaven.yml'), original)


    def test_protected_branch_contexts_remain_fail_closed_and_lightweight(self):
        text = (ROOT / '.github/workflows/ci.yml').read_text()
        self.assertIn('name: Check Vial UIDs', text)
        gate = text.split('\n  required:', 1)[1]
        self.assertIn('name: Ergohaven required checks', gate)
        self.assertIn('if: ${{ always() }}', gate)
        self.assertIn('needs: check-uid', gate)
        self.assertIn('CHECKS_RESULT: ${{ needs.check-uid.result }}', gate)
        command = next(line.strip().removeprefix('run: ') for line in gate.splitlines()
                       if line.strip().startswith('run: '))
        for value in ('success', 'failure', 'cancelled', 'skipped', ''):
            result = subprocess.run(['bash', '-c', command],
                                    env={**os.environ, 'CHECKS_RESULT': value},
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode == 0, value == 'success')

    def test_sm62_isolated_artifact_is_manual_and_opt_in(self):
        ci = (ROOT / '.github/workflows/ci.yml').read_text()
        header, jobs = ci.split('\njobs:', 1)
        self.assertRegex(header, r'(?m)^  workflow_dispatch:\n    inputs:\n      sm62_isolated:\n'
                         r'        description: [^\n]+\n        type: boolean\n        default: false$')
        job = jobs.split('\n  isolated:', 1)[1]
        self.assertIn("if: ${{ github.event_name == 'workflow_dispatch' && inputs.sm62_isolated }}", job)
        self.assertIn('SM62_DUALCORE_RENDER=yes SM62_SCAN_TRACE=yes', job)
        self.assertIn('name: sm62-isolated-development', job)

    def test_upstream_mass_compile_is_release_only_with_unchanged_coverage(self):
        path = '.github/workflows/upstream-compatibility.yml'
        text = (ROOT / path).read_text()
        header = text.split('\njobs:', 1)[0]
        self.assertIn('workflow_call:', header)
        self.assertNotIn('workflow_dispatch:', header)
        self.assertNotIn('schedule:', header)
        original = subprocess.check_output([
            'git', 'show', '088af4594448e3cc11e67626fa0553ba2cc066e2:' + path], cwd=ROOT, text=True)
        old_trigger = "on:\n  workflow_dispatch:\n  schedule:\n    - cron: '17 3 * * 1'\n"
        self.assertEqual(text.replace('on:\n  workflow_call:\n', old_trigger, 1), original)
        release = WORKFLOW.read_text()
        self.assertIn('uses: ./.github/workflows/upstream-compatibility.yml', release)
        self.assertIn('needs: [required, upstream-compatibility]', release)
        command = next(line.strip() for line in release.splitlines()
                       if line.strip().startswith("python3 -c 'import json, os, sys; results ="))
        for required in ('success', 'failure', 'cancelled', 'skipped'):
            for upstream in ('success', 'failure', 'cancelled', 'skipped'):
                values = {'required': {'result': required}, 'upstream-compatibility': {'result': upstream}}
                result = subprocess.run(['bash', '-c', command],
                                        env={**os.environ, 'RELEASE_RESULTS': json.dumps(values)},
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, required == upstream == 'success')
        for values in ({}, {'required': {'result': 'success'}}):
            result = subprocess.run(['bash', '-c', command],
                                    env={**os.environ, 'RELEASE_RESULTS': json.dumps(values)},
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)



if __name__ == '__main__':
    unittest.main()
