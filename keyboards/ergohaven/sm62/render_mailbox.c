// SPDX-License-Identifier: GPL-2.0-or-later
#include "render_mailbox.h"
#include <string.h>

_Static_assert((SM62_RENDER_CAPACITY & (SM62_RENDER_CAPACITY - 1U)) == 0, "power of two ring");
#define LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define STORE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define SLOT(n) ((n) & (SM62_RENDER_CAPACITY - 1U))

void sm62_render_init(sm62_render_mailbox_t *m) {
    /* Cold init only, before either owner starts. Runtime rearm preserves the
     * identity sequence so a late completion cannot alias a newer lease. */
    memset(m, 0, sizeof(*m));
}

bool sm62_render_ticket_equal(sm62_render_ticket_t a, sm62_render_ticket_t b) {
    return a.epoch == b.epoch && a.sequence == b.sequence;
}

bool sm62_render_publish(sm62_render_mailbox_t *m, const void *bytes, size_t size, sm62_render_ticket_t *ticket) {
    uint32_t p = LOAD(&m->published);
    if (LOAD(&m->stopped) || m->exhausted || size > SM62_RENDER_BYTES || (size && !bytes) || !ticket ||
        (uint32_t)(p - LOAD(&m->reaped)) >= SM62_RENDER_CAPACITY) return false;
    sm62_render_message_t *s = &m->messages[SLOT(p)];
    s->ticket = (sm62_render_ticket_t){m->epoch, m->sequence};
    s->size = size;
    if (size) memcpy(s->bytes, bytes, size);
    *ticket = s->ticket;
    if (++m->sequence == 0 && ++m->epoch == 0) m->exhausted = true;
    STORE(&m->published, p + 1U);
    return true;
}

bool sm62_render_take(sm62_render_mailbox_t *m, sm62_render_message_t *out) {
    uint32_t t = LOAD(&m->taken);
    if (!out || t != LOAD(&m->completed) || t == LOAD(&m->published)) return false;
    /* A complete value copy is owned by core1, never a live native pointer. */
    *out = m->messages[SLOT(t)];
    STORE(&m->taken, t + 1U);
    return true;
}

bool sm62_render_complete(sm62_render_mailbox_t *m, sm62_render_ticket_t ticket, bool success) {
    uint32_t c = LOAD(&m->completed);
    if (c == LOAD(&m->taken) || !sm62_render_ticket_equal(ticket, m->messages[SLOT(c)].ticket)) return false;
    m->success[SLOT(c)] = success;
    STORE(&m->completed, c + 1U);
    return true;
}

bool sm62_render_poll(sm62_render_mailbox_t *m, sm62_render_completion_t *out) {
    uint32_t r = LOAD(&m->reaped);
    if (!out || r == LOAD(&m->completed)) return false;
    *out = (sm62_render_completion_t){m->messages[SLOT(r)].ticket, m->success[SLOT(r)]};
    STORE(&m->reaped, r + 1U);
    return true;
}

void sm62_render_shutdown(sm62_render_mailbox_t *m) {
    /* Stop accepting new work; worker drains or explicitly aborts each queued
     * message, and producer still reaps failures. Never fake success. */
    STORE(&m->stopped, 1U);
}

bool sm62_render_rearm(sm62_render_mailbox_t *m) {
    if (!LOAD(&m->stopped) || LOAD(&m->published) != LOAD(&m->reaped) || m->exhausted) return false;
    STORE(&m->stopped, 0U);
    return true;
}
