# Retired Ergohaven keyboards

This directory freezes retired device-specific sources outside QMK's live
`keyboards/` discovery tree. These devices are not built or published by the
current firmware workflows.

The original snapshot was taken from commit `8bd528239a8ae1bc0747af53a6f0ba5a32f63fbc`.
Standalone Trackball v1/v2 sources were added from commit
`65e965c27694a3f15c1e7bf7574f39865ca45893`, without changing the original snapshots.
`manifest.json` records every original path, archived path, file mode and SHA-256,
and maps all retired firmware targets to their preserved Vial definitions.
An entry's optional `snapshot_commit` overrides the original top-level snapshot
for that entry; Trackball file and target entries record their own snapshot.
HPD v2 was retired with its latest local corrections intact. Its file and target
entries additionally reference `source_snapshot`: check out that snapshot's
`base_commit` and apply its pinned patch from `hpd/source-snapshots/` to recover
the complete pre-archive working source, including the shared pending changes.
The patch and every archived source file are independently SHA-256 pinned.
Velvet v2/v3 and the remaining family/default sources are also archived with
their final local corrections. Their `velvet-20261125` snapshot references the
base commit and pinned patch under `velvet/source-snapshots/`, including the
shared runtime changes present in the delivered QMK firmware.
K:03 v3/v4 and Imperial44 v3/v4 share the `k03-imperial44-20261125` source
snapshot under `k03/source-snapshots/`. It preserves their latest factory D/F
unlock positions and the complete pre-archive working source against its pinned
base commit, including pending shared code and earlier archive additions.
The release-scope validator pins the manifest and checks every archived file.

## Retired generations

- K:02, both Planeta revisions and Remnant
- HPD v1, Imperial44 v1/v2 and K:03 v1/v2
- K:03 Pro v1, including both 43 mm and 65 mm variants
- Macropad v1 and the complete QMK Velvet family (v1/v2/v3)
- Standalone Trackball v1/v2 (not HPD or Phenom trackball modules)
- HPD v2, all module variants, LCD/trackball and maintainer/default keymaps
- K:03 Pro v2/v3 and Space Mission 30 (QMK), from snapshot
  `7c2ac25ef68285d386c1e2d143b0ffa9ed3a46ba`
- K:03 v3/v4 and Imperial44 v3/v4 (QMK), including their official,
  maintainer and default keymaps

Shared code still used by supported keyboards remains in the active tree.
The OLED game formerly stored under K:02 was extracted byte-for-byte to
`keyboards/ergohaven/src/oled/game/`; supported default keymaps use that owner,
not this archive.
The SM30 animation data still used by SM62 was preserved byte-for-byte at
`keyboards/ergohaven/src/display/space_mission_anim.c`. SM62 and its source-backed
fixtures use that active shared owner, never the archived SM30 device tree.

## Historical builds

The archived files are historical source, not a buildable keyboard namespace.
Their original includes and build rules intentionally remain unchanged. For a
historical build, obtain the complete snapshot commit recorded for the selected
device and its pinned submodules in a separately authorized workspace.
For an entry referencing `source_snapshot`, also apply its recorded patch before
building; a base commit alone can predate the device's final local corrections.
Do not compile archived files
against the moving current shared firmware or restore them into the active tree.

Do not reformat, version-bump or otherwise maintain these snapshots. A deliberate
archive change requires a separate maintainer decision; do not refresh manifest
hashes merely to make a failed check pass. Published historical releases are not
removed by this source reorganization.
