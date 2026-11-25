#include "display.h"
#include "ergohaven.h"
#include "src/display/eh_display.h"

static bool display_housekeeping_pending = false;

void matrix_scan_user(void) {
    display_housekeeping_pending = true;
}

void housekeeping_task_user(void) {
    if (!display_housekeeping_pending) return;

    display_housekeeping_pending = false;
    display_housekeeping_task();
}

void keyboard_post_init_user(void) {
    display_init_kb();
}
