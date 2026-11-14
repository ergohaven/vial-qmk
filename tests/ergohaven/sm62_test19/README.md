# SM62 TEST19: current-production rows28 KEYMAP regression

Portable source-only native characterization. Requirements: Linux/Python 3, GCC,
pthreads, UBSan, and the delivered firmware source tree including its exact
LVGL dependency. No Git history, external fixture tree, prebuilt `.o`, or firmware
build is required. The runner never initializes submodules or changes sources.

```sh
python3 /absolute/path/to/portable-tests/run_rows28.py \
  --repo-root /absolute/path/to/firmware \
  --output /absolute/path/outside-firmware/fresh-rows28-run
```

`--repo-root` is mandatory and resolved explicitly (relative roots are resolved
from the invoking directory). `--output` is mandatory, absolute, nonexistent,
and outside both the firmware root and this package. All generated C/headers,
LVGL instrumentation copies, objects, executable, commands, logs, raw panel
captures and `result.json` go there. Failed runs retain their external artifacts
for diagnosis; use another fresh output directory for a rerun.

## What is exercised

* Read the **actual production config** and require rows=28/slice=4. Generate a
  header from those values, not an embedded rows4 geometry.
* Generate fixture TUs from current production renderer, render-owner, exchange,
  display policies, formatting, deferred executor and QP source. Strip only
  include/pragma lines to use the native hardware-boundary declarations, keeping
  source bodies. Pull `qp_internal_task` from the current QP translation unit.
* Compile production fonts and LVGL, adding six counters only in output copies
  and one panel-flush counter in generated QP. Keep the prior compressed-font
  `-fno-sanitize=shift` exception local to `lv_font_fmt_txt.c`; the other native
  code retains UBSan and warnings-as-errors with the existing harness warning
  exclusions.
* Run full KEYMAP repaint, first press and next-key focus on left and right
  geometries, master role. Require six exact operation-count records, six exact
  panel SHA256 references, changed pixels after key events, and completed
  cooperative refreshes with a 240×28 draw buffer.
* Validate pinned LVGL revision `e19410f8f8a256609da72cff549598e0df6fa4cf`,
  **all 606 base blobs**, exactly five base-checked overlays, and only the
  approved timer replacement (`e7ba614ada69d7ae8e7fd82ceeee7ef3dd941c53`,
  SHA256 `92eb59b8cdeb0aae6f808227431a287d874b4d38671804869abb338ee1313513`).
  Validator checks are unchanged; only manifest locations are relocated.
  Original `macropad_native_support.py` is neither patched nor replaced.
* Hash production C/headers before and after, requiring unchanged identity.
  `result.json` contains the source identity map and actual measurements.

This is characterization of already accepted behavior, not a new bug repair or
new test-first RED/GREEN claim. Expected records and panel hashes are extracted
from existing iteration14 rows28 evidence, not regenerated from the tested run.

## Inputs and identity boundaries

`inputs/main.c` is the geometry-probe native hardware/input boundary and scenario;
`inputs/generate_fixture.py` rebuilds its production-backed TUs from the explicit
repo root. No copied historic production renderer bodies are shipped. It supplies
synthetic data, panel transport, clock and native declarations. Its input-edge
helper invokes the real `sm62_render_input` and `sm62_render_capture`; this does
**not** exercise the production debounce scanner. No old `scanner.o` is linked.

`inputs/expected_rows28.json` contains the six reference measurements and six
panel hashes. The three other JSON inputs pin LVGL and approved timer identities;
`inputs/validate_lvgl.py` performs those checks. `provenance.json` records origin,
input hashes and the reference production identity boundaries. Production hashes
there are provenance, not a whole-tree freeze gate, so deliberate future source
changes can run against the same behavioral assertions.

The original iteration14 rows4-versus-rows28 experiment and iteration13 full
panel/recovery suite remain **external evidence**. This runner reproduces the
current28 KEYMAP subset and compares it to the recorded hashes; it does not rerun
rows4, all screen/recovery scenarios, slave roles, hardware, physical timing,
stack measurements, RPC or flash workflows. Deterministic host scheduling and
operation counts are not physical millisecond measurements or causality proof.

## Verified local execution

Executed once against `iteration15-rows28/firmware` with output at
`iteration16-checkpoint/portable-rows28-verification`, exit **0**:

```text
PASS exact pinned revision + 606 blobs + five base-checked overlays; only approved timer replacement
PASS rows=28 side=1 role=1
PASS rows=28 side=0 role=1
PASS current production rows28: 6 exact operation-count cases, 6 panel SHA256 references; production C/header identity unchanged
```

For each side, full repaint and first press used 10 panel parts, 67200 pixels,
and 3 worker steps; focus used 1 part, 4761 pixels, and 1 worker step. Exact
function counts and hashes are in the reference input and external `result.json`.
No generated outputs belong in this package or the eventual commit.
