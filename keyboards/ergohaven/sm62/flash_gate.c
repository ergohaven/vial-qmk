// SPDX-License-Identifier: GPL-2.0-or-later
#include "flash_gate.h"
#define LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define STORE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)

bool sm62_flash_request(sm62_flash_gate_t *g, uint32_t *token) {
    uint32_t r = LOAD(&g->requested);
    if (!token || r == UINT32_MAX || r != LOAD(&g->retired) || r != LOAD(&g->released)) return false;
    *token = r + 1U;
    STORE(&g->requested, *token);
    return true;
}

bool sm62_flash_granted(const sm62_flash_gate_t *g, uint32_t token) {
    return token && LOAD(&g->requested) == token && LOAD(&g->acknowledged) == token && LOAD(&g->released) != token;
}

void sm62_flash_release(sm62_flash_gate_t *g, uint32_t token) {
    if (token && LOAD(&g->requested) == token) STORE(&g->released, token);
}

__attribute__((noinline, section(".time_critical.sm62_flash_worker_park")))
void sm62_flash_worker_park(sm62_flash_gate_t *g, bool quiescent) {
    if (!quiescent) return;
    uint32_t r = LOAD(&g->requested);
    if (!r || r == LOAD(&g->retired)) return;
#if defined(__arm__)
    uint32_t irq;
    __asm__ volatile("mrs %0, primask\ncpsid i" : "=r"(irq) :: "memory");
#endif
    if (LOAD(&g->released) != r) {
        STORE(&g->acknowledged, r);
        while (LOAD(&g->released) != r) {
            /* SRAM-only spin, not WFE: no dependency on a flash IRQ handler.
             * No global system lock is owned here. */
        }
    }
    STORE(&g->retired, r);
#if defined(__arm__)
    __asm__ volatile("msr primask, %0" :: "r"(irq) : "memory");
#endif
}
