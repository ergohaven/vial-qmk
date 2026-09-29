#!/usr/bin/env python3
"""Verify that every Ergohaven Vial keymap has an explicit release policy."""

from pathlib import Path
import re
import sys


WORKFLOW = Path(".github/workflows/build-ergohaven.yml")
KEYBOARDS_ROOT = Path("keyboards")
ERGOHAVEN_ROOT = KEYBOARDS_ROOT / "ergohaven"
MATRIX_ENTRY = re.compile(r"^\s*-\s*\{\s*kb:\s*([^,]+),\s*km:\s*([^}]+)\}\s*$")

# Maintainer keymaps are kept in-tree for development and personal builds, but
# are intentionally excluded from official Ergohaven release artifacts.
EXCLUDED_KEYMAPS = {
    ERGOHAVEN_ROOT / "hpd/keymaps/kissetfall/vial.json",
    ERGOHAVEN_ROOT / "imperial44/keymaps/kissetfall/vial.json",
    ERGOHAVEN_ROOT / "k02/keymaps/kissetfall/vial.json",
    ERGOHAVEN_ROOT / "k03/keymaps/kissetfall/vial.json",
    ERGOHAVEN_ROOT / "planeta/keymaps/kissetfall/vial.json",
    ERGOHAVEN_ROOT / "remnant/keymaps/kissetfall/vial.json",
}

# This is the complete official release matrix. Keeping it independent from
# the workflow makes accidental target removal a CI failure rather than a
# silently smaller release.
REQUIRED_RELEASE_TARGETS = {
    ("ergohaven/hpd/rev1", "v1"),
    ("ergohaven/hpd/rev2", "v2"),
    ("ergohaven/hpd/rev2", "v2_ball_enc"),
    ("ergohaven/hpd/rev2", "v2_enc_ball"),
    ("ergohaven/hpd/rev2", "v2_enc_enc"),
    ("ergohaven/hpd/rev2", "v2_enc_joy"),
    ("ergohaven/hpd/rev2", "v2_enc_touch"),
    ("ergohaven/hpd/rev2", "v2_lcd_ball"),
    ("ergohaven/hpd/rev2", "v2_touch_enc"),
    ("ergohaven/imperial44/rev1", "v1_v2"),
    ("ergohaven/imperial44/rev3", "v3_v4"),
    ("ergohaven/k02", "v1"),
    ("ergohaven/k03/rev1", "v1_v2"),
    ("ergohaven/k03/rev3", "v3_v4"),
    ("ergohaven/k03pro/rev1/43mm", "v1"),
    ("ergohaven/k03pro/rev1/65mm", "v1"),
    ("ergohaven/k03pro/rev2", "v2_v3"),
    ("ergohaven/macropad/rev1", "v1"),
    ("ergohaven/macropad/rev2", "v2"),
    ("ergohaven/macropad/rev2", "v2_ccw"),
    ("ergohaven/macropad/rev3", "v3"),
    ("ergohaven/phenom/rev1", "v1"),
    ("ergohaven/phenom_micro/rev1", "v1"),
    ("ergohaven/phenom_mini/rev1", "v1"),
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


def find_vial_definition(keyboard, keymap):
    keyboard_dir = KEYBOARDS_ROOT / keyboard
    while keyboard_dir != KEYBOARDS_ROOT:
        candidate = keyboard_dir / "keymaps" / keymap / "vial.json"
        if candidate.is_file():
            return candidate
        keyboard_dir = keyboard_dir.parent
    return None


def main():
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

    if missing_targets or unexpected_targets or unresolved or missing or stale_exclusions:
        return 1

    print(
        "Verified {} release targets, {} Vial definitions and {} intentional exclusions.".format(
            len(targets), len(definitions), len(EXCLUDED_KEYMAPS)
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
