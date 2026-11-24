# Retired Ergohaven keyboards

This directory freezes retired device-specific sources outside QMK's live
`keyboards/` discovery tree. These devices are not built or published by the
current firmware workflows.

The snapshot was taken from commit `8bd528239a8ae1bc0747af53a6f0ba5a32f63fbc`.
`manifest.json` records every original path, archived path, file mode and SHA-256,
and maps all retired firmware targets to their preserved Vial definitions.
The release-scope validator pins the manifest and checks every archived file.

## Retired generations

- K:02, both Planeta revisions and Remnant
- HPD v1, Imperial44 v1/v2 and K:03 v1/v2
- K:03 Pro v1, including both 43 mm and 65 mm variants
- Macropad v1 and Velvet v1

Shared code still used by supported keyboards remains in the active tree.
The OLED game formerly stored under K:02 was extracted byte-for-byte to
`keyboards/ergohaven/src/oled/game/`; supported default keymaps use that owner,
not this archive.

## Historical builds

The archived files are historical source, not a buildable keyboard namespace.
Their original includes and build rules intentionally remain unchanged. For a
historical build, obtain the complete pinned commit above and its pinned
submodules in a separately authorized workspace. Do not compile archived files
against the moving current shared firmware or restore them into the active tree.

Do not reformat, version-bump or otherwise maintain these snapshots. A deliberate
archive change requires a separate maintainer decision; do not refresh manifest
hashes merely to make a failed check pass. Published historical releases are not
removed by this source reorganization.
