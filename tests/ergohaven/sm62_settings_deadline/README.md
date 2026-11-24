# SM62 local mode absolute GET deadline

Focused production-backed native regression, single-core settings policy only.
No MCU build or physical-display claim.

Run from the repository root with Python 3, a native C compiler and
`AGENT_BUILD_CACHE` set to an existing external build-cache directory. In a
managed agent environment, use the owning project's storage wrapper to provide
that directory and enforce its budget.

```sh
python3 tests/ergohaven/sm62_settings_deadline/run.py --phase ci
```

The runner creates a fresh absolute directory under `AGENT_BUILD_CACHE`, compiles
all of real `sm62_settings.c`, `sm62_settings.h`, and `sm62_side.h` after removing
hardware includes, with `cc -std=c11 -Wall -Wextra -Werror -O0 -g`.
Only clock, physical pins/master role, EEPROM/flash and split transport are fake.
The QSID carrier contains only the `qsid` field consumed by this translation unit.
EEPROM offsets are native fixture addresses. Production settings functions,
persistence, split scheduler and physical-side selection are retained verbatim.

40 isolated process cases: repeated QSID230 + QSID235 GETs every 100ms through
4500ms after SET; no local-mode GET; single GET at 100ms (249/250ms gap);
late GET at 3900ms; remote-side SET. Each runs both physical halves, both master
roles and ordinary/wrapping uint32 clock epochs. Check returned mode values,
ordinary ship normalization, pending release, deferred EEPROM and role-correct
split sending. Calls to the public pending accessor represent the renderer's
normal polling; this does not claim scheduler-only release without that caller.

For baseline comparisons, `--settings-source /absolute/path/to/settings.c`
selects an archived production input without replacing the repository source.
