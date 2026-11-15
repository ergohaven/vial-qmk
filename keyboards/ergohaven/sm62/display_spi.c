// SPDX-License-Identifier: GPL-2.0-or-later
/* SM62 SPI0 only. Poll DMA completion on the render owner, without a DMA ISR
 * or a kernel lock spanning a wait. Keep channels reserved for this owner's
 * lifetime: the pinned Free/Disable helpers can wait indefinitely on abort.
 */
#include "display_spi.h"
#include "render_owner.h"
#include "hardware/structs/timer.h"
#include <string.h>

#define CHUNK_BYTES 1024U
#define TRANSFER_TIMEOUT_US 20000U
#define ABORT_TIMEOUT_US 2000U
static const rp_dma_channel_t *tx_channel, *rx_channel;
/* No DMA pointer ever refers to a caller stack or an LVGL lease. Even a DMA
 * that fails to retire after abort retains only these permanent SRAM arenas.
 * This is fault containment, not a second LVGL draw buffer or pipelining. */
static uint8_t tx_storage[CHUNK_BYTES], rx_storage[CHUNK_BYTES];
static bool failed, selected, retired, peripheral_ready;

static bool dma_busy(void) {
    return (tx_channel && dmaChannelIsBusyX(tx_channel)) ||
           (rx_channel && dmaChannelIsBusyX(rx_channel));
}

static void clear_pending(void) {
    uint32_t mask = (tx_channel ? tx_channel->chnmask : 0U) |
                    (rx_channel ? rx_channel->chnmask : 0U);
    DMA->INTR = mask; /* W1C only our channels, after BUSY clears (RP2040 E13). */
}

static void fault(void) {
    failed = true;
    if (peripheral_ready) SPI0->SSPDMACR = 0U;
    uint32_t mask = (tx_channel ? tx_channel->chnmask : 0U) |
                    (rx_channel ? rx_channel->chnmask : 0U);
    if (tx_channel) dmaChannelDisableInterruptX(tx_channel);
    if (rx_channel) dmaChannelDisableInterruptX(rx_channel);
    DMA->SET.CHAN_ABORT = mask;
    uint32_t start = timer_hw->timerawl;
    /* CHAN_ABORT clearing alone is NOT completion: E13 allows a late write
     * and IRQ. Retain arenas permanently; later polls may finish retirement. */
    while (dma_busy() && (uint32_t)(timer_hw->timerawl - start) < ABORT_TIMEOUT_US) {
        chThdSleepMicroseconds(50);
    }
    if (peripheral_ready) SPI0->SSPCR1 = 0U;
    if (!dma_busy()) clear_pending();
}

bool sm62_display_spi_failed(void) { return failed; }

bool sm62_spi_quiescent(void) {
    if (!sm62_render_is_owner() || selected || dma_busy()) return false;
    /* Late completion can arrive after the bounded abort wait. Channels and
     * arenas are never freed/reused after a fault; safely retire only now. */
    if (failed && !retired) {
        if (peripheral_ready) hal_lld_peripheral_reset(RESETS_ALLREG_SPI0);
        peripheral_ready = false;
        clear_pending();
        retired = true;
    }
    return failed || !peripheral_ready || (SPI0->SSPSR & SPI_SSPSR_BSY) == 0U;
}

bool sm62_display_spi_start(const SPIConfig *config) {
    if (!sm62_render_is_owner() || failed || selected || !config) return false;
    if (!tx_channel && !rx_channel) {
        osalSysLock();
        rx_channel = dmaChannelAllocI(RP_SPI_SPI0_RX_DMA_CHANNEL, RP_IRQ_SPI0_PRIORITY, NULL, NULL);
        tx_channel = dmaChannelAllocI(RP_SPI_SPI0_TX_DMA_CHANNEL, RP_IRQ_SPI0_PRIORITY, NULL, NULL);
        osalSysUnlock();
        if (!tx_channel || !rx_channel) { fault(); return false; }
        /* No channel IRQ on either core: the pinned ISR's error branch itself
         * enters an unbounded abort. Only this owner polls these channels. */
        dmaChannelDisableInterruptX(tx_channel);
        dmaChannelDisableInterruptX(rx_channel);
        hal_lld_peripheral_unreset(RESETS_ALLREG_SPI0);
        peripheral_ready = true;
    }
    SPI0->SSPCR1 = 0U;
    SPI0->SSPCR0 = config->SSPCR0;
    SPI0->SSPCPSR = config->SSPCPSR;
    SPI0->SSPDMACR = SPI_SSPDMACR_RXDMAE | SPI_SSPDMACR_TXDMAE;
    SPI0->SSPCR1 = SPI_SSPCR1_SSE;
    selected = true;
    return true;
}

bool sm62_display_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length) {
    if (!sm62_render_is_owner() || !selected || failed) return false;
    while (length) {
        size_t count = length > CHUNK_BYTES ? CHUNK_BYTES : length;
        if (tx) memcpy(tx_storage, tx, count);
        else memset(tx_storage, 0xFF, count);
        dmaChannelSetSourceX(rx_channel, (uint32_t)(uintptr_t)&SPI0->SSPDR);
        dmaChannelSetDestinationX(rx_channel, (uint32_t)(uintptr_t)rx_storage);
        dmaChannelSetCounterX(rx_channel, count);
        dmaChannelSetModeX(rx_channel, DMA_CTRL_TRIG_TREQ_SPI0_RX | DMA_CTRL_TRIG_DATA_SIZE_BYTE | DMA_CTRL_TRIG_INCR_WRITE);
        dmaChannelSetSourceX(tx_channel, (uint32_t)(uintptr_t)tx_storage);
        dmaChannelSetDestinationX(tx_channel, (uint32_t)(uintptr_t)&SPI0->SSPDR);
        dmaChannelSetCounterX(tx_channel, count);
        dmaChannelSetModeX(tx_channel, DMA_CTRL_TRIG_TREQ_SPI0_TX | DMA_CTRL_TRIG_DATA_SIZE_BYTE | DMA_CTRL_TRIG_INCR_READ);
        dmaChannelEnableX(rx_channel);
        dmaChannelEnableX(tx_channel);
        uint32_t start = timer_hw->timerawl;
        do {
            uint32_t errors = (rx_channel->channel->CTRL_TRIG | tx_channel->channel->CTRL_TRIG) & DMA_CTRL_TRIG_AHB_ERROR;
            if (errors) {
                fault();
                return false;
            }
            if (!dma_busy() && !(SPI0->SSPSR & SPI_SSPSR_BSY)) {
                if (tx_channel->channel->TRANS_COUNT || rx_channel->channel->TRANS_COUNT) {
                    fault(); return false;
                }
                break;
            }
            /* A delayed owner may observe an already completed DMA after the
             * deadline. Do not permanently kill a successfully sent frame. */
            if ((uint32_t)(timer_hw->timerawl - start) >= TRANSFER_TIMEOUT_US) {
                fault(); return false;
            }
            chThdSleepMicroseconds(50);
        } while (true);
        clear_pending();
        if (rx) { memcpy(rx, rx_storage, count); rx += count; }
        if (tx) tx += count;
        length -= count;
    }
    return true;
}

void sm62_display_spi_stop(void) {
    if (!sm62_render_is_owner() || !peripheral_ready) return;
    SPI0->SSPDMACR = 0U;
    SPI0->SSPCR1 = 0U;
    selected = false;
}
