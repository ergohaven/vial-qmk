// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Four single-writer u32 words in SRAM. core0 owns requested/released,
 * core1 owns acknowledged/retired. No RMW or ChibiOS lock. Request identities
 * never reuse: exhaustion fails closed. After timeout the request must still
 * be retired by core1 before another request is accepted. */
typedef struct {
    uint32_t requested, released, acknowledged, retired;
} sm62_flash_gate_t;
bool sm62_flash_request(sm62_flash_gate_t *g, uint32_t *token);
bool sm62_flash_granted(const sm62_flash_gate_t *g, uint32_t token);
void sm62_flash_release(sm62_flash_gate_t *g, uint32_t token);
/* Worker only; entry with IRQ enabled, no RTOS/system/SPI mutex or DMA lease.
 * quiescent MUST be established from real driver/owner state by integration.
 * ACK is published only after disabling local interrupts INSIDE RAM code.
 * No IRQ, RTOS, allocator, trace, callback or flash fetch while parked.
 * Never call under chSysLock; no timeout on worker permits flash to finish.
 * Only core0 has the bounded timeout and MUST NOT flash without granted(). */
void sm62_flash_worker_park(sm62_flash_gate_t *g, bool quiescent);
