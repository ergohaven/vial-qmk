"""Release integration contracts; templates remain C-preprocessor input, not JSON."""
import importlib.util
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
    files = '            release_assets/*.uf2\n            release_assets/*.zip'
    if text.count(files) != 1:
        raise AssertionError('Expected only the reviewed UF2/ZIP release globs')
    return text.replace(files, '            ergohaven_fw/**/*.uf2', 1)


class ReleaseScope(unittest.TestCase):
    def test_sm62_is_required_in_complete_official_matrix(self):
        scope = scope_module()
        text = (ROOT / scope.WORKFLOW).read_text()
        targets = [tuple(part.strip() for part in match.groups())
                   for line in text.splitlines() if (match := scope.MATRIX_ENTRY.match(line))]
        self.assertIn(('ergohaven/sm62', 'v1'), scope.REQUIRED_RELEASE_TARGETS)
        self.assertEqual(set(targets), scope.REQUIRED_RELEASE_TARGETS)
        self.assertEqual(len(targets), 21)
        self.assertEqual(len(set(targets)), 21)
        names = [kb.replace('/', '_') + '_' + km + '.uf2' for kb, km in targets]
        self.assertEqual(len(set(names)), 21)
        for target in [('ergohaven/hpd/rev2', 'v2_lcd_ball'), ('ergohaven/macropad/rev2', 'v2_ccw')]:
            self.assertNotIn(target, targets)
            self.assertIn(scope.find_vial_definition(*target), scope.EXCLUDED_KEYMAPS)
        self.assertEqual(len(scope.EXCLUDED_KEYMAPS), 5)
        retired = {
            ("ergohaven/k03pro/rev1/43mm", "v1"),
            ("ergohaven/k03pro/rev1/65mm", "v1"),
            ("ergohaven/hpd/rev1", "v1"),
            ("ergohaven/imperial44/rev1", "v1_v2"),
            ("ergohaven/k02", "v1"),
            ("ergohaven/k03/rev1", "v1_v2"),
            ("ergohaven/macropad/rev1", "v1"),
            ("ergohaven/planeta/rev1", "v1"),
            ("ergohaven/planeta/rev2", "v2"),
            ("ergohaven/remnant", "v1"),
            ("ergohaven/velvet/rev1", "v1"),
        }
        self.assertEqual(scope.RETIRED_RELEASE_TARGETS, retired)
        self.assertTrue(retired.isdisjoint(targets))
        archive = scope.verify_archived_sources()
        self.assertEqual(archive['snapshot_commit'], "8bd528239a8ae1bc0747af53a6f0ba5a32f63fbc")
        self.assertEqual(len(archive['files']), 131)
        for target in retired:
            self.assertIsNone(scope.find_vial_definition(*target))
            self.assertTrue((ROOT / scope.find_archived_vial_definition(*target)).is_file())
        result = subprocess.run([sys.executable, 'util/ci_ergohaven_release_scope.py'],
                                cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class ReleaseIdentity(unittest.TestCase):
    def test_all_independent_version_owners_are_aligned(self):
        root = ROOT / 'keyboards/ergohaven'
        self.assertRegex((root / 'config.h').read_text(), r'#define EH_FIRMWARE_VERSION 0x00040200\b')
        self.assertIn('#define EH_VERSION_STR "4.2.0"', (root / 'ergohaven.h').read_text())
        definitions = sorted(root.glob('**/vial.json'))
        self.assertEqual(len(definitions), 26)
        # Keep strings (including comment-like text) intact while stripping comments.
        token = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/', re.S)
        for path in definitions:
            with self.subTest(owner=path.relative_to(ROOT)):
                preamble = path.read_text().split('#include', 1)[0]
                preamble = token.sub(lambda m: m[0] if m[0].startswith('"') else '', preamble)
                firmware = re.findall(r'"firmware"\s*:\s*\{([^{}]*)\}', preamble)
                self.assertEqual(len(firmware), 1)
                self.assertEqual(re.findall(r'"version"\s*:\s*"([^"]+)"', firmware[0]), ['4.2.0'])
                self.assertEqual(re.findall(r'"firmwareVersion"\s*:\s*"([^"]+)"', preamble), ['4.2.0'])
        usb = []
        for path in sorted(root.glob('**/keyboard.json')):
            versions = re.findall(r'"device_version"\s*:\s*"([^"]+)"', path.read_text())
            if versions:
                usb.append(path)
                with self.subTest(owner=path.relative_to(ROOT)):
                    self.assertEqual(versions, ['4.2.0'])
        self.assertEqual(len(usb), 14)


class ReleasePublication(unittest.TestCase):
    def test_420_notes_keep_rmk_sections_and_protected_history(self):
        path = 'keyboards/ergohaven/changelog_ru.md'
        text = (ROOT / path).read_text()
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
