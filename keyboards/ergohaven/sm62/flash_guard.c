// SPDX-License-Identifier: GPL-2.0-or-later
#include "flash_guard.h"
#include "flash_gate.h"
#if defined(SM62_FLASH_NATIVE_TEST)
/* Only MMIO/context is substituted. Rendezvous/deadline logic is production. */
extern uint32_t sm62_flash_test_time_us(void);
extern bool sm62_flash_test_context_valid(void);
#define NOW() sm62_flash_test_time_us()
#define CONTEXT() sm62_flash_test_context_valid()
#else
#include "hardware/structs/timer.h"
#include "hardware/structs/sio.h"
static bool context_valid(void) {
    uint32_t irq, isr;
    __asm__ volatile("mrs %0, primask\nmrs %1, ipsr" : "=r"(irq), "=r"(isr) :: "memory");
    return !irq && !isr && sio_hw->cpuid == 0U;
}
#define NOW() (timer_hw->timerawl)
#define CONTEXT() context_valid()
#endif
#define SM62_FLASH_PARK_TIMEOUT_US 50000U
static sm62_flash_gate_t gate;
static uint32_t core1_prepared;
static uint32_t owned_token;
static uint8_t depth;
static bool failed;

void sm62_flash_core1_prepare(void) {
    __atomic_store_n(&core1_prepared, 1U, __ATOMIC_RELEASE);
}

void sm62_flash_core1_poll(bool quiescent) {
    sm62_flash_worker_park(&gate, quiescent);
}

bool sm62_flash_begin(void) {
    if (!CONTEXT()) return false;
    if (depth) {
        if (failed) return false;
        if (depth == 16U) { failed = true; return false; }
        ++depth;
        return true;
    }
    failed = false;
    if (!__atomic_load_n(&core1_prepared, __ATOMIC_ACQUIRE)) {
        depth = 1;
        return true; // Exact legacy single-core case, no second XIP executor.
    }
    uint32_t start = NOW();
    uint32_t token;
    /* A prior successful write may have released the park while core1 is
     * still restoring IRQs. Wait within the SAME bounded deadline for retire;
     * do not turn an ordinary consecutive page write into a spurious failure. */
    while (!sm62_flash_request(&gate, &token)) {
        if ((uint32_t)(NOW() - start) >= SM62_FLASH_PARK_TIMEOUT_US) return false;
    }
    do {
        if (sm62_flash_granted(&gate, token)) {
            owned_token = token;
            depth = 1;
            return true;
        }
    } while ((uint32_t)(NOW() - start) < SM62_FLASH_PARK_TIMEOUT_US);
    sm62_flash_release(&gate, token); // late ACK cannot authorize this write
    return false;
}

void sm62_flash_failed(void) { failed = true; }
bool sm62_flash_ok(void) { return depth && !failed; }

bool sm62_flash_end(void) {
    if (!depth) return false;
    bool ok = !failed;
    if (--depth) return ok;
    if (owned_token) sm62_flash_release(&gate, owned_token);
    owned_token = 0;
    return ok;
}
