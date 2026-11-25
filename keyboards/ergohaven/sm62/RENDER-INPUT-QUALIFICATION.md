# SM62 isolated renderer qualification

Status: development only; **not qualified for default enablement**. No keyboard
was flashed or physically tested during source implementation. The original
report is not yet physically reproduced by this work.

## Requirement-to-evidence ledger

| Requirement | Source evidence | Outstanding physical evidence |
| --- | --- | --- |
| R1 rapid repeated keys | Actual QMK pipeline controls; owner stall fixture | AE1/AE3 baseline and candidate, both halves/orientations |
| R2 input independent of rendering | Bounded exchange, owner/flush guards, CPU/SPI stall controls | AE2 maximum scan gaps and host latency; actual capture cost |
| R3 keyboard semantics | Existing native processor/split regression wave | Real UART/reconnect, combos/modifiers/macros under redraw |
| R4 display features | All-layer local snapshots; ARM Orbit/keymap/player/clock drawing | Both screens, settings, all layers, power and long cycling |
| R5 failure containment | Init/transfer/power native owner tests; bounded MMIO fault tests | Actual startup, suspend/wake, peripheral faults and input |
| R6 storage compatibility | Unchanged addresses; native storage campaigns, flash token controls; linked SRAM park | AE5 redraw-time saves, park margin, blackout and cold readback |
| R7 provenance | Pinned source/builds, explicit fixture boundaries, separate development CI | Board revisions, exact image hashes and stimulus/capture records |
| R8 timing/counts | Optional per-half scan counters, bypass negative control | Physical maximum gaps/latency and zero loss/duplicates |

U1–U5 are source integration and regression work. U6 requires separate hardware
authorization and remains incomplete. Firmware matrix results belong to the PR
checks; a local SM62 link is not a full matrix result. Builds/native tests do not
prove any of the physical numbers below.

## Source validation sample

Built on macOS with Arm GNU 15.3.Rel2 and the unchanged pinned submodules:
default SM62, isolated SM62 with scan trace, and K03 Pro rev1/43mm v1. The
isolated sample's ELF SHA256 is
`2e6008d713551a4d7a3cd776176191b988a8f8980c75cec398df27ed3cc813a9`;
UF2 SHA256 is
`85c9f649e64e926cb97f0d762f6fda6212a24b12e06880be17ab184c96d25617`.
Hashes identify a local build sample, not a flashed or release-qualified image.

The Linux QMK container ran `tests/ergohaven/run_ci.py` with pinned LVGL:
52 commands, zero failed, including deliberate failing mutation controls.
The CI policy/workflow suite passed 41 tests. Release-scope validation covered
32 targets, 39 Vial definitions and eight intentional exclusions; this is an
inventory check, not 32 local firmware builds. The full firmware matrix remains
an upstream PR check.

The linked ARM allocation workload retained all screen objects and Orbit's
image while drawing Orbit/keymap/player/clock with long CJK media strings.
It passed with 3,072 bytes unclaimed heap and 1,556 bytes maximum emulated stack
depth. The SRAM park disassembly has no calls, literal loads or external branch
targets. The manual [SM62 isolated development workflow](../../../.github/workflows/sm62-isolated.yml)
repeats these checks with `sm62_isolated` enabled. It does not substitute for
hardware stack, scheduling or DMA validation.

## Hardware acceptance procedure (not executed)

Use the independently reviewed plan dated 2026-11-15; retain its targets:

1. Record board revision, firmware SHA, dependency pins, settings and USB side.
   Capture the reporter's failing sequence or reproduce loss on the original
   baseline. Compare that image, PR #60 and this development candidate under the
   same stimulus. If baseline loss cannot be reproduced, report only the tested
   envelope; do not claim the original report resolved.
2. Apply two 10 ms switch closures separated by 20 ms released time. Sweep phase
   against refresh and split polling for letters, Backspace and Enter on both
   halves, including the first pair after 30 seconds idle and after the actual
   configured screen-off timeout (10 minutes by default).
3. Exercise Orbit, helper transitions on every layer, clock/player/media/layout
   updates and simultaneous activity on both screens. Record maximum local scan
   gap on **each** half (target ≤2 ms) and physical-edge-to-host-report latency
   (target ≤20 ms). Averages, frame rate and text output are insufficient.
4. Run ≥100,000 generated press/release cycles **per USB orientation**, distributed
   over both halves and workloads, with zero loss, duplication or stuck keys.
   Include controlled 200 ms CPU and transfer stalls, reconnect and suspend/wake.
5. Save settings, single/buffer keymaps and macros under redraw, then cold restart
   and read them back. Measure healthy park response below the existing 50 ms
   deadline with margin. Verify canceled/late grants never write, and safe saves
   resume after actual transfer teardown. Report explicit write blackout apart
   from ordinary typing; automatic screen/idle transitions are not write exemptions.
6. Record heap/stack high-water marks and failures through sustained feature
   cycling. The emulated resource sample has only 3,072 bytes unclaimed heap;
   exercise fragmentation, long changing titles, all layers and both physical
   screen roles before accepting the budget.
7. Only after all targets pass, enable the isolated SM62 default and rerun
   representative tests on that exact final image. If a target is infeasible,
   retain the development gate and return for a product decision.

## Independent review corrections

Actual read-only `claude -p` reviews identified a valid completion/deadline bug:
an owner rescheduled after its deadline incorrectly faulted a DMA that had
already completed. A native late-poll control failed before the fix and passes
with completion checked first. The resource check also reproduced the initial
Orbit allocation failure, resolved by compact local-half snapshots and smaller
strips while retaining all layers and the exact scene.

Review claims of missing QP transaction cleanup or missing strip park service
were rejected against the actual call paths. Suggestions to grant flash while
DMA remained BUSY or snapshot only the current layer were rejected: they would
violate buffer/XIP safety or existing older-layer rendering behavior. Native
MMIO controls remain an explicit limit, not silicon E13 qualification.


The final source-packet review used actual `claude -p` (served model
`claude-opus-5-5`). Confirmed follow-ups add a resettable scan trial window,
keep a faulted panel dark across wake epochs, avoid SPI0 accesses before
unreset, and bypass HAL select assertions for the privately owned transport.
Native cases cover these boundaries and both PAD/NONE chip-select wrappers
with isolation enabled and disabled, using actual chip-select assignment,
wrapper and stop bodies (peripheral scheduling remains substituted). The
follow-up review exposed master-only telemetry; commands 5/6 now relay peer reset/read through the existing diagnostic
RPC. Reset is consumed on the peer input core, and pending/torn/failed samples
are rejected. The actual board RPC dispatch and master wrapper run in the
native fixture, including reboot, wrong-side/role, and old-protocol rejection.
Frame samples also reject a concurrent scan/reset. The final bounded review
found no remaining code-proven defect in these fixes. Real split timing is U6.
CI resource artifacts now use workspace-relative paths with explicit presence
checks. The actual ARM screen workload checks local-row queries for both roles.

The fork already propagates SPI errors through its transaction end result;
its pinned DMA helper explicitly selects self-chaining. Those review concerns
do not describe defects in this source. The telemetry fixture's injected gap
is named as such; CPU and panel stalls are exercised separately by the owner
fixture. Legacy-flush and isolated-input-hook tests are labeled separately.

Remaining availability limits: every keymap invalidation requires 70 bounded
producer turns per slot before a fresh snapshot can publish. Continuous changes
can delay all display-state updates; input still proceeds and the owner keeps
its previous immutable view. Display/Orbit initialization contains unpolled
windows that can make a concurrent save fail closed at 50 ms. Measure those
windows and initialization/retry saves in AE5 before default enablement.
Mutation controls currently cover Vial single/buffer invalidation; the other
host, native-label and peer invalidation sites were source inspected, not all
individually mutation-tested. Label reload currently occurs before rendering
readiness or after the reset path's invalidation. Real DMA E13/W1C behavior,
init-time park margin, heap fragmentation and physical scan bounds remain U6.
