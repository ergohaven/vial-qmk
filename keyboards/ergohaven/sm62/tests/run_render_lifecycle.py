#!/usr/bin/env python3
"""Production SPI fault/park logic; native MMIO, clocks and scheduler boundaries.

No electrical DMA, UART, USB, flash silicon or scan-latency qualification.
"""
import argparse
import resource
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def compile_run(name, source, modes, extra=(), rejected=False):
    path = args.output / (name + '.c')
    path.write_text(source)
    exe = args.output / name
    subprocess.run(['cc', '-std=gnu11', '-O1', '-g', '-pthread', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all', '-I'+str(HERE.parent),
                    str(path), *map(str, extra), '-o', str(exe)], check=True)
    for mode in modes:
        run = subprocess.run([str(exe), str(mode)], capture_output=True, text=True, timeout=10)
        (args.output / f'{name}-{mode}.log').write_text(run.stdout + run.stderr)
        print(run.stdout + run.stderr, end='', flush=True)
        assert run.returncode == (-6 if rejected else 0), (name, mode, run.returncode)
        if rejected: print('PASS negative control:', name, 'rejected', flush=True)

# Pinned register names/constants, but scheduling and peripheral progress are
# simulated. The production C state machine, deadlines, copies and teardown run.
spi_source = '\n'.join(line for line in (HERE.parent/'display_spi.c').read_text().splitlines()
                       if not line.startswith('#include'))
spi_fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DMA_CTRL_TRIG_BUSY 1U
#define DMA_CTRL_TRIG_AHB_ERROR 2U
#define DMA_CTRL_TRIG_TREQ_SPI0_RX 4U
#define DMA_CTRL_TRIG_TREQ_SPI0_TX 8U
#define DMA_CTRL_TRIG_DATA_SIZE_BYTE 0U
#define DMA_CTRL_TRIG_INCR_READ 16U
#define DMA_CTRL_TRIG_INCR_WRITE 32U
#define SPI_SSPSR_BSY 1U
#define SPI_SSPDMACR_RXDMAE 1U
#define SPI_SSPDMACR_TXDMAE 2U
#define SPI_SSPCR1_SSE 1U
#define RP_SPI_SPI0_RX_DMA_CHANNEL 0U
#define RP_SPI_SPI0_TX_DMA_CHANNEL 1U
#define RP_IRQ_SPI0_PRIORITY 2U
#define RESETS_ALLREG_SPI0 1U
struct channel { uint32_t CTRL_TRIG, TRANS_COUNT, READ_ADDR, WRITE_ADDR; } channels[2];
typedef struct { struct channel *channel; uint32_t chnmask; } rp_dma_channel_t;
static rp_dma_channel_t descriptors[2]={{&channels[0],1},{&channels[1],2}};
static struct { uint32_t INTR; struct { uint32_t CHAN_ABORT; } SET; } dma;
#define DMA (&dma)
static struct spi_regs { uint32_t SSPCR0, SSPCPSR, SSPCR1, SSPDMACR, SSPSR, SSPDR; } spi;
static bool peripheral_unreset;
static struct spi_regs *spi_registers(void) { assert(peripheral_unreset); return &spi; }
#define SPI0 spi_registers()
static struct { uint32_t timerawl; } clock_regs;
#define timer_hw (&clock_regs)
typedef struct { uint32_t SSPCR0, SSPCPSR; } SPIConfig;
static bool owner=true;
static unsigned mode, locks, allocs, resets, polls;
static uint32_t tx_arena, rx_arena, abort_at;
bool sm62_render_is_owner(void) { return owner; }
static void osalSysLock(void) { assert(!locks);locks++; }
static void osalSysUnlock(void) { assert(locks==1);locks--; }
static const rp_dma_channel_t *dmaChannelAllocI(unsigned id,unsigned priority,void *cb,void *arg) {
    (void)priority;(void)arg;assert(locks==1 && !cb);allocs++;
    return mode==5 && id==1 ? NULL : &descriptors[id];
}
static void dmaChannelDisableInterruptX(const rp_dma_channel_t *c) { assert(c); }
static bool dmaChannelIsBusyX(const rp_dma_channel_t *c) { return c->channel->CTRL_TRIG&DMA_CTRL_TRIG_BUSY; }
static void dmaChannelSetSourceX(const rp_dma_channel_t *c,uint32_t p) { assert(!dmaChannelIsBusyX(c));c->channel->READ_ADDR=p; }
static void dmaChannelSetDestinationX(const rp_dma_channel_t *c,uint32_t p) { assert(!dmaChannelIsBusyX(c));c->channel->WRITE_ADDR=p; }
static void dmaChannelSetCounterX(const rp_dma_channel_t *c,uint32_t n) { c->channel->TRANS_COUNT=n; }
static void dmaChannelSetModeX(const rp_dma_channel_t *c,uint32_t n) { (void)n;c->channel->CTRL_TRIG=0; }
static void dmaChannelEnableX(const rp_dma_channel_t *c) {
    assert(!locks);
    if(c==&descriptors[0]) assert(c->channel->WRITE_ADDR==rx_arena);
    else assert(c->channel->READ_ADDR==tx_arena);
    c->channel->CTRL_TRIG=DMA_CTRL_TRIG_BUSY;
}
static void hal_lld_peripheral_unreset(unsigned id) { assert(id==RESETS_ALLREG_SPI0); peripheral_unreset=true; }
static void hal_lld_peripheral_reset(unsigned id) {
    assert(id==RESETS_ALLREG_SPI0 && !locks);
    assert(!dmaChannelIsBusyX(&descriptors[0]) && !dmaChannelIsBusyX(&descriptors[1]));
    spi.SSPSR=0;resets++;peripheral_unreset=false;
}
static void chThdSleepMicroseconds(unsigned us) {
    assert(!locks);clock_regs.timerawl+=(mode==7 ? 25000U : us);polls++;
    if(mode==0 || mode==7 || (dma.SET.CHAN_ABORT && mode!=3 && mode!=2)) {
        channels[0].CTRL_TRIG=channels[1].CTRL_TRIG=0;
        channels[0].TRANS_COUNT=channels[1].TRANS_COUNT=0;
    }
    if(mode==4) channels[0].CTRL_TRIG|=DMA_CTRL_TRIG_AHB_ERROR;
    if(mode==6) spi.SSPSR=SPI_SSPSR_BSY;
    if(dma.SET.CHAN_ABORT && !abort_at) abort_at=clock_regs.timerawl;
}
'''
spi_cases = r'''
int main(int argc,char **argv) {
    assert(argc==2);mode=atoi(argv[1]);clock_regs.timerawl=UINT32_MAX-1000U;
    tx_arena=(uint32_t)(uintptr_t)tx_storage;rx_arena=(uint32_t)(uintptr_t)rx_storage;
    SPIConfig config={0,4};
    sm62_display_spi_stop();assert(sm62_spi_quiescent()); /* reset peripheral is untouched */
    owner=false;assert(!sm62_display_spi_start(&config));assert(allocs==0);owner=true;
    if(mode==5) {
        assert(!sm62_display_spi_start(&config));
        assert(sm62_display_spi_failed() && sm62_spi_quiescent());
    } else {
        assert(sm62_display_spi_start(&config));
        uint8_t source[1500],copy[1500];memset(source,0xA5,sizeof(source));
        uint32_t start=clock_regs.timerawl;
        bool ok=sm62_display_spi_transfer(source,copy,sizeof(source));
        assert(ok==(mode==0 || mode==7));
        if(mode!=7) assert((uint32_t)(clock_regs.timerawl-start)<=TRANSFER_TIMEOUT_US+ABORT_TIMEOUT_US+100U);
        sm62_display_spi_stop();
        if(mode==2 || mode==3) {
            assert(!sm62_spi_quiescent());
            memset(source,0xEE,sizeof(source));
            assert(tx_storage[0]==0xA5); /* DMA never borrowed caller memory */
            assert(!sm62_display_spi_start(&config));
            if(mode==2) {
                /* E13: abort already returned, BUSY and late IRQ retire later. */
                channels[0].CTRL_TRIG=channels[1].CTRL_TRIG=0;
                dma.INTR=3;assert(sm62_spi_quiescent());assert(resets==1 && dma.INTR==3);
            }
        } else assert(sm62_spi_quiescent());
        if(mode && mode!=7) assert(sm62_display_spi_failed());
        else assert(allocs==2 && !resets && tx_storage[0]==0xA5);
    }
    assert(!locks);
    printf("PASS SPI native-MMIO mode=%u polls=%u resets=%u\n",mode,polls,resets);
}
'''
compile_run('spi', spi_fixture+spi_source+spi_cases, range(8))

# The isolated transport owns the registers itself, so HAL select/unselect
# must not assert READY on an intentionally stopped SPIDriver. Exercise both
# QMK GPIO selection configurations using the actual wrapper bodies.
master = (HERE.parents[3]/'platforms/chibios/drivers/spi_master.c').read_text()
select_body = master[master.index('static inline void spi_select('):master.index('__attribute__((weak)) void spi_init(')]
select_declarations = master[master.index('#if SPI_SELECT_MODE ==', master.index('static bool spiStarted')):master.index('static SPIConfig')]
select_assignment = master[master.index('    spiStarted = true;'):master.index('    if (!sm62_display_spi_start(&spiConfig))')]
# The last #if opens the transport start, beyond this chip-select boundary.
select_assignment = select_assignment[:select_assignment.rindex('#if defined(SM62_DUALCORE_RENDER)')]
select_stop = master[master.index('void spi_stop(void)'):]
select_fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define SPI_SELECT_MODE_NONE 0
#define SPI_SELECT_MODE_PAD 1
#define SPI_USE_MUTUAL_EXCLUSION 0
#define TRUE 1
#define PAL_PORT(p) (p)
#define PAL_PAD(p) (p)
#define NO_PIN 255
#define SPI_DRIVER driver_instance
typedef unsigned pin_t;
static unsigned hal_calls, levels;
static bool spiStarted;
static unsigned stops, outputs;
static struct { unsigned ssport, sspad; } spiConfig __attribute__((unused));
typedef struct { unsigned slave_pin; bool cs_active_low; } spi_start_config_t;
static void gpio_set_pin_output(unsigned pin) { assert(pin==7); outputs++; }
static void gpio_write_pin(unsigned pin,unsigned level) { assert(pin==7);levels=(levels<<1)|level; }
#ifndef SM62_DUALCORE_RENDER
static unsigned driver_instance;
static void spiSelect(unsigned *driver) { (void)driver;hal_calls++; }
static void spiUnselect(unsigned *driver) { (void)driver;hal_calls++; }
static void spiStop(unsigned *driver) { (void)driver;stops++; }
#else
static bool sm62_render_is_owner(void) { return true; }
static void sm62_display_spi_stop(void) { stops++; }
#endif
'''
select_cases = r'''
int main(void) {
    spi_start_config_t config={7,true};configure_cs(&config);spi_select();spi_stop();
    assert(!spiStarted && stops==1 && outputs==1);
#ifdef SM62_DUALCORE_RENDER
    assert(!hal_calls && levels==1);
#else
    assert(hal_calls==2 && levels==(SPI_SELECT_MODE==SPI_SELECT_MODE_NONE ? 1U : 0U));
#endif
    puts("PASS production chip-select wrappers");
}
'''
for isolated, pad in [(True,True),(True,False),(False,False),(False,True)]:
    defines = ('#define SM62_DUALCORE_RENDER\n' if isolated else '') + f'#define SPI_SELECT_MODE {int(pad)}\n'
    fixture = select_fixture if isolated or not pad else select_fixture.replace('static void gpio_write_pin', '__attribute__((unused)) static void gpio_write_pin')
    configure='static void configure_cs(spi_start_config_t *start_config) {\n'+select_assignment+'}\n'
    compile_run(f'select-{isolated}-{pad}', defines+fixture+select_declarations+select_body+configure+select_stop+select_cases, [0])

flash_source = '\n'.join(line for line in (HERE.parent/'flash_guard.c').read_text().splitlines()
                         if not line.startswith('#include'))
flash_fixture = r'''
#define SM62_FLASH_NATIVE_TEST
#define SM62_DUALCORE_RENDER
#include "flash_guard.h"
#include "flash_gate.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static _Atomic bool stop_worker;
static bool valid=true;
uint32_t sm62_flash_test_time_us(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint32_t)((uint64_t)t.tv_sec*1000000U+t.tv_nsec/1000U);
}
bool sm62_flash_test_context_valid(void) { return valid; }
'''
flash_cases = r'''
static void *worker(void *unused) {
    (void)unused;
    while(!atomic_load(&stop_worker)) { sm62_flash_core1_poll(true);sched_yield(); }
    return NULL;
}
int main(int argc,char **argv) {
    assert(argc==2);unsigned mode=atoi(argv[1]);
    if(mode==0) {
        uint32_t start=sm62_flash_test_time_us();
        assert(!sm62_flash_begin()); /* launch-before-ready must fail closed */
        assert(sm62_flash_test_time_us()-start>=SM62_FLASH_PARK_TIMEOUT_US);
        assert(!sm62_flash_ok() && !sm62_flash_end());
        /* A cancelled request cannot receive a stale grant. */
        sm62_flash_core1_poll(true);
        assert(!sm62_flash_granted(&gate,gate.requested));
    }
    pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));
    uint32_t max_park=0;
    for(unsigned i=0;i<30;i++) {
        uint32_t start=sm62_flash_test_time_us();assert(sm62_flash_begin());
        uint32_t elapsed=sm62_flash_test_time_us()-start;
        if(elapsed>max_park) max_park=elapsed;
        assert(sm62_flash_granted(&gate,owned_token));
        assert(sm62_flash_begin());assert(depth==2);
        if(mode==1) { sm62_flash_failed();assert(!sm62_flash_ok());assert(!sm62_flash_end());assert(!sm62_flash_end()); }
        else { assert(sm62_flash_end());assert(sm62_flash_end()); }
    }
    valid=false;assert(!sm62_flash_begin());valid=true;
    atomic_store(&stop_worker,true);assert(!pthread_join(thread,NULL));
    assert(!depth && max_park<SM62_FLASH_PARK_TIMEOUT_US);
    printf("PASS flash production gate mode=%u max_native_park_us=%u\n",mode,max_park);
}
'''
gate_source = (HERE.parent/'flash_gate.c').read_text().replace(
    '__attribute__((noinline, section(".time_critical.sm62_flash_worker_park")))',
    '__attribute__((noinline))') # Native Mach-O has no embedded RAM section.
compile_run('flash', flash_fixture+flash_source+gate_source+flash_cases, range(2))

# A nominal abort/stop bit is insufficient while DMA BUSY remains asserted.
assert ' || dma_busy()' in spi_source
compile_run('spi-unsafe-retire', spi_fixture+spi_source.replace(' || dma_busy()', '')+spi_cases,
            [2], rejected=True)
assert 'static uint32_t core1_prepared = 1U;' in flash_source
compile_run('flash-startup-bypass', flash_fixture+flash_source.replace(
    'static uint32_t core1_prepared = 1U;', 'static uint32_t core1_prepared;')+gate_source+flash_cases,
    [0], rejected=True)
