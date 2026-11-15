# Isolated display development

The shipping strip-service mitigation remains the default. The isolated owner
is a development path until both physical halves pass input, power, storage and
display qualification. Native injected delays are not measured scan latency.

## Pinned platform boundaries

ChibiOS `8bd61b804303f1614d574546c2dd735eeabb09f5` already supports RP2040
SMP. Its `demos/RP/RT-RP2040-PICO/c1_main.c` shows the second instance startup
used here. `hal_lld_init()` launches core1 before keyboard initialization;
therefore flash protection must start closed at static initialization, not in
keyboard post-init. Core1 waits for native initialization before display work,
but can service a safe park request while waiting.

`rp_dma.c:dmaChannelAllocI()` chooses IRQ0 or IRQ1 from the allocating core's
CPUID, and `dmaChannelEnableInterruptX()` routes the channel to that core.
Allocation and RTOS transitions use the shared kernel spinlock. Rendering,
transfer waiting, abort waiting and SRAM parking must never hold that lock.
Matrix scanning, debounce, split transactions, USB and persistence stay on
core0. The display owner never calls `keyboard_task()`.

The existing synchronous `spiSend()` uses DMA and an indefinite thread wait.
The pinned `spi_lld_abort()` does nothing. `spi_lld_stop()` only tears down a
READY driver, while `dmaChannelAbortX()` waits indefinitely on CHAN_ABORT.
Those functions do not establish a bounded fault path. The SM62 boundary must
start DMA, wait with a deadline outside the kernel lock, mask only its channel
interrupts, abort only its own channels, and observe BUSY clear before returning
the draw buffer or acknowledging a flash park. SPI0 can be stopped without
resetting UART, USB, the DMA block, or core0. A stopped screen is acceptable.

RP2040-E13 means CHAN_ABORT alone is insufficient evidence of quiescence and
can produce a late completion IRQ. Check channel BUSY, then clear the channels'
pending IRQs before retiring the transaction. See the [Raspberry Pi DMA API](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#group_hardware_dma)
and [RP2040 datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf).
If quiescence is not established, retain every buffer and refuse flash writes;
never turn a software timeout into a false DMA-completion claim.

The pinned LVGL 8.2 handler is synchronous. Its return is the frame boundary;
there is no `qp_lvgl_refr_pending()` implementation to call. Park service also
belongs after every completed strip, before `lv_disp_flush_ready()`, so a normal
save does not wait for the entire frame. A parked mid-frame owner retains both
the snapshot and the draw-buffer lease. Healthy park latency must be measured
below the existing 50 ms deadline with margin; retries are not qualification.

## Baseline memory

At `088af4594448e3cc11e67626fa0553ba2cc066e2`, an SM62 v1 build with Arm GNU
15.3.Rel2 places the main RAM heap at `0x20019c98..0x20040000`: 156,520 bytes.
Both cores' bootstrap stacks already occupy separate scratch RAM: 1,024 bytes
main and 2,048 bytes process stack per core. Do not mistake the ELF's reserved
heap section for allocated C objects when reading `size` output.

The original dormant owner additionally needed two 6,144-byte snapshot slots, a 6,144-byte
working area (plus RTOS header/alignment), and a 3,168-byte trace record. The
exact Orbit RGB565 scene needs 107,520 heap bytes; the existing LCD strip needs
13,440. This is a tight budget. Prepare keymaps incrementally inside an available
snapshot slot rather than adding another all-layer cache. Final image maps,
renderer allocation peaks and stack evidence remain required before enabling
this path.

## Selected development path

Build explicitly with:

```sh
make ergohaven/sm62:v1 SM62_DUALCORE_RENDER=yes SM62_SCAN_TRACE=yes
```

The ordinary build keeps the existing core0 renderer. Both C and assembly must
see `CRT0_EXTRA_CORES_NUMBER=1` in the development build; the RP platform's
default is zero. The owner uses the pinned ChibiOS second-instance startup and
a separate 6,144-byte render stack, not the bootstrap stack. Core0 retains the
native input pipeline and PWM HAL. Core1 owns LVGL, QP, SPI0 and two DMA channels.
The existing strip input callback does nothing on this path.

Each half snapshots all 16 layers for its own 35 matrix positions. Renderers
only query their local positions, including older layers used for focus and
transparent-key behavior. There are two 3,200-byte slots (3,176 bytes of value
state each). Preparation fetches at most eight positions per input turn; a
changed map takes 70 preparation turns per slot. A full exchange returns before
fetching. Vial single-key/buffer/reset edits and host-layout changes invalidate
the caches. Published snapshots and pending input/style latches survive owner
stalls; display coalescing is separate from USB input delivery.

The LCD draw buffer is 2,400 bytes (five rows, divisor 56), retaining the exact
107,520-byte Orbit scene. The first integrated image with full-matrix slots and
28-row strips failed the actual ARM allocation workload: Orbit's 107,528-byte
malloc request exceeded the remaining heap by 11,736 bytes. The selected layout
recovers 5,888 static bytes and 11,040 draw-buffer bytes without dropping a screen
mode, layer, or reducing the render stack.

The development ELF has 137,920 heap bytes. Actual ARM LVGL/screens/newlib under
emulation use 24,988 bytes after display initialization and peak at 134,848 bytes
while retaining animated Orbit and drawing every view with a long CJK media
title. Unclaimed heap is 3,072 bytes; observed emulated stack depth is 1,556 bytes.
This is a limited workload, not an exhaustive allocation/fragmentation or IRQ
stack measurement. Physical stack high-water marks and longer feature cycling
remain qualification requirements.

SPI completion remains synchronous within the isolated owner. The board-specific
transport copies chunks into two permanent 1,024-byte SRAM arenas; DMA never
references a caller or LVGL buffer. It polls outside kernel locks, with a 20 ms
per-chunk deadline and a 2 ms abort deadline. A completed transfer observed after
a scheduling delay succeeds; only an incomplete transfer times out. On failure
the screen stops, channels/arenas remain reserved, and no new DMA starts. Park
is refused while BUSY remains set. A later actual completion permits SPI0-only
retirement and safe flash parking. A permanently busy channel leaves writes
closed; it does not block the input core. No DMA interrupt, shared DMA reset,
core reset, or input-peripheral reset participates in teardown.

Flash protection is active at static initialization. The owner polls before
display readiness, between handlers and after completed strips with SPI stopped.
The existing 50 ms token/cancellation/retirement protocol remains. The linked
park function is at `0x20000000`, has no calls or literal dependencies, masks
local IRQs and spins only on the SRAM gate until release. A healthy save's actual
park response and configuration blackout still need physical measurement.

## Executable evidence

`tests/run_render_isolation.py` compiles the production diagnostic logic with
only the microsecond clock, raw HID transport and render-status getters
substituted. It checks enabled/disabled instrumentation, a zero first timestamp,
200 ms gaps, wrap and a bypassed scan hook. Raw trace words 20–23 contain local
scan count, maximum gap in microseconds, last timestamp and instrumentation
enabled, sampled when the frame trace finishes. A concurrent scan/reset marks trace
flag bit 32 and zeros words 20–23 rather than presenting a torn sample. A zero count or disabled flag cannot qualify timing. The optional
`SM62_SCAN_TRACE` hook runs immediately before local GPIO sampling on each half;
there is no per-scan logging or allocation.

The diagnostic protocol also provides an independent trial window. Send a
32-byte raw HID request with bytes 0..7 equal to
`D7 62 18 03 00 SS 00 00` to reset its scan counters (`SS` is any nonzero
sequence), then command `04` to read them after the trial. Bytes 8..31 must be
zero. Both commands require the USB master and page zero. Replies echo the
command/sequence; reset status is 1 and read status is 2. The read payload is
six little-endian u32 words: count, maximum gap, last scan timestamp,
instrumentation-enabled flag, current timestamp, reserved zero. The first scan
after reset starts the interval; boot and preceding trials are excluded.
Commands 1/2 and frame completion never reset or stop this window. Reads do not
reset it. A disabled flag or fewer than two scans cannot establish a gap bound.
A suspend or flash blackout within a window remains visible; explicitly reset
before a new trial rather than silently filtering those gaps.

Commands `05` (queue reset) and `06` (read) relay the same scan window to the
peer through the existing diagnostic RPC. The serial callback only queues a
reset; the peer's next core0 scan consumes it. A queued reset replies status 1;
its first payload u32 is the expected reset token. A second pending reset
replies 6. Read status 3 means pending or a concurrent
scan changed the version; retry command 6. Status 2 carries the same statistics
plus the peer reset acknowledgement in word 5. Require that acknowledgement
to match the token returned by command 5. A read before reset or after peer
reboot is status 8; it cannot qualify a trial. Reset tokens do not wrap. Status 7 is a failed/invalid RPC
and `FE` means peer instrumentation is disabled. Reply byte 6 identifies the
sampled peer side (`20` plus left=1/right=2), not the USB master's side.
The master rejects a reply identifying a master role or the same physical side.

Reset both windows and wait for the peer acknowledgement before stimulus;
read each window after stimulus. Require nonzero counts and at least two peer
scans. Record the peer reset acknowledgement so stale samples are detectable.
The relay makes slave-role statistics observable without swapping its role;
repeat the trial in both USB orientations. Run the explicit diagnostic RPCs at
trial boundaries, since requesting a split transaction can perturb timing.
No automatic per-frame or per-scan diagnostic RPC is added.

`tests/run_scan_timing.py` retains the actual
QMK scan/debounce/action chain and LVGL rendering. Its injected 200 ms transfer
and CPU-delay controls demonstrate why strip service alone cannot bound input.
The matrix, report sink and elapsed costs are synthetic; UART delivery, USB
latency and physical contact edges require separately authorized hardware tests.

`tests/run_render_exchange.py` exercises the actual bounded producer, exchange,
and Vial setters, with full-slot, immutable-lease, generation-wrap, edit-during-
preparation and both-half controls. Missing invalidation mutants must fail.

`tests/run_render_owner.py` runs the production owner and flush body with the
pinned LVGL sources. It tests both halves, 200 ms CPU/transport stalls, failed
initialization/transfer, wrong-core calls, suspend/resume epochs and stale PWM
replies. Early frame completion is a failing control. Panel, scheduler, clock
and input stimulus are native substitutes; iteration counts are not scan timing.

`tests/run_render_lifecycle.py` executes production transport with substituted
MMIO/DMA helpers and production flash guard/park logic with native context and
threads. It covers healthy completion, delayed polling, errors, timeout, late
BUSY, permanent BUSY, startup cancellation and consecutive/nested writes.
Removing the BUSY gate or restoring startup bypass must fail. The MMIO model
checks intended masks and ordering; it does not emulate electrical E13 timing
or the silicon's W1C effects.

`tests/run_render_resources.py` executes the linked ARM LVGL/screens/newlib with
Unicorn 2.1.4 and pyelftools 0.33. It checks actual allocations, draws each view,
requires animated Orbit, at least 2 KiB unclaimed heap and stack within 6 KiB,
and inspects the SRAM park closure. Panel, clock, view inputs, hardware divider,
allocator locks and strip/flash service are substituted and listed in JSON.
Both `--side left` and `--side right` workloads assert that real screen keycode
queries stay within their local rows; keycode values themselves are fixtures.
This checks resources and build wiring, not RP2040 boot, scheduling, DMA or
electrical behavior. The isolated CI artifact is separate from release UF2s.

See [the qualification ledger](RENDER-INPUT-QUALIFICATION.md) for the remaining
hardware gate. Do not enable the default from native or emulated results alone.
