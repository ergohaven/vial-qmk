// Copyright 2022 Ergohaven (@ergohaven)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "ergohaven.h"

#ifdef EH_HPD_FACTORY_LAYER_NAMES
const char *default_layer_label(uint8_t layer) {
    static const char labels[DYNAMIC_KEYMAP_LAYER_COUNT][LAYER_LABEL_SIZE] PROGMEM = {
        [0] = "Base",
        [1] = "Num + Nav",
        [2] = "Symbols",
#    if defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL) || defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD)
        [4] = "Mouse",
#    endif
    };
    static const char empty[LAYER_LABEL_SIZE] PROGMEM = {0};
    return layer < DYNAMIC_KEYMAP_LAYER_COUNT ? labels[layer] : empty;
}
#endif

const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
          'L', 'L', '*', '*', '*', '*',   '*', '*', '*', '*', 'R', 'R',
                                   '*',   '*'

);
