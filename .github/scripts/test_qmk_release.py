"""Release integration contracts; templates remain C-preprocessor input, not JSON."""
import importlib.util
import ast
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]


def scope_module():
    spec = importlib.util.spec_from_file_location('release_scope', ROOT / 'util/ci_ergohaven_release_scope.py')
    assert spec is not None and spec.loader is not None
    scope = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(scope)
    return scope


def publisher_before_packaging(text, path):
    name = Path(path).name
    if name == 'release.yml':
        # Reverse only the owner-authorized publisher credential change.
        guard = '''      - name: Verify release publisher
        env:
          GH_TOKEN: ${{ secrets.ERGOHAVEN_RELEASE_TOKEN }}
        run: |
          test -n "$GH_TOKEN"
          test "$(gh api user --jq .login)" = kissetfall

'''
        token = '          token: ${{ secrets.ERGOHAVEN_RELEASE_TOKEN }}\n'
        if text.count(guard) != 1 or text.count(token) != 1:
            raise AssertionError('Expected the verified kissetfall release publisher')
        text = text.replace(guard, '', 1).replace(token, '', 1)
    if name == 'pre-release.yml':
        # Reverse only the separately approved routing change for the historical payload comparison.
        trigger = 'on:\n  workflow_dispatch:\n'
        if text.count(trigger) != 1:
            raise AssertionError('Expected exactly one manual prerelease trigger')
        text = text.replace(trigger, "on:\n  push:\n    branches: ['vial']\n", 1)
    steps = {'release.yml': '      - name: Prepare release assets\n        env:\n          RELEASE_VERSION: ${{ github.ref_name }}\n          SOURCE_COMMIT: ${{ github.sha }}\n        run: |\n          python3 util/prepare_ergohaven_release.py \\\n            --input ergohaven_fw/ergohaven_fw \\\n            --output release_assets \\\n            --version "$RELEASE_VERSION" \\\n            --source-commit "$SOURCE_COMMIT" \\\n            --manifest "$RUNNER_TEMP/qmk-release-assets.json"\n\n', 'pre-release.yml': '      - name: Prepare release assets\n        env:\n          RELEASE_VERSION: latest\n          SOURCE_COMMIT: ${{ github.sha }}\n        run: |\n          python3 util/prepare_ergohaven_release.py \\\n            --input ergohaven_fw/ergohaven_fw \\\n            --output release_assets \\\n            --version "$RELEASE_VERSION" \\\n            --source-commit "$SOURCE_COMMIT" \\\n            --manifest "$RUNNER_TEMP/qmk-release-assets.json"\n\n'}
    if name not in steps or text.count(steps[name]) != 1:
        raise AssertionError('Expected exactly one reviewed packaging step')
    replacement = "      - name: Configure\n        run: |\n          sudo apt-get install rename\n\n      - name: Rename\n        run: |\n          cd ergohaven_fw/ergohaven_fw\n          rename 's/(rev\\d_)//; s/ergohaven_/'${GITHUB_REF_NAME}'_'/ *.uf2\n\n" if name == 'release.yml' else ''
    text = text.replace(steps[name], replacement, 1)
    if name == 'pre-release.yml':
        checkout = '      - name: Checkout\n        uses: actions/checkout@v4\n        with:\n          persist-credentials: false\n\n'
        if text.count(checkout) != 1:
            raise AssertionError('Expected exactly one credential-free packager checkout')
        text = text.replace(checkout, '', 1)
    files = '            release_assets/*.uf2'
    if text.count(files) != 1:
        raise AssertionError('Expected only the reviewed supported-UF2 release glob')
    return text.replace(files, '            ergohaven_fw/**/*.uf2', 1)


class ReleaseScope(unittest.TestCase):
    def test_sm62_is_required_in_complete_official_matrix(self):
        scope = scope_module()
        text = (ROOT / scope.WORKFLOW).read_text()
        targets = [tuple(part.strip() for part in match.groups())
                   for line in text.splitlines() if (match := scope.MATRIX_ENTRY.match(line))]
        self.assertIn(('ergohaven/sm62', 'v1'), scope.REQUIRED_RELEASE_TARGETS)
        self.assertEqual(set(targets), scope.REQUIRED_RELEASE_TARGETS)
        self.assertEqual(len(targets), 6)
        self.assertEqual(len(set(targets)), 6)
        names = [kb.replace('/', '_') + '_' + km + '.uf2' for kb, km in targets]
        self.assertEqual(len(set(names)), 6)
        for target in [('ergohaven/macropad/rev2', 'v2_ccw')]:
            self.assertNotIn(target, targets)
            self.assertIn(scope.find_vial_definition(*target), scope.EXCLUDED_KEYMAPS)
        self.assertEqual(len(scope.EXCLUDED_KEYMAPS), 1)
        retired = {
            ("ergohaven/k03pro/rev1/43mm", "v1"),
            ("ergohaven/k03pro/rev1/65mm", "v1"),
            ("ergohaven/k03pro/rev2", "v2_v3"),
            ("ergohaven/hpd/rev1", "v1"),
            ("ergohaven/hpd/rev2", "kissetfall"),
            ("ergohaven/hpd/rev2", "v2"),
            ("ergohaven/hpd/rev2", "v2_ball_enc"),
            ("ergohaven/hpd/rev2", "v2_enc_ball"),
            ("ergohaven/hpd/rev2", "v2_enc_enc"),
            ("ergohaven/hpd/rev2", "v2_enc_joy"),
            ("ergohaven/hpd/rev2", "v2_enc_touch"),
            ("ergohaven/hpd/rev2", "v2_lcd_ball"),
            ("ergohaven/hpd/rev2", "v2_touch_enc"),
            ("ergohaven/imperial44/rev1", "v1_v2"),
            ("ergohaven/imperial44/rev3", "kissetfall"),
            ("ergohaven/imperial44/rev3", "v3_v4"),
            ("ergohaven/k02", "v1"),
            ("ergohaven/k03/rev1", "v1_v2"),
            ("ergohaven/k03/rev3", "kissetfall"),
            ("ergohaven/k03/rev3", "v3_v4"),
            ("ergohaven/macropad/rev1", "v1"),
            ("ergohaven/planeta/rev1", "v1"),
            ("ergohaven/planeta/rev2", "v2"),
            ("ergohaven/remnant", "v1"),
            ("ergohaven/sm30", "v1"),
            ("ergohaven/trackball", "v1"),
            ("ergohaven/trackball", "v2"),
            ("ergohaven/velvet/rev1", "v1"),
            ("ergohaven/velvet/rev2", "v2"),
            ("ergohaven/velvet/rev3", "v3"),
        }
        self.assertEqual(scope.RETIRED_RELEASE_TARGETS, retired)
        self.assertTrue(retired.isdisjoint(targets))
        archive = scope.verify_archived_sources()
        self.assertEqual(archive['snapshot_commit'], "8bd528239a8ae1bc0747af53a6f0ba5a32f63fbc")
        self.assertEqual(len(archive['files']), 299)
        self.assertFalse((ROOT / 'keyboards/ergohaven/k03').exists())
        self.assertFalse((ROOT / 'keyboards/ergohaven/imperial44').exists())
        fixed_encoder_files = [row for row in archive['files']
                               if row.get('source_snapshot') == 'k03-imperial44-20261125']
        self.assertEqual(len(fixed_encoder_files), 37)
        self.assertFalse((ROOT / 'keyboards/ergohaven/k03pro').exists())
        self.assertFalse((ROOT / 'keyboards/ergohaven/sm30').exists())
        animation = ROOT / 'keyboards/ergohaven/src/display/space_mission_anim.c'
        self.assertEqual(animation.read_bytes(),
                         (ROOT / 'archive/ergohaven/sm30/anim.c').read_bytes())
        self.assertIn('SRC += keyboards/ergohaven/src/display/space_mission_anim.c',
                      (ROOT / 'keyboards/ergohaven/sm62/rules.mk').read_text())
        velvet_files = [row for row in archive['files']
                        if row.get('source_snapshot') == 'velvet-20261125']
        self.assertEqual(len(velvet_files), 25)
        self.assertFalse((ROOT / 'keyboards/ergohaven/velvet').exists())
        hpd_files = [row for row in archive['files']
                     if row.get('source_snapshot') == 'hpd-v2-20261125']
        self.assertEqual(len(hpd_files), 61)
        self.assertFalse((ROOT / 'keyboards/ergohaven/hpd').exists())
        self.assertEqual(len(archive['source_snapshots']), 3)
        self.assertEqual(archive['source_snapshots'][0]['base_commit'],
                         '7c2ac25ef68285d386c1e2d143b0ffa9ed3a46ba')
        trackball_files = [row for row in archive['files']
                           if row['path'].startswith('archive/ergohaven/trackball/')]
        self.assertEqual(len(trackball_files), 17)
        self.assertEqual({row['snapshot_commit'] for row in trackball_files},
                         {'65e965c27694a3f15c1e7bf7574f39865ca45893'})
        for target in retired:
            self.assertIsNone(scope.find_vial_definition(*target))
            self.assertTrue((ROOT / scope.find_archived_vial_definition(*target)).is_file())
        result = subprocess.run([sys.executable, 'util/ci_ergohaven_release_scope.py'],
                                cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class ReleaseIdentity(unittest.TestCase):
    def test_all_independent_version_owners_are_aligned(self):
        root = ROOT / 'keyboards/ergohaven'
        self.assertRegex((root / 'config.h').read_text(), r'#define EH_FIRMWARE_VERSION 0x00010000\b')
        self.assertIn('#define EH_VERSION_STR "1.0"', (root / 'ergohaven.h').read_text())
        definitions = sorted(root.glob('**/vial.json'))
        self.assertEqual(len(definitions), 7)
        # Keep strings (including comment-like text) intact while stripping comments.
        token = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/', re.S)
        for path in definitions:
            with self.subTest(owner=path.relative_to(ROOT)):
                preamble = path.read_text().split('#include', 1)[0]
                preamble = token.sub(lambda m: m[0] if m[0].startswith('"') else '', preamble)
                firmware = re.findall(r'"firmware"\s*:\s*\{([^{}]*)\}', preamble)
                self.assertEqual(len(firmware), 1)
                self.assertEqual(re.findall(r'"version"\s*:\s*"([^"]+)"', firmware[0]), ['1.0'])
                self.assertEqual(re.findall(r'"firmwareVersion"\s*:\s*"([^"]+)"', preamble), ['1.0'])
        usb = []
        for path in sorted(root.glob('**/keyboard.json')):
            versions = re.findall(r'"device_version"\s*:\s*"([^"]+)"', path.read_text())
            if versions:
                usb.append(path)
                with self.subTest(owner=path.relative_to(ROOT)):
                    self.assertEqual(versions, ['1.0'])
        self.assertEqual(len(usb), 6)

    def test_two_component_usb_versions_use_the_real_config_generator(self):
        path = ROOT / 'lib/python/qmk/cli/generate/config_h.py'
        tree = ast.parse(path.read_text())
        owner = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'bcd_version')
        namespace = {}
        exec(compile(ast.Module(body=[owner], type_ignores=[]), str(path), 'exec'), namespace)
        encode = namespace['bcd_version']
        pattern = json.loads((ROOT / 'data/schemas/definitions.jsonschema').read_text())['bcd_version']['pattern']
        for version, expected in [('1.0', '0x0100'), ('1.1', '0x0101'), ('1.10', '0x0110'),
                                  ('2.0', '0x0200'), ('99.99', '0x9999'), ('4.2.0', '0x0420'), ('1.2.3', '0x0123')]:
            with self.subTest(version=version):
                self.assertRegex(version, pattern)
                self.assertEqual(encode(version), expected)
        for version in ['1', '1.2.3.4', '100.0', '1.100', '1.10.1', '1.-1', '1.a']:
            with self.subTest(version=version):
                self.assertIsNone(re.fullmatch(pattern, version))
                with self.assertRaises(ValueError):
                    encode(version)

    def test_release_prefix_defaults_to_two_components_without_normalizing(self):
        path = ROOT / 'util/prepare_ergohaven_release.py'
        tree = ast.parse(path.read_text())
        names = {'validate_release_version', 'release_name'}
        owners = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
        self.assertEqual(len(owners), len(names))
        namespace = {'re': re}
        exec(compile(ast.Module(body=owners, type_ignores=[]), str(path), 'exec'), namespace)
        validate = namespace['validate_release_version']
        for version in ['1.0', '1.1', '2.0', '1.10', 'latest']:
            validate(version)
        for version in ['1.0.0', '1.0.1', '4.2.0', 'v1.0', '01.0', '1', '../1.0', '1.0-rc1']:
            with self.subTest(version=version):
                with self.assertRaises(ValueError):
                    validate(version)
        validate('1.0.1', allow_patch_version=True)
        self.assertEqual(namespace['release_name']('ergohaven/phenom/rev1', 'v1', '1.0'), '1.0_phenom_v1.uf2')


class ReleasePublication(unittest.TestCase):
    def test_10_notes_readme_and_downloads(self):
        path = 'keyboards/ergohaven/changelog_ru.md'
        text = (ROOT / path).read_text()
        self.assertTrue(text.startswith('## 1.0\n'))
        notes, history = text.split('## 4.2.0\n', 1)
        original = subprocess.check_output(
            ['git', 'show', 'e41deee209235232e81ac86a3e92e1626492ec0c:' + path], cwd=ROOT)
        self.assertEqual(('## 4.2.0\n' + history).encode(), original)
        readme = (ROOT / 'readme.md').read_text()
        for body in (notes, readme):
            self.assertIn('feature parity', body)
            self.assertIn('RMK', body)
            self.assertNotIn('retir', body.lower())
        for term in ('auto layer', 'auto-layer', 'autolayer', 'автосло'):
            self.assertNotIn(term, notes.lower())
        self.assertIn('4.2.0...1.0', notes)
        expected = {'1.0_' + re.sub(r'rev\d_', '', kb.replace('/', '_') + '_' + km, count=1).replace('ergohaven_', '', 1) + '.uf2'
                    for kb, km in scope_module().REQUIRED_RELEASE_TARGETS}
        self.assertEqual(set(re.findall(r'`(1\.0_[^`]+\.uf2)`', readme)), expected)
        self.assertIn('SHA256SUMS.txt', readme)

    def test_420_notes_keep_rmk_sections_and_protected_history(self):
        path = 'keyboards/ergohaven/changelog_ru.md'
        text = (ROOT / path).read_text()
        # Historical release guarantees apply to that entry, not the newest one.
        text = '## 4.2.0\n' + text.split('## 4.2.0\n', 1)[1]
        self.assertTrue(text.startswith('## 4.2.0\n'))
        self.assertEqual(text.count('## 4.2.0\n'), 1)
        self.assertNotIn('## 4.2.1\n', text)
        notes, previous = text.split('## 4.1.0\n', 1)
        for heading in ['Features', 'Improvements', 'Fixes', 'Verification', 'Contributors']:
            self.assertIn('### ' + heading + '\n', notes)
        for contributor in ['@Yurii-Q', '@IgorArkhipov', '@kissetfall']:
            self.assertIn(contributor, notes)
        for number in range(51, 57):
            self.assertIn('/pull/' + str(number), notes)
        self.assertIn('4.1.0...4.2.0', notes)
        self.assertIn('resident100', notes)
        self.assertIn('SINGLE/core0', notes)
        self.assertIn('replacement', notes)
        # The published tag is explicitly approved for replacement. Pin its original
        # commit, not a mutable tag, and freeze only the 4.1.0-and-older suffix.
        baseline = '8f3d4f17b79b169c10226822d2ed616fa2f6d75b'
        original = subprocess.check_output(['git', 'show', baseline + ':' + path], cwd=ROOT)
        self.assertEqual(('## 4.1.0\n' + previous).encode(),
                         b'## 4.1.0\n' + original.split(b'## 4.1.0\n', 1)[1])
        # Retain every substantive feature/improvement/fix from the old 4.2.0.
        old_notes = original.decode().split('## 4.1.0\n', 1)[0]
        for section in ['Features', 'Improvements', 'Fixes']:
            body = old_notes.split('### ' + section + '\n', 1)[1].split('### ', 1)[0]
            for bullet in body.splitlines():
                if bullet.startswith('- '):
                    self.assertIn(bullet, notes)
        publisher = (ROOT / '.github/workflows/release.yml').read_bytes()
        original_publisher = subprocess.check_output(
            ['git', 'show', baseline + ':.github/workflows/release.yml'], cwd=ROOT)
        self.assertEqual(publisher.count(b'uses: ./.github/workflows/release-validation.yml'), 1)
        restored = publisher_before_packaging(publisher.decode(), 'release.yml').encode().replace(
            b'uses: ./.github/workflows/release-validation.yml',
            b'uses: ./.github/workflows/build-ergohaven.yml')
        self.assertEqual(restored, original_publisher)
        self.assertIn(b'draft: true', publisher)


if __name__ == '__main__':
    unittest.main()
