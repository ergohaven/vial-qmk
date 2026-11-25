#pragma once

#include QMK_KEYBOARD_H
#include "ergohaven.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT(
                        KC_MUTE,
      KC_7, KC_8,       KC_9,
      KC_4, KC_5,       KC_6,
      KC_1, KC_2,       KC_3,
      KC_0, LAYER_PREV, LAYER_NEXT
  ),
  [1] = LAYOUT(
                        _______,
      KC_HOME, KC_INS,  KC_END,
      PREVWRD, KC_UP,   NEXTWRD,
      KC_LEFT, KC_DOWN, KC_RIGHT,
      KC_DEL,  LAYER_PREV, LAYER_NEXT
  ),
  [2] = LAYOUT(
                        KC_BTN3,
      C(KC_X), C(KC_C), C(KC_V),
      KC_BTN1, KC_MS_U, KC_BTN2,
      KC_MS_L, KC_MS_D, KC_MS_R,
      KC_PSCR, LAYER_PREV, LAYER_NEXT
  ),
  [3] = LAYOUT(
                        KC_MUTE,
      KC_BRID, KC_CPNL, KC_BRIU,
      KC_MYCM, KC_WSCH, KC_MAIL,
      KC_MPRV, KC_MPLY, KC_MNXT,
      KC_CALC, LAYER_PREV, LAYER_NEXT
  ),
  // Keep navigation explicit on every remaining page; all other actions stay None.
  [4 ... DYNAMIC_KEYMAP_LAYER_COUNT - 1] = LAYOUT(
                        KC_NO,
      KC_NO, KC_NO, KC_NO,
      KC_NO, KC_NO, KC_NO,
      KC_NO, KC_NO, KC_NO,
      KC_NO, LAYER_PREV, LAYER_NEXT
  ),
};

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
  [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
  [1] = { ENCODER_CCW_CW(KC_PGDN, KC_PGUP) },
  [2] = { ENCODER_CCW_CW(KC_WH_D, KC_WH_U) },
  [3] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
  [4 ... DYNAMIC_KEYMAP_LAYER_COUNT - 1] = { ENCODER_CCW_CW(KC_NO, KC_NO) },
};
#endif
