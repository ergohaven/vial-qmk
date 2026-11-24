#include "display.h"
#include "ergohaven.h"
#include "src/display/eh_display.h"

#include QMK_KEYBOARD_H

const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
          '*', '*', '*', '*', '*', '*',   '*', '*', '*', '*', '*', '*'
);

void housekeeping_task_user(void) {
    if (is_display_enabled()) {
        display_housekeeping_task();
    }
}

uint8_t get_lcd_brightness(void) {
    return get_backlight_level();
}

void set_lcd_brightness(uint8_t brightness) {
    backlight_level(brightness);
}

void keyboard_post_init_user(void) {
    display_init_kb();
}
