# SM62 local mode absolute GET deadline

Focused production-backed native regression, single-core settings policy only.
No MCU build or physical-display claim.

```sh
/home/agent/.local/bin/agent-storage run --agent ben --project work/sm62/entropy-sm62-multi-device/publication-0aaa9947/firmware-source -- python3 tests/ergohaven/sm62_settings_deadline/run.py --phase green
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

## Retained checkpoints

Base HEAD: `05ec665ecf879b3eee4362d966980365bbd34663` (production source was staged).
Cache root:
`/home/agent/.cache/agent-build/ben/work_sm62_entropy-sm62-multi-device_publication-0aaa9947_firmware-sour-cd1496a620/`

- RED: `settings-deadline-red-jesumihz/`, exit 1, 24 pass / 16 fail.
  Only `repeated` and `late` fail across all eight role/clock combinations.
  Repeated GETs keep pending true at 4000–4500ms and block master split sync;
  a GET at 3900ms also extends the 4000ms bound.
  Production SHA256: `8dbae874b4ce8ef86e5d6894a40ba31869a2575ec693d846b33ea5050c2ed642`.
- GREEN: `settings-deadline-green-6_55gmj4/`, exit 0, 40 pass / 0 fail.
  Production SHA256: `3e07490fa05e39339e723f698388183c38853173bd1cd6f5992138c667fa0ee8`.

Each directory retains the exact production input, generated native fixture,
source hashes, compile command/log, test commands/log and machine-readable
results/summary. Both checkpoints use byte-identical test inputs.
Replay RED with `--settings-source <RED-directory>/settings-production.c`.

Historical setup run `settings-deadline-red-x8qn7y51/` also had an incorrect
right-side ship-enabled expectation (leaving factory ship intentionally disables
it); it is not the clean RED checkpoint. This expectation was corrected before
any production edit. An initial agent-storage attempt refused transient existing
cache symlinks; inspection later found none and the unchanged wrapper succeeded.

Minimal repair adds the absolute SET-age bound to the readback gap branch;
GET behavior, saved layout, setter normalization, persistence and split rules
are unchanged. `git diff --check` passed. No staging, commits or broad suites.
