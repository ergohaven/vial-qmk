#include QMK_KEYBOARD_H


#define _BASE 0
#define _RAISE 1
#define _LOWER 2
#define _ADJUST 3

// Fillers to make layering more clear

#define ____ KC_TRNS

#define SFT_ESC  SFT_T(KC_ESC)
#define CTL_BSPC CTL_T(KC_BSPC)
#define ALT_SPC  ALT_T(KC_SPC)
#define SFT_ENT  SFT_T(KC_ENT)

#define KC_ML KC_MS_LEFT
#define KC_MR KC_MS_RIGHT
#define KC_MU KC_MS_UP
#define KC_MD KC_MS_DOWN
#define KC_MB1 KC_MS_BTN1
#define KC_MB2 KC_MS_BTN2

#define RAISE MO(_RAISE)
#define LOWER MO(_LOWER)
#define ADJUST MO(_ADJUST)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

// Base (qwerty)

[_BASE] = LAYOUT( \
  KC_Q, KC_W,   KC_E,   KC_R, KC_T,                             KC_Y, KC_U, KC_I,    KC_O,   KC_P,    \
  KC_A, KC_S,   KC_D,   KC_F, KC_G,                             KC_H, KC_J, KC_K,    KC_L,   KC_SCLN, \
  KC_Z, KC_X,   KC_C,   KC_V, KC_B,                             KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, \
        KC_GRV, KC_TAB,                                                     KC_LBRC, KC_RBRC,         \
                              KC_LCTL, ____,           ____,    KC_RSFT,                              \
                              KC_DEL,  RAISE,          LOWER,   C(KC_RIGHT),                          \
                              KC_LGUI, KC_LALT,        KC_MPLY, C(KC_LEFT)
),

// Raise

[_RAISE] = LAYOUT( \
  KC_1,   KC_2,    KC_3,    KC_4,   KC_5,                        KC_6,    KC_7,    KC_8,   KC_9,     KC_0,   \
  KC_ESC, KC_SPC,  KC_LSFT, KC_ENT, KC_BSPC,                     KC_LEFT, KC_DOWN, KC_UP,  KC_RIGHT, ____,   \
  KC_F1,  KC_F2,   KC_F3,   KC_F4,  KC_F5,                       KC_F6,   KC_F7,   KC_F8,  KC_F9,    KC_F10, \
          KC_TILD, KC_CAPS,                                                        KC_F11, KC_F12,           \
                                    ____, ____,          ____,   ____,                                       \
                                    ____,  ____,         ADJUST, KC_PGUP,                                    \
                                    RESET, ____,         ____,   KC_PGDN                                     \
),

// Lower

[_LOWER] = LAYOUT( \
  KC_EXLM, KC_AT,  KC_HASH, KC_DLR, KC_PERC,                       KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, \
  KC_ESC,  KC_SPC, KC_LSFT,  KC_ENT, KC_BSPC,                      KC_MINS, KC_EQL,  KC_BSLS, ____,    KC_QUOT, \
  ____,    ____,   ____,    ____,   ____,                          KC_UNDS, KC_PLUS, KC_PIPE, ____,    KC_DQUO, \
                   ____,    ____,                                                    KC_LCBR, KC_RCBR,          \
                                    ____,       ____,        ____, ____,                                        \
                                    ____,       ADJUST,      ____, ____,                                        \
                                    C(KC_LALT), ____,        ____, RESET                                        \
), 

// Adjust

[_ADJUST] = LAYOUT( \
    ____,    ____,    ____,    ____,    ____,                      ____, ____,    ____,    ____,    ____, \
    RGB_TOG, RGB_HUI, RGB_SAI, RGB_VAI, ____,                      ____, KC_VOLD, KC_MUTE, KC_VOLU, ____, \
    RGB_MOD, RGB_HUD, RGB_SAD, RGB_VAD, ____,                      ____, KC_MPRV, KC_MPLY, KC_MNXT, ____, \
                      ____,    ____,                                               ____, ____,            \
                                        ____, ____,          ____, ____,                                  \
                                        ____, ____,          ____, ____,                                  \
                                        ____, ____,          ____, ____                                   \
)
};

void persistent_default_layer_set(uint16_t default_layer) {
  eeconfig_update_default_layer(default_layer);
  default_layer_set(default_layer);
}

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
