// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
/* SM62 core0 flash entry, BEFORE disabling IRQs. False means do not erase or
 * program; caller must propagate failure. Never call while system locked/ISR.
 * End only after XIP is restored. Bounded same-core/thread nesting retains
 * ONE admission across public/logical/backend operations. No worker/ISR use. */
bool sm62_flash_begin(void);
bool sm62_flash_end(void);
void sm62_flash_failed(void);
bool sm62_flash_ok(void);
/* core0: irreversible gate activation BEFORE launching core1, not after its
 * first XIP instruction. A missing/crashed core1 then fails flash closed. */
void sm62_flash_core1_prepare(void);
/* core1: sole render owner after releasing ALL draw/SPI/DMA/RTOS locks.
 * Integration must establish quiescent from actual ownership, not assume it. */
void sm62_flash_core1_poll(bool quiescent);
