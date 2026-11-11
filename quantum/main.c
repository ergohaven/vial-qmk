/* Copyright 2021 QMK
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "keyboard.h"
#include "sm62_trace.h"

void platform_setup(void);

void protocol_setup(void);
void protocol_pre_init(void);
void protocol_post_init(void);
void protocol_pre_task(void);
void protocol_post_task(void);

// Bodge as refactoring this area sucks....
void protocol_keyboard_task(void) __attribute__((weak));
void protocol_keyboard_task(void) {
    keyboard_task();
}

/** \brief Main
 *
 * FIXME: Needs doc
 */
int main(void) __attribute__((weak));
int main(void) {
    platform_setup();
    protocol_setup();
    keyboard_setup();

    protocol_pre_init();
    keyboard_init();
    protocol_post_init();

    /* Main loop */
    while (true) {
        sm62_trace_loop();
        sm62_trace_edge(0,true);
        protocol_pre_task();
        sm62_trace_edge(0,false);
        sm62_trace_edge(1,true);
        protocol_keyboard_task();
        sm62_trace_edge(1,false);
        sm62_trace_edge(2,true);
        protocol_post_task();
        sm62_trace_edge(2,false);

#ifdef RAW_ENABLE
        void raw_hid_task(void);
        sm62_trace_edge(3,true);
        raw_hid_task();
        sm62_trace_edge(3,false);
#endif

#ifdef CONSOLE_ENABLE
        void console_task(void);
        console_task();
#endif

#if defined(QUANTUM_PAINTER_ENABLE) && !defined(SM62_DUALCORE_RENDER)
        // Run Quantum Painter task
        void qp_internal_task(void);
        sm62_trace_edge(4,true);
        qp_internal_task();
        sm62_trace_edge(4,false);
#endif

#ifdef DEFERRED_EXEC_ENABLE
        // Run deferred executions
        void deferred_exec_task(void);
        sm62_trace_edge(5,true);
        deferred_exec_task();
        sm62_trace_edge(5,false);
#endif // DEFERRED_EXEC_ENABLE

        sm62_trace_edge(6,true);
        housekeeping_task();
        sm62_trace_edge(6,false);
    }
}
