// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Protocol prerequisite, NOT a complete screen snapshot adapter. Exactly one
 * core0 publisher/reaper and one core1 taker/completer. No ISR/split worker may
 * enter these APIs. No allocation, callbacks, global RTOS lock or pointer
 * publication. Release/acquire u32 stores/loads work on ARMv6-M without RMW
 * libatomic helpers. A slot remains leased until core0 reaps its completion.
 * Full queues reject explicitly: callers MUST retain/retry events in FIFO order;
 * press/release messages must never be silently coalesced or dropped. */
#define SM62_RENDER_CAPACITY 4U
#define SM62_RENDER_BYTES 1024U

typedef struct { uint32_t epoch, sequence; } sm62_render_ticket_t;
typedef struct {
    sm62_render_ticket_t ticket;
    size_t size;
    uint8_t bytes[SM62_RENDER_BYTES];
} sm62_render_message_t;
typedef struct { sm62_render_ticket_t ticket; bool success; } sm62_render_completion_t;
typedef struct {
    uint32_t published, taken, completed, reaped, stopped;
    uint32_t epoch, sequence;
    bool exhausted;
    sm62_render_message_t messages[SM62_RENDER_CAPACITY];
    bool success[SM62_RENDER_CAPACITY];
} sm62_render_mailbox_t;

void sm62_render_init(sm62_render_mailbox_t *m);
bool sm62_render_publish(sm62_render_mailbox_t *m, const void *bytes, size_t size, sm62_render_ticket_t *ticket);
bool sm62_render_take(sm62_render_mailbox_t *m, sm62_render_message_t *out);
bool sm62_render_complete(sm62_render_mailbox_t *m, sm62_render_ticket_t ticket, bool success);
bool sm62_render_poll(sm62_render_mailbox_t *m, sm62_render_completion_t *out);
void sm62_render_shutdown(sm62_render_mailbox_t *m);
bool sm62_render_rearm(sm62_render_mailbox_t *m);
bool sm62_render_ticket_equal(sm62_render_ticket_t a, sm62_render_ticket_t b);
