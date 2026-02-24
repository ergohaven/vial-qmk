#include QMK_KEYBOARD_H
#include "ergohaven.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
        [_BASE] = LAYOUT(
          KC_ESC,  KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,                                          KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,     KC_BSPC,
          KC_TAB,  KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                                          KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN,  KC_RGUI,
          KC_LSFT, KC_Z,   KC_X,    KC_C,    KC_V,    KC_B,                                          KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_QUOT,  KC_RSFT,
                                             KC_LCTL, LOWER,   KC_SPC, KC_LBRC,    KC_RBRC, KC_ENT,  RAISE,   KC_RALT
        ),

        [_LOWER] = LAYOUT(
         _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                                          KC_6,    KC_7,    KC_8,    KC_9,    KC_0,     _______,
         _______, KC_HOME, KC_INS,  KC_DEL,  KC_END,  LANG,                                          _______, KC_LEFT, KC_DOWN, KC_UP,   KC_RIGHT, _______,
         _______, C(KC_Z), C(KC_X), C(KC_C), C(KC_V), KC_PSCR,                                       KC_CAPS, PREVWRD, KC_PGDN, KC_PGUP, NEXTWRD,  _______,
                                             _______, _______, _______, _______,   _______, _______, ADJUST,  _______
        ),

        [_RAISE] = LAYOUT(
         _______, KC_AT,   KC_LT,   KC_EQL,  KC_GT,   KC_GRV,                                        KC_CIRC, KC_LBRC, KC_UNDS, KC_RBRC, _______,  _______,
         _______, KC_BSLS, KC_LPRN, KC_MINS, KC_RPRN, KC_PLUS,                                       KC_PERC, KC_LCBR, KC_SCLN, KC_RCBR, KC_EXLM,  KC_QUES,
         _______, KC_HASH, KC_ASTR, KC_COLN, KC_SLSH, _______,                                       _______, KC_PIPE, KC_TILD, KC_AMPR, KC_DLR,   _______,
                                             _______, ADJUST,  _______, _______,   _______, _______, _______, _______
        ),

      [_ADJUST] = LAYOUT(

        KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,                                          KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,   KC_F12,
        _______, _______, _______, _______, _______, _______,                                        _______, KC_VOLD, KC_MUTE, _______, KC_VOLU,  _______,
        QK_BOOT, DM_PLY1, DM_REC1, DM_RSTP, _______, CG_TOGG,                                        _______, KC_MPRV, KC_MPLY, _______, KC_MNXT,  _______,
                                            _______, _______,  _______, _______,   _______, _______, _______, _______

       ),
};

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
  [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
  [1] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
  [2] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
  [3] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
};
#endif
