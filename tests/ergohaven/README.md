# Ergohaven / Vial native regressions

From the repository root, with Python 3 and native GCC (no Python packages,
ARM toolchain, hardware or submodules required):

```sh
python3 tests/ergohaven/run_native.py /absolute/path/to/scratch-output
python3 util/ci_vial_verify_uid.py
# Only the secure/insecure sampler and shared flag/reset regression:
python3 tests/ergohaven/run_native.py /absolute/path/to/scratch-output --only-unlock
```

All generated C, mock headers, executables and `results.txt` go to the explicit
scratch directory. The runner uses `-std=c11 -Wall -Wextra -Werror` and UBSan,
without warning exemptions. These are standalone native hardware-mock tests,
not QMK's GoogleTest firmware-target runner (`test.mk` is intentionally absent).

The runner includes the actual flash translation units. For Vial command/task,
LCD/reset and home-indicator paths, it extracts complete current function bodies
and supplies only peripheral/dependency mocks. Static assertions check that the
matrix/reset/shared-consumer hooks still invoke those owners. Assertions describe
correct behavior, unlike the initial PR review's defect-reproduction probes.

Coverage:

- Exact 3000ms continuous physical unlock hold, delayed first poll, release and
  restart, timer wrap, host cadences from every millisecond to no polling;
  insecure build remains insecure by explicit configuration only. Native unlock
  tests extract the production state declarations as well as the complete task
  and command bodies. They also compare 20000 irregular physical samples against
  the original absolute-hold formula and cover 39 cadence/wrap/long-gap sequences,
  including delays exceeding 16-bit range and a 32-bit half-range gap. Public
  boolean flag declarations are compiled with the production definitions; the
  real dynamic-keymap reset body is tested for temporary unlock and restoration
- All seven shared-home indicators, oneshot mods, caps-word, Mac label selection,
  media/activity visibility at the actual 10000ms production timeout (included
  from `eh_display.h`) and no redundant unchanged-state redraws; macropad
  rev2/rev3 explicitly omit these widgets
- Active-screen brightness after volume load, including zero standby/main
  brightness and zero authoritative volume; inactive-volume guard
- Journal validation/debounce/reboot/rollover, legacy erased-field defaults,
  interrupted saves; reset immediately persists all date/standby defaults and
  preserves every byte outside the journal (including uploaded assets and EEPROM)
- Both 2MiB/20-frame and 4MiB/56-frame background mappings, all supported frame
  counts, seven complete CRC uploads, 32/64-byte reports, 16-bit sequence wrap,
  and speed/EEPROM sentinels
- Pictogram single-slot edits/deletion, first/header sector, straddling record,
  distant slot and final slot; slot50 edit/delete exercises all three backup
  sectors. Power fails before, halfway through and after
  every erase/program operation; failures also interrupt rollback and reboot
  again. Every unrelated slot and every byte outside the pictogram reservation
  must survive. Full collection upload/CRC rejection, CLEAR cancellation, fresh
  slot initialization and v3 legacy read/reboot without destructive migration
- Q1, both chip sizes: silently drop every live page program of slot24/slot50,
  including the final header rewrite. All 96 one-shot failures per chip must
  return FLASH_ERROR and restore the exact old package. Two persistent failures
  per chip must retain armed undo through a failed reboot recovery, then restore
  after the injected fault clears
- Q2, both chip sizes: corrupt checksum, magic, count, offsets or part of a pending
  descriptor after a real torn live erase. With an invalid live collection, each
  subsequent slot save must fail with zero flash writes and leave live/backup
  bytes untouched. Explicit bulk replacement/CLEAR still work; a valid live
  package with an incomplete descriptor is not destructively reinitialized

The flash mock enforces erase/page alignment, chip bounds and NOR 1-to-0 writes.
Torn writes model a half-completed operation, not all possible analog failure
patterns. Silent no-op programming and separate descriptor corruption are
additional fault models, not equivalent to ordinary interrupted power. No test
claims ARM linking/size, USB driver integration, real LVGL
rendering, physical scan/encoder cadence, watchdog timing or flash endurance.

## Focused Macropad БАГ-002 / БАГ-004 regressions

```sh
python3 tests/ergohaven/run_macropad_bugs.py /absolute/scratch/encoder --only 002
python3 tests/ergohaven/run_macropad_bugs.py /absolute/scratch/labels --only 004 --lvgl /path/to/initialized/lib/lvgl
```

Add `--ref d531ecf18f4226ab6de04b3b2784500f1979b67f` to compile the same
assertions against the original production source (both must fail behaviorally).
Only test fixtures remain current with `--ref`; production bodies/declarations
and fonts are read with `git show`. No source checkout is modified by the runner.
All generated code, executables and assertion output (`results.txt`) stay in
the explicit output directory. GCC and Python stdlib suffice.

002 compiles the actual pre-process hook, QMK event types/predicates, Macropad
screen transition/encoder functions and complete HID application-layout section.
Only timers, layer-state primitives, screen load/power/animation sinks, downstream
Vial dispatch and unrelated user/RuEn hooks are substituted. It covers native
event queue identity, QMK fallback, modifier-wrapped volume, layer transparency,
selected stacks, press/release, timeout and unavailable host volume. Runtime
profiles are synthetic in-memory fixtures, not a live companion session.

004 compiles the production keycode formatting functions, builtin-icon selector,
key-content renderer, label buffers and unmodified 20/28px fonts with real LVGL
software layout/rasterization. It checks a 74x43 tile, short and long combinations,
scroll frames, nonempty text pixels confined to the tile, left/right and Mac/Linux
labels, adjacent-buffer sentinels and basic/modtap/OSM/icon controls. The initialized
LVGL source must contain commit `e19410f8f8a256609da72cff549598e0df6fa4cf`;
every tracked C/header blob is checked against it before compilation. A read-only
copy of an existing initialized submodule also works. Hardware and OS detection
are synthetic; the surrounding screen/style tree is a minimal fixture, not the
full device initialization or the user's private layout. No ARM link, physical
display/USB or companion-native OS-action execution is certified.

Both probes use `-Wall -Wextra -Werror` and UBSan. Only the LVGL probe disables
GCC's `type-limits` warning for QMK's existing unsigned `IS_QK_BASIC` range macro;
it keeps all other warnings fatal. These commands do not run the broader suite.
