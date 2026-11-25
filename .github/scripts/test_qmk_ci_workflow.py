"""Guard QMK CI trigger scope and execute the actual required-check gate."""
from pathlib import Path
import json
import os
import re
import subprocess
import tempfile
import textwrap
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / '.github/workflows/release-validation.yml'
MANUAL = ROOT / '.github/workflows/build-ergohaven.yml'


class QMKCIContract(unittest.TestCase):
    def test_uid_validator_follows_shared_config_without_losing_duplicate_checks(self):
        validator = ROOT / 'util/ci_vial_verify_uid.py'
        uid = '#define VIAL_KEYBOARD_UID {0xBB, 0x17, 0xF0, 0x5B, 0x02, 0x80, 0x1D, 0x1D}\n'
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            shared = base / 'keyboards/first/keymaps/common_config.h'
            keymap = shared.parent / 'v1'
            keymap.mkdir(parents=True)
            (keymap / 'vial.json').write_text('{}')
            (keymap / 'config.h').write_text('#include "../common_config.h"\n')
            shared.write_text(uid)

            def run():
                return subprocess.run(['python3', str(validator)], cwd=base,
                                      capture_output=True, text=True)

            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('first uses UID 1d1d80025bf017bb', result.stdout)
            second = base / 'keyboards/second/keymaps/v1'
            second.mkdir(parents=True)
            (second / 'vial.json').write_text('{}')
            (second / 'config.h').write_text(uid)
            result = run()
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('duplicated:', result.stdout)
            # Cycles terminate; a UID outside keyboards cannot mask a missing UID.
            shared.write_text('#include "v1/config.h"\n#include "../../../outside.h"\n')
            (base / 'outside.h').write_text(uid)
            result = run()
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('first does not define a VIAL_KEYBOARD_UID', result.stdout)

    def test_automatic_checks_are_production_or_staging_and_manual_build_is_separate(self):
        header = MANUAL.read_text().split('\njobs:', 1)[0]
        self.assertIn('workflow_dispatch:', header)
        for event in ('workflow_call:', 'pull_request:', 'push:'):
            self.assertNotIn(event, header)
        release = WORKFLOW.read_text().split('\njobs:', 1)[0]
        self.assertIn('workflow_call:', release)
        self.assertNotIn('workflow_dispatch:', release)
        uid = (ROOT / '.github/workflows/ci.yml').read_text()
        self.assertIn('push:\n    branches: [main, ci/source-check]', uid)
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
        scope = __import__('runpy').run_path(str(ROOT / 'util/ci_ergohaven_release_scope.py'))
        retired = scope['RETIRED_RELEASE_TARGETS']
        original_workers = original.split(b'\n  verify-release-scope:', 1)[1]
        retained = []
        removed = set()
        for line in original_workers.splitlines(keepends=True):
            match = scope['MATRIX_ENTRY'].match(line.decode())
            target = tuple(part.strip() for part in match.groups()) if match else None
            if target in retired:
                removed.add(target)
            else:
                retained.append(line)
        original_targets = {tuple(part.strip() for part in match.groups())
                            for line in original_workers.splitlines()
                            if (match := scope['MATRIX_ENTRY'].match(line.decode()))}
        self.assertEqual(removed, retired & original_targets)
        self.assertEqual(WORKFLOW.read_bytes().split(b'\n  verify-release-scope:', 1)[1]
                         .split(b'\n  product-scope:', 1)[0], b''.join(retained))

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
        self.assertEqual(len(rows), 6)
        self.assertEqual(len({(row['kb'], row['km']) for row in rows}), 6)
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
            normalize = __import__('runpy').run_path(
                str(ROOT / '.github/scripts/test_qmk_release.py'))['publisher_before_packaging']
            restored = normalize(current.decode(), name).encode()
            self.assertEqual(restored.replace(b'uses: ./.github/workflows/release-validation.yml',
                                              b'uses: ./.github/workflows/build-ergohaven.yml'), original)


    def test_prerelease_is_manual_and_keeps_its_existing_qualification_and_payload(self):
        current = (ROOT / '.github/workflows/pre-release.yml').read_bytes()
        original = subprocess.check_output([
            'git', 'show', 'df0666fba81afa036dde0f16f222e3f9225d5168:.github/workflows/pre-release.yml'], cwd=ROOT)
        expected = original.replace(b"on:\n  push:\n    branches: ['vial']\n", b'on:\n  workflow_dispatch:\n', 1)
        self.assertEqual(expected.count(b'\n            release_assets/*.zip'), 1)
        expected = expected.replace(b'\n            release_assets/*.zip', b'', 1)
        self.assertEqual(current, expected)
        self.assertIn(b'uses: ./.github/workflows/release-validation.yml', current)

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
        workflow = (ROOT / '.github/workflows/sm62-isolated.yml').read_text()
        header, jobs = workflow.split('\njobs:', 1)
        self.assertNotRegex(header, r'(?m)^  (push|pull_request|schedule|workflow_call):')
        self.assertRegex(header, r'(?m)^  workflow_dispatch:\n    inputs:\n      sm62_isolated:\n'
                         r'        description: [^\n]+\n        type: boolean\n        default: false$')
        job = jobs.split('\n  isolated:', 1)[1]
        self.assertIn("if: ${{ github.event_name == 'workflow_dispatch' && inputs.sm62_isolated }}", job)
        self.assertIn('SM62_DUALCORE_RENDER=yes SM62_SCAN_TRACE=yes', job)
        self.assertIn('name: sm62-isolated-development', job)

    def test_product_source_scope_replaces_foreign_mass_compile_without_duplicate_builds(self):
        path = '.github/workflows/product-scope.yml'
        text = (ROOT / path).read_text()
        header = text.split('\njobs:', 1)[0]
        self.assertIn('workflow_call:', header)
        self.assertNotIn('workflow_dispatch:', header)
        self.assertNotIn('schedule:', header)
        self.assertFalse((ROOT / '.github/workflows/upstream-compatibility.yml').exists())
        self.assertFalse((ROOT / '.github/upstream-vial-known-failures.txt').exists())
        self.assertIn('contents: read', header)
        self.assertIn('persist-credentials: false', text)
        self.assertIn('python3 -B util/ci_ergohaven_release_scope.py', text)
        self.assertIn('python3 -B util/ci_vial_verify_uid.py', text)
        for foreign_or_duplicate_build in ('mass-compile', 'qmk compile', 'run_ci.py', 'apt-get'):
            self.assertNotIn(foreign_or_duplicate_build, text)
        release = WORKFLOW.read_text()
        self.assertIn('uses: ./.github/workflows/product-scope.yml', release)
        self.assertIn('needs: [required, product-scope]', release)
        command = next(line.strip() for line in release.splitlines()
                       if line.strip().startswith("python3 -c 'import json, os, sys; results ="))
        for required in ('success', 'failure', 'cancelled', 'skipped'):
            for source_scope in ('success', 'failure', 'cancelled', 'skipped'):
                values = {'required': {'result': required}, 'product-scope': {'result': source_scope}}
                result = subprocess.run(['bash', '-c', command],
                                        env={**os.environ, 'RELEASE_RESULTS': json.dumps(values)},
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, required == source_scope == 'success')
        for values in ({}, {'required': {'result': 'success'}}, {'product-scope': {'result': 'success'}}):
            result = subprocess.run(['bash', '-c', command],
                                    env={**os.environ, 'RELEASE_RESULTS': json.dumps(values)},
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)

    def test_product_keyboard_namespace_rejects_foreign_sources_and_missing_product(self):
        namespace = __import__('runpy').run_path(str(ROOT / 'util/ci_ergohaven_release_scope.py'))
        verify = namespace['verify_keyboard_namespace']
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            product = root / 'ergohaven'
            with patch.dict(verify.__globals__, KEYBOARDS_ROOT=root, ERGOHAVEN_ROOT=product):
                with self.assertRaisesRegex(ValueError, 'only ergohaven'):
                    verify()
                product.mkdir()
                verify()
                foreign = root / 'foreign'
                foreign.mkdir()
                with self.assertRaisesRegex(ValueError, 'foreign'):
                    verify()
                foreign.rmdir()
                foreign.write_text('Foreign files are forbidden too')
                with self.assertRaisesRegex(ValueError, 'foreign'):
                    verify()



if __name__ == '__main__':
    unittest.main()
