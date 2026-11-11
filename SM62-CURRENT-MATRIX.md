# Historical SM62 framebuffer + current matrix GPIO

This branch is a child of `archive/sm62-old-rendering-base-1e2857fe9e`
(`60441b21f85f3e76d43e848c6ba42bc7dd94f4e9`), originally based on
`1e2857fe9ebb3e2dcd3468a1803ac5b4a330de49`. It is **not exact test13**,
not resident100, and not the newer experimental core1 renderer.

## Entire production delta

Only `keyboards/ergohaven/sm62/keyboard.json` changes:
- `matrix_pins.cols[2]`: `GP15` -> `GP23`
- `matrix_pins.cols[4]`: `GP20` -> `GP25`

Columns: GP21 GP22 GP23 GP24 GP25 GP26 GP27.
Rows: GP28 GP19 GP18 GP17 GP16. COL2ROW; GP29 handedness unchanged.
All logical coordinates/layout JSON match the actual current board source at
`5b64017342e7de53049c525879d822a18b3e67f2`. Renderer, libraries,
SPI settings, scheduling, flash geometry and storage policies are unchanged.
All nine dependencies remain ordinary files; **do not initialize submodules**.
Historical `PACKAGE-*` files describe the original archive, not this derivative.

## Verification performed locally

- Test-first RED: old production columns fail the explicit current-board assertion.
- GREEN: `tests/sm62_matrix_port.py` checks actual JSON, GP29, generates the real
  QMK keyboard header, compiles its production LAYOUT with `cc -Wall -Wextra -Werror`,
  and checks all 62 key identities in all 70 matrix cells (8 holes, both halves).
- Normal top-level `make -j8 ergohaven/sm62:v1 VERBOSE=true SKIP_GIT=yes`,
  xPack ARM GCC 15.2.1; explicit managed-cache BUILD_DIR, output-copy and map routing.
  Exit 0. No feature-disabling or renderer modifications.
- Actual normal ARM ELF `col_pins` and `row_pins` data equal the requested GPIOs.
- UF2: 7,822 RP2040-family blocks; headers, numbering, unique addresses, bounds,
  vector stack/entry and every ELF load payload byte validated. Extra padding zero.
  Flash interval: `[0x10000000,0x101e8e00)` within historical 2 MiB.
- Local UF2 SHA256: `8131273afa9addae5ead0e44407c3f7253c76640510b27f18dcfd916f1cdad0c`.
  Build origin: **local, not GitHub CI**. Binary intentionally not published/sent.
- Independent source comparison reads the original ZIP directly (SHA256
  `14698ba958ac5238fafa18ad14302173fb17b38114988b2bd2bb81215a4111e2`):
  all 44,179 original payload files compared; exactly the two GPIO string tokens
  differ, all remaining bytes/modes retained. Canonically serialized full
  path/size/mode/SHA256 map digest:
  `db43d90cea021b3f39dfd2392ccc9e1cc38c59725c257d9c6c2ea64d070e9e60`.
  Test and this document are additional verification-only files.

Reproduce geometry with QMK and a C compiler installed:
```
python3 tests/sm62_matrix_port.py --source "$PWD" --output /absolute/external/build-cache/native
```
Use the owner's managed build wrapper for compilation. Original archive line-ending
attributes can make a Git checkout normalize unrelated dependency files; qualification
restored their exact ZIP bytes and did not stage those paths. No dependency edits
belong to this port.

## Important limits

This is an archival matrix-only comparison image, **not a preservation-safe update
from resident100**. Historical 2 MiB flash addressing and four TD/Combo/KO entries
remain, versus the later target/capacities. Historical VIA validity uses the random
BUILD_ID and can reset dynamic settings on boot; old factory post-init heuristics
also remain. No migration, storage reset, flashing or hardware test was performed.
Do not infer that unchanged pins, a successful build, or software geometry proves
hardware acceptance, settings preservation, or improved animation speed.

Per the current request, deliver only the branch link, not its UF2. No PR, merge,
release, or updates to the original archive/main branches are part of this task.
