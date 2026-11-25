#include QMK_KEYBOARD_H
#include "kissetfall.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
      [_BASE] = LAYOUT(
         KC_F1,   KC_F2, KC_F3,   KC_F4,   KC_F5,   KC_F6,                                              KC_F7,  KC_F8,   KC_F9,   KC_F10, KC_F11,  KC_F12,
         KC_TAB,  KC_Q,  KC_W,    KC_E,    KC_R,    KC_T,                                               KC_Y,   KC_U,    KC_I,    KC_O,   KC_P,    KC_GRV,
         KC_LALT, KC_A,  KC_S,    KC_D,    KC_F,    KC_G,                                               KC_H,   KC_J,    KC_K,    KC_L,   KC_SCLN, KC_ENT,
         KC_ESC,  KC_Z,  KC_X,    KC_C,    KC_V,    KC_B,                                               KC_N,   KC_M,    KC_COMM, KC_DOT, KC_QUOT, KC_MPLY,
                         KC_VOLD, KC_VOLU, KC_LCTL, LOWER, KC_LSFT, KC_SPC,            KC_BSPC, KC_RCTL, RAISE,  KC_RSFT, PREVWRD, NEXTWRD

      ),

      [_LOWER] = LAYOUT(

         _______, _______, _______, _______, _______, _______,                                          _______, _______, _______, _______, _______,  _______,
         _______, KC_3,    KC_2,    KC_1,    KC_0,    KC_4,                                             KC_7,    KC_6,    KC_5,    KC_9,    KC_8,     _______,
         KC_ENT,  KC_LSFT, APP_S,   KC_DEL,  WNEXT,   LANG,                                             KC_HOME, KC_LEFT, KC_DOWN, KC_UP,   KC_RIGHT, _______,
         _______, _______, _______, APP_C,   _______, _______,                                          KC_END,  KC_HOME, CTRLDN,  CTRLUP,  KC_END,  _______,
                           KC_MPRV, KC_MNXT, _______, _______, _______, _______,      _______, _______, ADJUST, _______,  KC_PGDN, KC_PGUP

      ),

         [_RAISE] = LAYOUT(
         _______, _______, _______, KC_AT,   _______, _______,                                          _______, _______, KC_CIRC, _______, _______, _______,
         _______, _______, KC_TILD, KC_EQL,  KC_PLUS, _______,                                          _______, KC_LPRN, KC_UNDS, KC_RPRN, _______, _______,
         _______, KC_BSLS, KC_DLR,  KC_MINS, KC_HASH, KC_EXLM,                                          KC_QUES, KC_LBRC, KC_PERC, KC_RBRC, KC_AMPR, _______,
         _______, _______, KC_ASTR, KC_COLN, KC_SLSH, _______,                                          _______, KC_LCBR, KC_PIPE, KC_RCBR, _______, _______,
                           _______, _______, _______, ADJUST, _______, _______,       _______, _______, _______, _______, _______, _______
      ),

      [_ADJUST] = LAYOUT(
        QK_BOOT, _______, _______, _______, _______, _______,                                           _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                                           _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                                           KC_CAPS, _______, KC_BRID, KC_BRIU, _______, _______,
        _______, _______, _______, _______, _______, _______,                                           KC_PSCR, _______, _______, _______, _______, _______,
                          _______, _______, _______, _______, _______, _______,       _______, _______, _______, _______, _______, _______
       )
};
