#include QMK_KEYBOARD_H
#include "ergohaven.h"
#include "src/eh_pointing.h"

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,                                             KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_BSPC,
KC_ESC,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,                                             KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_BSLS,
KC_TAB,   KC_A,     KC_S,     KC_D,     KC_F,     KC_G,                                             KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,
KC_LSFT,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,                                             KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,  KC_RSFT,
                    KC_PGDN,  KC_PGUP,  KC_LCTL,  LOWER,    KC_LALT,  KC_SPACE,           KC_ENT,   KC_RGUI,  RAISE,    KC_RCTL,  KC_LBRC,  KC_RBRC,
                                                                      KC_MUTE,            KC_MUTE
    ),

    [_LOWER] = LAYOUT(
KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,                                            KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,
KC_VOLU,  KC_MNXT,  KC_HOME,  KC_UP,    KC_END,   KC_INS,                                           _______,  KC_7,     KC_8,     KC_9,     _______,  _______,
KC_MUTE,  KC_MPLY,  KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_ENT,                                           _______,  KC_4,     KC_5,     KC_6,     _______,  _______,
KC_VOLD,  KC_MPRV,  PREVWRD,  _______,  NEXTWRD,  KC_DEL,                                           KC_PSCR,  KC_1,     KC_2,     KC_3,     _______,  _______,
                    _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  KC_0,     _______,
                                                                      _______,            _______
    ),

    [_RAISE] = LAYOUT(
_______,  _______,  KC_AT,    KC_DLR,   KC_HASH,  _______,                                          _______,  _______,  KC_CIRC,  _______,  _______,  _______,
_______,  _______,  KC_LT,    KC_EQL,   KC_GT,    KC_GRV,                                           _______,  KC_LBRC,  KC_UNDS,  KC_RBRC,  _______,  _______,
_______,  KC_BSLS,  KC_LPRN,  KC_MINS,  KC_RPRN,  KC_PLUS,                                          KC_PERC,  KC_LCBR,  KC_SCLN,  KC_RCBR,  KC_EXLM,  _______,
_______,  _______,  KC_ASTR,  KC_COLN,  KC_SLSH,  _______,                                          _______,  KC_PIPE,  KC_TILD,  KC_AMPR,  _______,  _______,
                    _______,  _______,  _______,  _______,  _______,  _______,          _______,    _______,  _______,  _______,  _______,  _______,
                                                                      _______,          _______
    ),

    [_ADJUST] = LAYOUT(
_______,  _______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,  _______,
_______,  _______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,  _______,
_______,  _______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,  _______,
_______,  _______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,  _______,
                    _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
                                                                      _______,            _______
    ),

    [_FOUR] = LAYOUT(
_______,  _______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,  _______,
_______,  _______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,  _______,
_______,  EH_SCR,   KC_BTN3,  KC_BTN2,  KC_BTN1,  EH_SNP,                                           EH_SNP,   KC_BTN1,  KC_BTN2,  KC_BTN3,  EH_SCR,   _______,
_______,  _______,  _______,  _______,  _______,  EH_TXT,                                           EH_TXT,   _______,  _______,  _______,  _______,  _______,
                    _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
                                                                      _______,            _______
    ),
};
// clang-format on

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)}, //
    [1] = {ENCODER_CCW_CW(_______, _______)}, //
    [2] = {ENCODER_CCW_CW(_______, _______)}, //
    [3] = {ENCODER_CCW_CW(_______, _______)}, //
    [4] = {ENCODER_CCW_CW(_______, _______)}, //
};
#endif
