#pragma once

#include "transactions.h"

// Transport success alone does not mean a booting peer ran its callback.
static inline bool eh_sync_settings(int8_t id, uint8_t length, const void *settings) {
    uint8_t applied = 0;
    return transaction_rpc_exec(id, length, settings, sizeof(applied), &applied) && applied == 1;
}

static inline void eh_sync_settings_ack(uint8_t length, void *response) {
    if (length == 1 && response != NULL) {
        *(uint8_t *)response = 1;
    }
}
