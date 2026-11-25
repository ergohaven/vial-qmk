#include QMK_KEYBOARD_H
#include "ergohaven.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
        [_BASE] = LAYOUT( \
         KC_ESC,   KC_Q,   KC_W,    KC_E,     KC_R,     KC_T,                                              KC_Y,   KC_U,    KC_I,     KC_O,   KC_P,    KC_BSPC,
         KC_TAB,   KC_A,   KC_S,    KC_D,     KC_F,     KC_G,                                              KC_H,   KC_J,    KC_K,     KC_L,   KC_SCLN, KC_BSLS,
         KC_LSFT,  KC_Z,   KC_X,    KC_C,     KC_V,     KC_B,                                              KC_N,   KC_M,    KC_COMM,  KC_DOT, KC_QUOT, KC_RSFT,
                   KC_GRV, KC_LGUI, KC_LCTL,  LOWER,    KC_SPC,                                            KC_ENT, RAISE,   KC_LALT,  KC_LBRC,  KC_RBRC
        ),

        [_LOWER] = LAYOUT(
        _______,   KC_1,      KC_2,      KC_3,      KC_4,      KC_5,                          KC_6,      KC_7,      KC_8,      KC_9,      KC_0,      _______,
        _______,   KC_HOME,   KC_INS,    KC_DEL,    KC_END,    KC_ENT,                        _______,   KC_LEFT,   KC_DOWN,   KC_UP,     KC_RIGHT,  _______,
        _______,   _______,   _______,   _______,   _______,   KC_PSCR,                       _______,   _______,   _______,   _______,   _______,   _______,
        KC_PGDN,   KC_PGUP,   _______,   _______,   _______,                       _______,   ADJUST,    _______,   KC_CAPS,   CW_TOGG
    ),

        [_RAISE] = LAYOUT(
        _______,   KC_AT,     KC_LABK,   KC_EQL,    KC_RABK,   KC_GRV,                        KC_CIRC,   KC_LBRC,   KC_UNDS,   KC_RBRC,   _______,   _______,
        _______,   KC_BSLS,   KC_LPRN,   KC_MINS,   KC_RPRN,   KC_PLUS,                       KC_PERC,   KC_LCBR,   KC_SCLN,   KC_RCBR,   KC_EXLM,   _______,
        _______,   KC_HASH,   KC_ASTR,   KC_COLN,   KC_SLSH,   _______,                       _______,   KC_PIPE,   KC_TILD,   KC_AMPR,   KC_DLR,    _______,
        _______,   _______,   _______,   ADJUST,    _______,                       _______,   _______,   _______,   _______,   _______
    ),

      [_ADJUST] = LAYOUT(
        KC_F1,     KC_F2,     KC_F3,     KC_F4,     KC_F5,     KC_F6,                         KC_F7,     KC_F8,     KC_F9,     KC_F10,    KC_F11,    KC_F12,
        _______,   _______,   KC_MPRV,   KC_MPLY,   KC_MNXT,   _______,                       _______,   KC_VOLD,   KC_MUTE,   KC_VOLU,   _______,   _______,
        _______,   _______,   _______,   _______,   _______,   _______,                       _______,   _______,   _______,   _______,   _______,   _______,
        _______,   _______,   _______,   _______,   _______,                       _______,   _______,   _______,   _______,   _______
    ),
};
