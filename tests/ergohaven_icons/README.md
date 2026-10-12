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
python3 tests/ergohaven_icons/test_precedence.py 12816bc441  # expected RED
python3 ../entropy-pictograms-fix/tools/test_layer_icon_artwork.py
python3 tools/generate_layer_navigation_icons.py --entropy-assets ../entropy-pictograms-fix/assets
```

Parity was RED for all eight firmware/generator/display/picker combinations
before restoration. The actual production C setter was RED on saved-icon
precedence before the NULL guards. The source-backed host fixture compiles the
entire unchanged setter with `-Wall -Wextra -Werror`; hardware storage/rendering
and integration/LVGL boundaries are stubbed. It verifies saved native 35px and
legacy 32px icons, built-in fallback, integration visual priority, ordinary
built-ins and text fallback. It does not claim physical display/HID validation.

The Entropy sync regression was RED because it accepted the baseline rhombus
header. A separate RED proved that its display assets were not synchronized.
The corrected sync validates the approved layer bytes and writes both display
and stock-picker representations. Its tests run regeneration only in scratch.
Catalog entries 0–19 remain byte-identical (prefix hash in provenance); entries
20/21 retain previous/next ordering.

Full display/native/Combo regressions and the three firmware builds are run
separately. No hardware resets, storage migrations or flashes are required.
