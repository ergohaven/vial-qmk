/* A standard layout for the Dactyl Manuform 5x6 Keyboard */ 

#include QMK_KEYBOARD_H


#define _QWERTY 0
#define _LOWER 1
#define _RAISE 2
#define _ADJUST 3

#define RAISE MO(_RAISE)
#define LOWER MO(_LOWER)
#define ADJUST MO(_ADJUST)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_QWERTY] = LAYOUT_5x6(
     KC_ESC,  KC_1, KC_2, KC_3, KC_4, KC_5,                             KC_6, KC_7, KC_8,    KC_9,   KC_0,    KC_MINS,
     KC_TAB,  KC_Q, KC_W, KC_E, KC_R, KC_T,                             KC_Y, KC_U, KC_I,    KC_O,   KC_P,    KC_EQL,
     KC_LSFT, KC_A, KC_S, KC_D, KC_F, KC_G,                             KC_H, KC_J, KC_K,    KC_L,   KC_SCLN, KC_QUOT,
     KC_LCTL, KC_Z, KC_X, KC_C, KC_V, KC_B,                             KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_BSLASH,
                    KC_GRV, KC_HYPR,                                                KC_LBRC, KC_RBRC,
                                      KC_LALT, _______,                 _______, KC_BSPC,
                                      KC_DEL,  RAISE,                   LOWER,   KC_PGUP,
                                      KC_LGUI, KC_SPC,                  KC_ENT,  KC_PGDN
  ),

  [_LOWER] = LAYOUT_5x6(

     _______, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC,              KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, _______,
     _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                 KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
     _______, KC_CAPS, KC_INS,  KC_HOME, KC_END,   KC_PSCR,             _______, _______, _______, _______, _______, _______,
     _______, KC_UNDO, KC_CUT,  KC_COPY, KC_PASTE, _______,             _______, _______, _______, _______, _______ , _______,
                       _______, _______,                                                 KC_LCBR, KC_RCBR,
                                            _______, _______,           _______, _______, 
                                            _______, ADJUST,            _______, _______,
                                            _______, _______,            RESET,  _______

  ),

  [_RAISE] = LAYOUT_5x6(
     _______, _______, _______, _______, _______,  _______,             _______, _______, _______, _______, _______, _______,
     _______, _______, _______, KC_UP,   _______,  _______,             _______, _______, _______, _______, _______, _______,
     _______, _______, KC_LEFT, KC_DOWN, KC_RIGHT, _______,             _______, _______, _______, _______, _______, _______,
     _______, KC_F1,   KC_F2,   KC_F3,   KC_F4,    KC_F5,               KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  _______,
                       _______, _______,                                                  KC_F11, KC_F12,
                                           _______, _______,            _______, _______,
                                           _______, _______,            ADJUST,  C(KC_RIGHT),
                                           _______,   RESET,            _______, C(KC_LEFT)
  ),

  [_ADJUST] = LAYOUT_5x6(
    _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______, \
    _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______, \
    RGB_TOG, RGB_HUI, RGB_SAI, RGB_VAI, _______, _______,          _______, KC_VOLD, KC_MUTE, KC_VOLU, _______, _______, \
    RGB_MOD, RGB_HUD, RGB_SAD, RGB_VAD, _______, _______,          _______, KC_MPRV, KC_MPLY, KC_MNXT, _______, _______, \
                      _______, _______,                                     _______, _______,                            \
                                        _______, _______,   _______, _______,                                            \
                                        _______, _______,   _______, _______,                                            \
                                        _______, _______,   _______, _______                                             \
)
};

// Rotary encoder related code
#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
  if (index == 1) { // Encoder on master side
    if(IS_LAYER_ON(_RAISE)) { // on Raise layer
      // Cursor control
      if (clockwise) {
          tap_code(KC_MNXT);
      } else {
          tap_code(KC_MPRV);
      }
    }
    else {
      if (clockwise) {
          tap_code(KC_VOLU);
      } else {
          tap_code(KC_VOLD);
      }
    }
  }
  else if (index == 0) { // Encoder on slave side
    if(IS_LAYER_ON(_LOWER)) { // on Lower layer
      //
      if (clockwise) {
          tap_code(KC_RIGHT);
      } else {
          tap_code(KC_LEFT);
      }
    }
    else {
      if (clockwise) {
          tap_code(KC_DOWN);
      } else {
          tap_code(KC_UP);
      }
    }
  }
    return true;
}
#endif
