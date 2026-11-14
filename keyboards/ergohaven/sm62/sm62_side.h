// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include "keyboard.h"

/* Physical side is latched by QMK from SPLIT_HAND_PIN (GP29) at boot.
 * It is independent of which half owns USB. Keep the wire/settings numbering:
 * index 0 / ID 1 = left, index 1 / ID 2 = right. */
static inline uint8_t sm62_local_side_index(void) {
    return is_keyboard_left() ? 0U : 1U;
}

static inline uint8_t sm62_side_id(void) {
    return sm62_local_side_index() + 1U;
}
