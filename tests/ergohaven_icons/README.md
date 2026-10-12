# Approved Equal-Corner layer pictograms

The user approved `out/Macropad-Equal-Corner-Linux-2026-10-08/pixel-preview.png`
with `layer-previous.svg` and `layer-next.svg`. The packed 154-byte bitmaps were
copied **without redrawing** from the read-only reference worktree
`vial-qmk-app-layouts-clean/keyboards/ergohaven/macropad/layer_navigation_icons.h`.
`golden/` retains independent test fixtures; `tools/artwork/layer-navigation/`
retains the generator masters. Both include the original preview/SVGs and
SHA-256 provenance. The preview test checks every enlarged RGB pixel against
the packed 35x35 masters, not just a visual approximation.

Run from the QMK root:

```sh
python3 tests/ergohaven_icons/test_parity.py
python3 tests/ergohaven_icons/test_precedence.py
python3 tests/ergohaven_icons/test_precedence.py 12816bc441  # characterization: also PASS
python3 ../entropy-pictograms-fix/tools/test_layer_icon_artwork.py
python3 tools/generate_layer_navigation_icons.py --entropy-assets ../entropy-pictograms-fix/assets
```

Parity was RED for all eight firmware/generator/display/picker combinations
before restoration. Precedence coverage is **characterization**, not proof of a
firmware defect: the corrected test passes both the current tree and
`12816bc441`. The previous getter stub returned artwork for arbitrary keycodes
and incorrectly suggested custom assignments to direct layer keys; its claimed
RED for the NULL guards did not exercise supported storage behavior.

The harness compiles the entire real `eh_pictograms.c` translation unit,
including upload, validation, cached validity, getter and width logic, plus the
unchanged production display setter. It reuses the native runner's flash model
and header setup, uses actual QMK/Ergohaven keycode definitions, and compiles
with `-Wall -Wextra -Werror` and UBSan. Flash/interrupt/timer hardware,
integration visuals, ordinary built-in lookup, rendering and LVGL are test
boundaries; it does not claim physical display/HID or Entropy chooser validation.

- Upload the exact approved previous/next 35px bytes through the production
  slot HID handler to Macro 0/127 and Tap Dance 0/255; reboot, compare every byte
  returned by the real getter, and exercise the real setter's stored-image path.
- Storage exposes 256 slots per kind, but actual QMK Macro keycodes stop at
  `QK_MACRO_MAX` (slot 127). Macro storage slot 255 can be uploaded but is not
  returned for `QK_MACRO + 255`. Reject slot 256 and invalid kinds; check empty
  slots and adjacent out-of-range keycodes return NULL.
- Direct `LAYER_PREV`/`LAYER_NEXT` getters return NULL even with populated
  storage. The setter uses the approved built-in header bytes. The same artwork
  is usable in supported Macro/Tap Dance storage; no direct-layer custom
  assignment feature is implied or added.
- Seed a valid historical v3 package at the flash boundary and exercise real
  32px getter/setter reads. Its deterministic 32x32 crops are compatibility
  fixtures, not independently approved legacy artwork. Production rejects v3
  slot edits; this test does not claim a supported 32px upload.
- Integration visuals take priority over populated Macro/Tap Dance slots and
  direct-layer fallback; ordinary built-in and text fallback remain covered.

The optional revision argument loads the setter and full storage source from
that revision; current approved artwork and current test hardware remain fixed.
Artwork history is checked separately by parity, not by this characterization.

The Entropy sync regression was RED because it accepted the baseline rhombus
header. A separate RED proved that its display assets were not synchronized.
The corrected sync validates the approved layer bytes and writes both display
and stock-picker representations. Its tests run regeneration only in scratch.
Catalog entries 0–19 remain byte-identical (prefix hash in provenance); entries
20/21 retain previous/next ordering.

Full display/native/Combo regressions and the three firmware builds are run
separately. No hardware resets, storage migrations or flashes are required.
