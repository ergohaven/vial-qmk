#!/usr/bin/env python3
"""Verify the Ergohaven-only source tree, frozen archive and release policy."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


WORKFLOW = Path(".github/workflows/release-validation.yml")
KEYBOARDS_ROOT = Path("keyboards")
ERGOHAVEN_ROOT = KEYBOARDS_ROOT / "ergohaven"
MATRIX_ENTRY = re.compile(r"^\s*-\s*\{\s*kb:\s*([^,]+),\s*km:\s*([^}]+)\}\s*$")

# Maintainer keymaps and excluded variants of supported devices stay active.
# Retired device sources are frozen separately, outside QMK discovery.
EXCLUDED_KEYMAPS = {
    ERGOHAVEN_ROOT / "macropad/keymaps/v2_ccw/vial.json",
}

# Permanently retired generations must stay outside live keyboard discovery.
RETIRED_RELEASE_TARGETS = {
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

# This is the complete official release matrix. Keeping it independent from
# the workflow makes accidental target removal a CI failure rather than a
# silently smaller release.
REQUIRED_RELEASE_TARGETS = {
    ("ergohaven/macropad/rev2", "v2"),

    ("ergohaven/macropad/rev3", "v3"),
    ("ergohaven/phenom/rev1", "v1"),
    ("ergohaven/phenom_micro/rev1", "v1"),
    ("ergohaven/phenom_mini/rev1", "v1"),
    ("ergohaven/sm62", "v1"),
}


ARCHIVE_ROOT = Path("archive/ergohaven")
ARCHIVE_MANIFEST = ARCHIVE_ROOT / "manifest.json"
# Intentional archive additions require review; never refresh this pin to hide a mismatch.
ARCHIVE_MANIFEST_SHA256 = "259443b49ce8040a3159b3b88bd54ea165b4251f155525a74ef04f96a415da8b"


def _archive_manifest():
    raw = ARCHIVE_MANIFEST.read_bytes()
    if hashlib.sha256(raw).hexdigest() != ARCHIVE_MANIFEST_SHA256:
        raise ValueError("Frozen archive manifest changed")
    return json.loads(raw)


def find_archived_vial_definition(keyboard, keymap):
    for target in _archive_manifest()["targets"]:
        if (target["keyboard"], target["keymap"]) == (keyboard, keymap):
            return Path(target["definition"])
    return None


def verify_archived_sources():
    manifest = _archive_manifest()
    targets = [(row["keyboard"], row["keymap"]) for row in manifest["targets"]]
    if manifest["schema"] != 1 or len(targets) != len(set(targets)) or set(targets) != RETIRED_RELEASE_TARGETS:
        raise ValueError("Frozen archive target inventory changed")
    expected = set()
    originals = set()
    snapshots = {}
    for snapshot in manifest.get("source_snapshots", []):
        path = Path(snapshot["path"])
        path.relative_to(ARCHIVE_ROOT)
        if (".." in path.parts or path in expected or snapshot["id"] in snapshots
                or not re.fullmatch(r"[0-9a-f]{40}", snapshot["base_commit"])
                or snapshot["mode"] != "100644" or path.is_symlink() or not path.is_file()):
            raise ValueError("Unsafe or duplicated archive source snapshot")
        if (hashlib.sha256(path.read_bytes()).hexdigest() != snapshot["sha256"]
                or path.stat().st_mode & 0o111):
            raise ValueError("Frozen archive source snapshot changed: " + str(path))
        expected.add(path)
        snapshots[snapshot["id"]] = snapshot
    for record in manifest["files"]:
        path = Path(record["path"])
        original = Path(record["original_path"])
        path.relative_to(ARCHIVE_ROOT)
        original.relative_to(ERGOHAVEN_ROOT)
        if ".." in path.parts or ".." in original.parts or path in expected or original in originals:
            raise ValueError("Unsafe or duplicated archive mapping")
        expected.add(path)
        originals.add(original)
        if path.is_symlink() or not path.is_file():
            raise ValueError("Missing or unsafe frozen archive file: " + str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("Frozen device source changed: " + str(path))
        mode = "100755" if path.stat().st_mode & 0o111 else "100644"
        if mode != record["mode"]:
            raise ValueError("Frozen source mode changed: " + str(path))
        if original.exists():
            raise ValueError("Retired source restored into active tree: " + str(original))
        if "source_snapshot" in record:
            snapshot = snapshots.get(record["source_snapshot"])
            if snapshot is None or record.get("snapshot_commit") != snapshot["base_commit"]:
                raise ValueError("Missing or inconsistent archive source snapshot")
    metadata = {ARCHIVE_MANIFEST, ARCHIVE_ROOT / "README.md", ARCHIVE_ROOT / "AGENTS.md"}
    actual = {path for path in ARCHIVE_ROOT.rglob("*") if path.is_file() or path.is_symlink()} - metadata
    if actual != expected:
        raise ValueError("Frozen archive file inventory changed")
    if any(Path(row["definition"]) not in expected for row in manifest["targets"]):
        raise ValueError("Retired target missing its archived definition")
    for target in manifest["targets"]:
        if "source_snapshot" in target:
            snapshot = snapshots.get(target["source_snapshot"])
            if snapshot is None or target.get("snapshot_commit") != snapshot["base_commit"]:
                raise ValueError("Retired target has an inconsistent source snapshot")
    return manifest


def verify_keyboard_namespace():
    entries = {path.name for path in KEYBOARDS_ROOT.iterdir()}
    if entries != {"ergohaven"}:
        raise ValueError("keyboards/ must contain only ergohaven/; found: " + ", ".join(sorted(entries)))
    if ERGOHAVEN_ROOT.is_symlink() or not ERGOHAVEN_ROOT.is_dir():
        raise ValueError("keyboards/ergohaven must be a real directory")


def find_vial_definition(keyboard, keymap):
    keyboard_dir = KEYBOARDS_ROOT / keyboard
    while keyboard_dir != KEYBOARDS_ROOT:
        candidate = keyboard_dir / "keymaps" / keymap / "vial.json"
        if candidate.is_file():
            return candidate
        keyboard_dir = keyboard_dir.parent
    return None


def main(*, matrix=False):
    try:
        verify_keyboard_namespace()
    except (OSError, ValueError) as error:
        print("Invalid product keyboard namespace: {}".format(error))
        return 1
    try:
        verify_archived_sources()
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Invalid frozen device archive: {}".format(error))
        return 1
    workflow_text = WORKFLOW.read_text(encoding="utf-8")
    targets = []
    for line in workflow_text.splitlines():
        match = MATRIX_ENTRY.match(line)
        if match:
            targets.append((match.group(1).strip(), match.group(2).strip()))

    if not targets:
        print("No Ergohaven release targets found in {}".format(WORKFLOW))
        return 1

    target_set = set(targets)
    retired_targets = target_set & RETIRED_RELEASE_TARGETS
    if retired_targets:
        print("Retired targets must not be built or published:")
        for keyboard, keymap in sorted(retired_targets):
            print("  {}:{}".format(keyboard, keymap))
    if len(targets) != len(target_set):
        print("Duplicate Ergohaven release targets found in {}".format(WORKFLOW))
        return 1

    missing_targets = REQUIRED_RELEASE_TARGETS - target_set
    unexpected_targets = target_set - REQUIRED_RELEASE_TARGETS

    if missing_targets:
        print("Required Ergohaven release targets missing from the workflow:")
        for keyboard, keymap in sorted(missing_targets):
            print("  {}:{}".format(keyboard, keymap))

    if unexpected_targets:
        print("Undocumented Ergohaven release targets found in the workflow:")
        for keyboard, keymap in sorted(unexpected_targets):
            print("  {}:{}".format(keyboard, keymap))

    represented = set()
    unresolved = []
    for keyboard, keymap in targets:
        definition = find_vial_definition(keyboard, keymap)
        if definition is None:
            unresolved.append("{}:{}".format(keyboard, keymap))
        else:
            represented.add(definition)

    definitions = set(ERGOHAVEN_ROOT.glob("**/vial.json"))
    missing = definitions - represented - EXCLUDED_KEYMAPS
    stale_exclusions = EXCLUDED_KEYMAPS - definitions

    if unresolved:
        print("Release targets without a vial.json definition:")
        for target in sorted(unresolved):
            print("  {}".format(target))

    if missing:
        print("Ergohaven Vial keymaps missing from the release policy:")
        for path in sorted(missing):
            print("  {}".format(path))

    if stale_exclusions:
        print("Stale Ergohaven release exclusions:")
        for path in sorted(stale_exclusions):
            print("  {}".format(path))

    if retired_targets or missing_targets or unexpected_targets or unresolved or missing or stale_exclusions:
        return 1

    if matrix:
        print(json.dumps({'include': [{'kb': kb, 'km': km} for kb, km in targets]}))
    else:
        print(
            "Verified {} release targets, {} Vial definitions and {} intentional exclusions.".format(
                len(targets), len(definitions), len(EXCLUDED_KEYMAPS)
            )
        )
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", action="store_true", help="Emit the verified official build matrix as JSON")
    sys.exit(main(matrix=parser.parse_args().matrix))
