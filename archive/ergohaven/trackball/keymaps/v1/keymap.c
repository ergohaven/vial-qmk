#include QMK_KEYBOARD_H
#include "ergohaven.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_UP, KC_DOWN, KC_LEFT, KC_RIGHT),
};


const int DPI_TABLE[15] = {400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6300, 8000, 8200};
