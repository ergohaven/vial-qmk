// SPDX-License-Identifier: GPL-2.0-or-later
#include "keyboard.h"

/* SM62 only. Same main-loop owner as QP: no protocol, HID, housekeeping or
 * painter recursion here. The first ordinary QMK task must finish before any
 * painter callback may service input (including initialization refreshes).
 * All direct keyboard_task callers on this target are this wrapper. */
static bool sm62_input_initialized;
static bool sm62_input_running;

void protocol_keyboard_task(void) {
    if (sm62_input_running) return;
    sm62_input_running = true;
    keyboard_task();
    sm62_input_running = false;
    sm62_input_initialized = true;
}

void qp_lvgl_flush_input_service(void) {
#if defined(SM62_DUALCORE_RENDER)
    /* Render core MUST NOT enter keyboard/TD/USB/split dispatch. */
    return;
#else
    if (!sm62_input_initialized || sm62_input_running) return;
    protocol_keyboard_task();
#endif
}
