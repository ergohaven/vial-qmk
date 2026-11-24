# Space Mission 62 — production PCB, SINGLE/core0

QMK/Vial target for the split SM62 production PCB with 4 MiB flash. It is not a firmware target for the old 2 MiB Pico prototype.

The normal `ergohaven/sm62:v1` target uses the synchronous SINGLE/core0 renderer (`SM62_OLD_RENDERER`), version-8/62-control application geometry, split publication and display integration. It implements resident100: 100 application slots and one rotating COW spare. Native Default is separate from these slots. Tap Dance is disabled; 109 macros use a 6066-byte COW overlay without automatically rewriting legacy EEPROM. DUAL/core1 prototypes remain historical and are not selected by this target. Native tests and MCU/UF2 verification do not certify physical hardware acceptance.

This checkout uses pinned Git submodules; initialize them with
`git submodule update --init --recursive`.
The current protocols are documented in `compact_usb.md` and `resident100.md`.

## Hardware and build

- RP2040 on both halves; 5×7 matrix per half, `COL2ROW`; 62 physical keys.
- Rows: GP28, GP19, GP18, GP17, GP16. Columns: GP21, GP22, GP23, GP24, GP25, GP26, GP27. Physical/matrix mapping is authoritative in `keyboard.json`; all 62 logical key coordinates and 16 layers are unchanged.
- Split UART full duplex: TX GP0, RX GP1. Handedness GP29, high=left, low=right.
- ST7789 240×280: SCK GP2, MOSI GP3, RST GP4, DC GP5, CS GP6, backlight GP7; rotation 0.
- No RGB or encoders. `DEBOUNCE=10`, `asym_eager_defer_pk`.
- One common UF2 targets both halves. QMK reads `SPLIT_HAND_PIN` (GP29): high=LEFT, low=RIGHT, independently of the USB master. Build once with `make -j4 ergohaven/sm62:v1 SKIP_GIT=yes`; do not define `SM62_FIRMWARE_SIDE`. This builds but does not flash.
- Physical flash is explicitly 4194304 bytes. Resident100 uses `[0x200000,0x2ca000)`, macro COW banks `[0x2ca000,0x2ce000)`, historical cache `[0x3f2000,0x3fa000)`, extension `[0x3fa000,0x3fc000)`, and EEPROM backing `[0x3fc000,0x400000)`. Code keeps the conservative `0x1f2000` ceiling. The older 2 MiB prototype map is not an update-compatible settings source; this does not instruct users to reset an existing production SM62.

`layout_reserve.h` checks the flash geometry at compile time. `layout_reserve.ld`
enforces the same `0x1f2000` code ceiling as the C configuration and exposes
`__sm62_layout_reserve_start=0x10200000` and
`__sm62_layout_reserve_end=0x102ca000` without allocating a flash section.
The focused backend/geometry regression is
`python3 tests/ergohaven/run_sm62_layout_reserve.py /absolute/build-cache/reserve`.
Use an approved external build cache; the command does not flash a device.

## Accepted display and persistent settings

Each half retains its independent mode (Clock, Player, Ship, Keymap), idle-ship enable/seconds, layout-on-keypress enable/hold, Cyrillic key legends, date, RU/EN visibility, stars and persistent screens-off. Defaults are LEFT Clock and RIGHT Ship; factory keymap hold is **350 ms**. Valid saved current-version values remain unchanged, including custom hold and idle values. The packed SM62 EEPROM block remains **18 bytes, version 5**, after the existing shared LCD block. QSID **230–249** and shared brightness/timeout **318–319** retain their wire meanings; sync uses deferred split RPC.

- Persistent off gates normal wake and panel power, repeatedly suppresses the LCD backlight without deleting saved brightness, and keeps HID/settings/mode-transition leases serviced. Re-enable restores the current view without restarting permanent Ship.
- Boot rendering waits for split/role stabilization; painter initialization retries reuse the device handle. All views live beneath a persistent LVGL root. Frame completion releases pending mode/animation state.
- Keypress focus is the accepted physical-mask-aware 3×3 grid with compact service legends, Cyrillic glyphs, rounded internal junctions and accepted fill/colors. Held keys stay filled. Temporary release fade is `max(1, local_hold_ms/400) * 96 ms` using the accepted 200 ms reference; permanent Keymap always uses 96 ms. Additional uncolored focus hold is 70 ms. Expired off-viewport flags cannot revive when settings increase.
- MAIN momentary NAV is `MO(1)` at matrix **[4][0]**. Saved legacy factory maps migrate only after existing PCB/thumb/legend migrations and only when the **entire** base map matches factory defaults with the one recognized old TG binding. Custom base maps and every upper layer remain intact.
- Unsynchronized clock retains the original Unbounded `SM:62`, centered by its actual glyph bounds. Every valid live time keeps the original hour/colon/minute coordinates. Date and language are independently optional.
- RU/EN requires an actual layout report; time/media activity cannot authenticate it. Layout lease is 15 seconds; explicit layout value 255 clears it. Deferred relay never replays an already delivered layout on mode changes and drops expired pending layouts.
- Player keeps the accepted fonts, geometry and fade masks. Long titles move seamlessly at 18 px/s using repeated text; identical snapshots and forced re-entry preserve phase. Title and artist independently retain prior nonempty metadata for **1500 ms** during transient empty snapshots without extending repeated-empty deadlines. Generic HID inactivity bypasses retention; its accepted lease is 61 seconds.
- Clock, player and focus share the accepted starfield; Ship uses the original Orbit framebuffer/cadence and data assets.

## Stable integration boundaries

SM62-specific date/layout/time parsing, settings storage, painter startup/power, frame diagnostics and task throttling are selected only for this target. Other supported Ergohaven devices use their own board policies. Production SINGLE qualification is owned by `tests/ergohaven/run_ci.py`; see `tests/ergohaven/README.md` for the complete wave and cooperative-profile exclusions.

Host C/LVGL tests and UF2/ELF verification are not physical hardware validation. No build command here flashes a device.
