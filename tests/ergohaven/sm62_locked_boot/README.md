# Locked QK_BOOT ingress and immutable resident recovery

## Run

Run from the repository root with Python 3, native GCC and an external build
cache. In managed agent environments, use the owning project's storage wrapper
to provide `AGENT_BUILD_CACHE` and enforce its budget.

```sh
python3 tests/ergohaven/sm62_locked_boot/run.py \
  --output-dir "${AGENT_BUILD_CACHE:?}/locked-boot-next-run"
```

Use a fresh absolute output basename with an existing parent; existing paths,
source paths/ancestors and optimized Python are rejected before creating output.
The runner retains generated C, compile log and `result.json` with source/test
SHA-256, compile command and real per-case exits/stdout/stderr. All outputs are
outside sources. The real exported firewall is compiled in a separate native TU
using its actual declaration, not a copied policy. GCC uses warnings-as-errors
and UBSan.

## Assignment and recovery policy

- Export existing `vial_keycode_firewall` through `quantum/vial.c` / `vial.h`.
- Use it at external VIA single-key SET, not in internal NVM setters.
- At application COMMIT, first validate the original completeness/revision/CRC.
  Then reject the entire snapshot if any keycode or stack direction would be
  changed by the firewall. Do not rewrite semantic/raw CRC or resident identity.
- Gate before the compact validation-only success path, covering stage-only
  resident import and STORE as well as direct/compact publication and SELECT.
- Preserve existing bulk FFFF policy and internal/factory NVM paths.
- Keep immutable record-integrity checks independent of the current lock state.
  Only synchronous `cache_record_valid` sets/restores the process-local integrity
  flag. COMMIT skips the assignment firewall only when both integrity scope and
  validation-only mode are set; no live publication occurs in this scope.
- The single-key firewall call is conditional on `VIAL_ENABLE`; VIA-only builds
  keep their existing behavior without requiring a Vial symbol.

## Coverage

- Complete actual `raw_hid_receive` switch -> native real dynamic-keymap and
  EEPROM wrappers; application packets enter via its keyboard callback.
- Complete actual app engine, compact parser and schema2 resident journal.
- SINGLE8 complete transfer; CRC from independent packet-source snapshot;
  ordinary keys, unlocked QK_BOOT, invalid CRC, incomplete transfer, lease expiry.
- Every 16x62 layer/control position and 16x4x2 stack position, including inactive
  metadata; active four-slot stack refusal and unlocked/ordinary controls.
- Compact raw CRC rejection, immediate publication refusal and stage-only import;
  real journal STORE/SELECT using a NOR boundary model, no-wear and raw-byte
  equality controls; READY -> relock -> STORE revalidation; warm/cold SELECT;
  inactive stack import/SELECT refusal.
- Assertions require zero bootloader calls. The cases cover assignment,
  readback and publication, not action_exec behavior, a host-only exploit or
  a physical bootloader jump.

## Cold-scan behavior

The regression cold-scans an intact stored QK_BOOT record while locked. SELECT
is refused, but QUERY/ENUM/full READ retain its exact bytes without writes.
Unlock then permits SELECT without manual rescan or flash mutation. STORE and
stage-only import still authorize the current lock independently of cold-scan
integrity; a READY token obtained while unlocked cannot bypass a later lock.

## Other native fixture dependencies

Source-extraction fixtures need the actual firewall and its `vial_unlocked`
external state ahead of their callers. In particular,
`sm62_macro_storage/test_macro_storage.py` extracts the actual firewall before
the real `vial_handle_cmd` / `raw_hid_receive`. Keep its `vial.h` pin aligned with
the actual API declaration; do not mock the helper or blind-refresh configuration
pins. Other app-engine extraction consumers must do likewise:
`run_compact_usb.py`, `run_macropad_bugs.py`, `run_macropad_wake.py`,
`run_sm62_layout.py`, `run_sm62_split.py`, `run_sm62_cached_split.py`,
`run_sm62_delta_split.py`, `run_sm62_display.py`, `run_sm62_screen.py`.
Their qualification is separate from this focused runner.

`run_via_optional.py --source-root /absolute/firmware-source --output-dir
/absolute/fresh-output` is an additional compile/runtime control for the exact
single-key VIA case and actual firewall with Vial enabled and disabled. Only its
NVM setter is a recorder; this small control is not the complete VIA dispatcher.

## Limits

VIA uses native EEPROM geometry 10x7x16, not the SM62 flash-mutation gate or macro
overlay. App parser uses SM62 SINGLE8 and compact raw codec; compressed codec and
other boards' parser geometries are not behaviorally qualified. Time, layers,
action sinks and EEPROM/NOR are modeled hardware boundaries. Complete factory
reset, whole firmware/ARM linkage, Rust interop, physical HID and bootloader
execution are not qualified by this focused native runner. No Git writes,
network or hardware access is performed by it.
