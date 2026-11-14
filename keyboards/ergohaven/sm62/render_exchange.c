// SPDX-License-Identifier: GPL-2.0-or-later
#include "render_exchange.h"
#define LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define STORE(p,v) __atomic_store_n((p),(v),__ATOMIC_RELEASE)
void *sm62_exchange_reserve(sm62_render_exchange_t *e) {
    uint32_t w = LOAD(&e->written);
    if ((uint32_t)(w - LOAD(&e->read)) >= 2U) return 0;
    return e->slots[w & 1U];
}
void sm62_exchange_publish(sm62_render_exchange_t *e) {
    STORE(&e->written, LOAD(&e->written) + 1U);
}
void *sm62_exchange_adopt(sm62_render_exchange_t *e) {
    uint32_t r = LOAD(&e->read), n = LOAD(&e->written) - r;
    if (!e->active) {
        if (!n) return 0;
        e->active = true;
        return e->slots[r & 1U];
    }
    if (n < 2U) return 0;
    /* No access to the previous slot is permitted after this release. */
    ++r;
    STORE(&e->read, r);
    return e->slots[r & 1U];
}
