# Focused Macropad native regressions

Requirements: Python 3, GCC with UBSan, Git, and an **external** LVGL checkout
containing commit `e19410f8f8a256609da72cff549598e0df6fa4cf` and its exact C/header
contents. Both LVGL runners validate every pinned C/header blob before compiling.
Do not initialize or modify canonical QMK submodules just to run these probes.
Output directories belong outside the source tree. Run from the QMK root:

```sh
export PYTHONDONTWRITEBYTECODE=1
LVGL=/path/to/external/pinned/lvgl
OUT=/path/to/scratch/macropad-controls
python3 tests/ergohaven/run_macropad_bugs.py "$OUT/default-labels" --only 004 --no-app-layout --lvgl "$LVGL"
python3 tests/ergohaven/run_macropad_bugs.py "$OUT/app-labels" --only 004 --lvgl "$LVGL"
python3 tests/ergohaven/run_macropad_wake.py "$OUT/wake" --lvgl "$LVGL"
```

The default-label test compiles the complete shared formatter body plus the
production bitmap selector and label setter, without `EH_APP_LAYOUT_ENABLE`.
It checks two-row modifier/base-key geometry and lower-row pixels, ordinary
numeric transitions, and the existing immutable Backspace/Delete golden bytes.
The application-layout control retains static, uniformly scaled one-line and
frame-equality assertions with the production fonts and real pinned LVGL.

To reproduce the independent-review label failure against the delivered source,
keep the current tests and add `--ref 8f6840b4f7cf28abccd3576f1af0754600753b3a`
to the default-label invocation. Expected RED: Ctrl+M is 22px high despite its
newline; the two-line-height assertion fails. Working-tree GREEN: 44px high,
base-key pixels visible. This option reads production code through `git show`
and does not change the worktree.

The wake runner is self-contained: it creates hardware headers, compiles the
real HID packet consumer/display bodies/screen load and refresh bodies, and
executes the committed deterministic scenario generator. No earlier runner,
stdin input, desktop executable, private capture or hardware is needed. Each
scenario runs in a fresh process; generated 32-byte protocol-v7 inputs and
individual result logs are retained in the output directory. CRC-16 is computed
independently in Python. The runner also requires direct empty-input execution
to fail, and requires a positive executed-check count for every scenario.

Scenarios cover semantic transitions and identical replay, content-only changes,
keepalive, legacy packets, disabled/manual-off precedence, deactivate/expiry and
same-identity reconnect, Default transition, manual key/non-volume/volume encoder
controls, volume overlay precedence and the near-expired LCD timer. Current
expected result: 9 scenarios / 27 checks, plus empty-input rejection.

These are native source-backed regressions, not physical USB/panel tests.
Clock, hardware, storage, physical event timestamps and HID transport boundaries
are injected. No firmware release build or package is produced.
