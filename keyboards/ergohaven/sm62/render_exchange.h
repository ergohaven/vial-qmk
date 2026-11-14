// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define SM62_SNAPSHOT_BYTES 6144U
/* SPSC, two fixed slots, no RMW/allocation/RTOS lock. The current consumer
 * snapshot stays leased until it adopts its replacement at a frame boundary.
 * A full exchange simply declines a core0 reservation; input latches are NOT
 * cleared until successful publication. Caller never waits for a frame. */
typedef struct {
    uint32_t written, read;
    bool active; /* consumer-only */
    _Alignas(8) uint8_t slots[2][SM62_SNAPSHOT_BYTES];
} sm62_render_exchange_t;
void *sm62_exchange_reserve(sm62_render_exchange_t *e);
void sm62_exchange_publish(sm62_render_exchange_t *e);
void *sm62_exchange_adopt(sm62_render_exchange_t *e);
