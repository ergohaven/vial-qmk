# SM62 SINGLE production-backed macro-storage regression

Requires Python 3 (assertions enabled), GCC, and Linux native `mmap` at
`0x10000000`. No SDK, submodule checkout, keyboard or ARM build is required.
This relocates the existing `sm62-old-macro6066`
`test_macro_storage.py` / `test_macro_storage.c` harness and retains its original
campaigns. A subsequent focused regression covers the firmware repair that
keeps an interrupted legacy import muted before an overlay is committed.

```sh
python3 /path/to/tests/ergohaven/sm62_macro_storage/test_macro_storage.py \
  --source-root /absolute/path/to/firmware-source \
  --output-dir /absolute/external/existing-parent/fresh-run
```

Both arguments are required. Output must be a nonexistent absolute path with an
existing parent, outside the selected source/repository and this package's
repository. Existing paths (including dangling symlinks), resolved symlink paths
inside a protected tree, and ancestors of protected trees are rejected before
any output is created. Never use Python `-O` or `PYTHONOPTIMIZE`.
The source may be a publication worktree or a plain exported source tree.

On the Ben host, use the enrolled project and managed build cache:

```sh
/home/agent/.local/bin/agent-storage run --agent ben \
  --project work/sm62/entropy-sm62-multi-device/publication-0aaa9947/firmware-source \
  -- bash -c 'python3 tests/ergohaven/sm62_macro_storage/test_macro_storage.py --source-root "$PWD" --output-dir "$AGENT_BUILD_CACHE/macro-storage-fresh-run"'
```

Use a fresh output basename for each admitted run; do not bypass storage gates.
All generated C, hardware headers, executable, compiler log, reboot image,
source-identity and result JSON stay in that output directory. Nothing writes
back to source/package. The caller can capture stdout/stderr and exit status
alongside it in managed cache. Cache evidence is disposable, not a durable
release artifact.

## Production boundaries and fixed qualification target

The runner still extracts actual NVM bridge functions, macro access/playback,
Vial handler, storage mutation classifier and the **complete** VIA
`raw_hid_receive`. It includes actual `macro_storage.c`,
`macro_storage_flash.c`, `flash_guard.c`, and links `flash_gate.c`.
Only OS/MMIO/NOR/HID, time/interrupt delivery and unrelated dispatcher branches
are substituted. GCC retains `-Wall -Wextra -Werror`, optimization and assertions.
The original C campaign/assertion functions are retained unchanged. The added
`legacy_abort_marker` control checks valid legacy playback, interrupted-import
refusal across reboot, unchanged GET/EEPROM and explicit overlay update/reset
recovery. `--legacy-abort-only` runs that focused control; the default still
runs it plus every original campaign and both independent process invocations.

`single_config.json` strictly pins selected production SINGLE config, parent
config, keyboard descriptor, v1 config/rules, macro/extension declarations,
Vial declarations and NVM layout declaration prefix. Any drift fails closed
before creating output; these hashes are intentionally restrictive, not a
promise of compatibility with arbitrary future configurations. They validate
the fixed qualification geometry/counts against selected production inputs,
not against a returned result. The fixed historical EEPROM fixture is
8192 bytes, macro prefix [2968,8192), 109 macros, logical pool 6066 bytes,
4 MiB NOR, two 8192-byte banks at 0x2ca000, data offset 256 and marker 6400.
These are the previously qualified SINGLE layout, not native `sizeof` guesses
or recomputed expectations from GET replies. Changes to layout require an
explicitly reviewed fixture/pin update, not blindly refreshing hashes.
The Vial-header pin includes the reviewed export of `vial_keycode_firewall`;
no geometry/count declarations changed. The runner extracts that real helper
before its dispatcher callers and takes both boot-key declarations from
`quantum/keycodes.h`, rather than substituting a firewall stub.

One native scaffolding correction on relocation: `MATRIX_COLS` is 7, not the
old harness's 6; the actual production descriptor declares seven pins per row
and five rows per half (10 split rows). Descriptor checks retain that geometry.
This does not change any macro campaign or production bytes.

## Unchanged acceptance controls

- Full 6066-byte public import/readback and fresh-RAM recovery; all 109 playback
  slots including a 5957-character macro; last-byte/sentinel semantics.
- Verified equal SET/reset no-wear, bounds/locked/owner denial/read lease and
  byte-identical unrelated EEPROM/flash.
- Actual VIA→Vial TD exclusion across 256 indices and malformed lengths 0..31.
- Three campaigns: migration crossing legacy/suffix, unaligned update with two
  committed banks, full reset. Every byte cut in two sector erases and 26 page
  programs, plus completed-operation boundary: 44,547 cut controls total.
- Explicit/silent I/O failures, read failures, every bit of header/payload/marker,
  corrupt equality refusal and generation rollover: 52,710 fault controls.
- Separate process image save/load and full public GET with zero writes.

`source-identity.json` records hashes of direct production inputs and boundary
headers before compilation; `result.json` records actual compiler command,
executable digest, three child outputs/exit codes and byte-for-byte production
input stability after successful execution. Failures remain nonzero, not PASS.
This is native production-backed regression evidence only: not a full firmware
suite, independent review, ARM build readiness or physical power-cycle proof.
