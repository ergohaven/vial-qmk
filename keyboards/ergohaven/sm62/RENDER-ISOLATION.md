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

The dormant owner additionally needs two 6,144-byte snapshot slots, a 6,144-byte
working area (plus RTOS header/alignment), and a 3,168-byte trace record. The
exact Orbit RGB565 scene needs 107,520 heap bytes; the existing LCD strip needs
13,440. This is a tight budget. Prepare keymaps incrementally inside an available
snapshot slot rather than adding another all-layer cache. Final image maps,
renderer allocation peaks and stack evidence remain required before enabling
this path.

## Executable evidence

`tests/run_render_isolation.py` compiles the production diagnostic logic with
only the microsecond clock, raw HID transport and render-status getters
substituted. It checks enabled/disabled instrumentation, a zero first timestamp,
200 ms gaps, wrap and a bypassed scan hook. Raw trace words 20–23 contain local
scan count, maximum gap in microseconds, last timestamp and instrumentation
enabled. A zero count or disabled flag cannot qualify timing. The optional
`SM62_SCAN_TRACE` hook runs immediately before local GPIO sampling on each half;
there is no per-scan logging or allocation.

`tests/run_scan_timing.py` retains the actual
QMK scan/debounce/action chain and LVGL rendering. Its injected 200 ms transfer
and CPU-delay controls demonstrate why strip service alone cannot bound input.
The matrix, report sink and elapsed costs are synthetic; UART delivery, USB
latency and physical contact edges require separately authorized hardware tests.
