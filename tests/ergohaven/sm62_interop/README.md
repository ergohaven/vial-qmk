# Current-source SM62 production-C bridge

Portable successor of `sm62-complete-layouts-interop/build.py`: no dependency on that external project, frozen firmware snapshot, or another test runner's Python prefix. Run with Python 3, GCC and the complete production QMK source checkout. Build/test outputs must be fresh absolute paths within `AGENT_BUILD_CACHE`, outside the source and this package; invalid paths are rejected before directory creation. No network, Cargo, MCU or hardware is used.

```sh
/home/agent/.local/bin/agent-storage run --agent ben --project work/sm62/entropy-sm62-multi-device/publication-0aaa9947/firmware-source -- sh -c 'python3 tests/ergohaven/sm62_interop/build.py --source "$PWD" --output "$AGENT_BUILD_CACHE/interop-final" && python3 tests/ergohaven/sm62_interop/selfcheck.py --exe "$AGENT_BUILD_CACHE/interop-final/firmware-bridge" --output "$AGENT_BUILD_CACHE/interop-final/selfcheck"'
```

`build.json` records source input hashes (including compiler-discovered transitive production headers), exact compiler command, executable path and SHA256. Generated C, compiler output and all evidence stay in the managed output. Shipping SINGLE8 config is retained: legacy 109 macros / 6066 logical bytes, 16 layers, 10x7 matrix, 62 logical controls, TD0. Layout geometry is checked against production `keyboard.json`; layer/macro counts have compile-time pins. The current production `resident_cache.h` declaration and implementation are included transitively, not replaced by declarations or a scan model.

## CLI and both Rust callers

`firmware-bridge /absolute/path/to/flash.bin`: one process per image. A fresh image models blank 4 MiB NOR (FF) plus 8192 zero EEPROM bytes, **not** the factory keymap. Restarting with that image provides fresh process RAM. EOF flushes the mapping and exits.

* 64 hex characters: exact 32-byte HID payload.
* 66 hex characters: exact 33-byte OS report, accepted only with report ID `00`.
* One JSON response per line: `responses` contains every actual `raw_hid_send`, in order, plus per-request `nor_writes`/`eeprom_writes`. Empty remains empty; no synthetic echoes or success packets.
* `INFO`: actual compiled counts/offsets and process write counters.
* `KEY layer control`: actual live C key/visual/active state, diagnostic only.
* `PLAY slot`: actual production macro playback, observed output character count.
* `TICK milliseconds`: advance synthetic monotonic clock; invoke actual unlock matrix task and app-layout timeout task.
* `MATRIX row mask`: hardware-boundary matrix state, then actual unlock task. It does not set authorization flags.

This matches the command/report/JSON forms in `qmk_hid_host/production_c_tests.rs` (`SM62_C_FIRMWARE`) and `ui/resident_catalog*_c_tests.inc` / production-C catalog regressions (`SM62_PRODUCTION_C_BRIDGE`). Their actual command inventory is INFO/KEY/PLAY/TICK and hex packets. There is no TIME/STATE/BUDGET JSON command in these callers; the separate old text `run_resident100_peer.py` interface is intentionally not substituted.

## Lock and test setup

The actual production Vial global definitions are extracted from `quantum/vial.c`: **locked at startup and restart**. Actual `vial_keycode_firewall`, `vial_unlock_task`, combo helpers and public FE06/FE07/FE08 dispatch remain intact. No `VIAL_INSECURE`, unconditional unlock or duplicate firewall policy.

A test that intentionally writes macros must first exercise public physical unlock:

1. Send 32-byte FE06 (remaining bytes zero).
2. `MATRIX 1 48` (the shipping combo from the production keymap config: row1 cols5 and4).
3. `TICK 3000`.
4. Send FE07 and require first response bytes `01 00`.
5. `MATRIX 1 0`.

The Rust macro-write qualification supplies this public unlock sequence before mutation, including the 2999 ms still-locked control. Do not change the peer to unlock automatically. Safe resident profile store/select works while locked; forbidden keycodes remain governed by actual production ingress policy. Existing random-profile fixtures can legitimately hit forbidden values.

## Scope and retained boundaries

The complete public VIA dispatcher/tail, Vial handler, real keyboard callback/app engine, full NVM implementation, dynamic storage wrappers, compact parser, CRC/lease/readiness/journal logic and actual NOR backends are compiled from `--source`, with `-Wall -Wextra -Werror` and UBSan. Only native MMIO/NOR/EEPROM/time/matrix/output and unrelated feature seams are substituted. No firmware algorithms are translated into Python.

The one-slot selfcheck is bounded characterization, not the full Rust/100-profile suite: exact framing/replies; locked QK_BOOT filtering; locked macro refusal; actual 3000ms unlock; native last-key persistence; macro NOR persistence; FULL4560 stage/ready/store; fresh-process cold query/enum/full byte-exact READ/select; first/last live key+visual; zero recovery/read/select writes.

Unqualified seams inherited from the reviewed builder remain explicit: QMK settings descriptor/query/get/set/reset abort; the Vial discovery definition is a placeholder (the UID itself comes from production keymap config); legacy label/settings and custom lighting callbacks are not in this closure; Combo/KO/AltRepeat reloads are runtime-engine boundaries. Non-split app callback is exercised, not physical split transport/dual-core scheduling. Native storage evidence is not real USB/HID, complete GUI import/export, runtime engine or hardware power-cut qualification.
