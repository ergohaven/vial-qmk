#include QMK_KEYBOARD_H
#include "ergohaven.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT(
    KC_1,     KC_2,    KC_3,    KC_4,     KC_5,
    KC_ESC,   KC_Q,    KC_W,    KC_E,     KC_R,     KC_T,
    KC_TAB,   KC_A,    KC_S,    KC_D,     KC_F,     KC_G,
    KC_LSFT,  KC_Z,    KC_X,    KC_C,     KC_V,     KC_B,     KC_ENTER,
    KC_LCTL,  KC_NO,   KC_NO,   KC_LALT,  LOWER,    KC_SPC
  ),

  [1] = LAYOUT(
    KC_6,     KC_7,    KC_8,    KC_9,     KC_0,
    KC_F1,    KC_Y,    KC_U,    KC_I,     KC_O,     KC_P,
    KC_F2,    KC_H,    KC_J,    KC_K,     KC_L,     KC_MPLY,
    KC_F5,    KC_N,    KC_M,    KC_VOLD,  KC_VOLU,  KC_MNXT,  KC_BSPC,
    KC_F9,    KC_F12,  KC_LGUI, KC_NO,    KC_NO,    KC_NO
  ),
};

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
  [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
  [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif
