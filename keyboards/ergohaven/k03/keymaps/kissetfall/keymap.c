#include QMK_KEYBOARD_H
#include "oled/ergohaven_dark.c"
#include "kissetfall.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
      [_BASE] = LAYOUT(
         KC_F1,   KC_F2, KC_F3,   KC_F4,   KC_F5,   KC_F6,                                                  KC_F7, KC_F8,   KC_F9,   KC_F10, KC_F11,  KC_F12,
         KC_TAB,  KC_Q,  KC_W,    KC_E,    KC_R,    KC_T,                                                   KC_Y,  KC_U,    KC_I,    KC_O,   KC_P,    KC_GRV,
         KC_LALT, KC_A,  KC_S,    KC_D,    KC_F,    KC_G,                                                   KC_H,  KC_J,    KC_K,    KC_L,   KC_SCLN, KC_ENT,
         KC_ESC,  KC_Z,  KC_X,    KC_C,    KC_V,    KC_B,                                                   KC_N,  KC_M,    KC_COMM, KC_DOT, KC_QUOT, KC_MPLY,
                         KC_VOLD, KC_VOLU, KC_LCTL, LOWER, KC_SPC, PREVWRD,               NEXTWRD, KC_BSPC, RAISE, KC_RSFT, KC_PGDN, KC_PGUP

      ),

      [_LOWER] = LAYOUT(

         _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______,  _______,
         _______, KC_3,    KC_2,    KC_1,    KC_0,    KC_4,                                                 KC_7,    KC_6,    KC_5,    KC_9,    KC_8,     _______,
         KC_ENT,  APP_A,   ALT_TAB, KC_DEL,  KC_LSFT, LANG,                                                 _______, KC_LEFT, KC_DOWN, KC_UP,   KC_RIGHT, _______,
         _______, KC_UNDO, KC_CUT,  KC_COPY, KC_PSTE, APP_B,                                                _______, KC_HOME, CTRLDN,  CTRLUP,  KC_END,   _______,
                           KC_MPRV, KC_MNXT, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______

      ),

         [_RAISE] = LAYOUT(
         _______, _______, _______, KC_AT,   _______, _______,                                              _______, _______, KC_CIRC, _______, _______, _______,
         _______, _______, KC_TILD, KC_EQL,  KC_PLUS, _______,                                              _______, KC_LPRN, KC_UNDS, KC_RPRN, _______, _______,
         _______, KC_BSLS, KC_DLR,  KC_MINS, KC_HASH, KC_EXLM,                                              KC_QUES, KC_LBRC, KC_PERC, KC_RBRC, KC_AMPR, _______,
         _______, _______, KC_ASTR, KC_COLN, KC_SLSH, _______,                                              _______, KC_LCBR, KC_PIPE, KC_RCBR, _______, _______,
                           _______, _______, _______, _______,  _______, _______,         _______, _______, _______, _______, _______, _______
      )

};

// #ifdef ENCODER_MAP_ENABLE
// const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
//   [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(PREVWRD, NEXTWRD)},
//   [1] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(CTRLDN,   CTRLUP)},
//   [2] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
// };
// #endif

#ifdef OLED_ENABLE

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
  if (!is_keyboard_master()) {
    return OLED_ROTATION_270;
     }
    else {
    return OLED_ROTATION_270;
    }
  return rotation;
}

void render_layer_state(void) {
    // Print current mode
    oled_write_ln_P(PSTR("K:03\n"), false);
    /* oled_write_P(PSTR("\n"), false); */
    oled_write_ln_P(PSTR("v3.2\n"), false);
    oled_write_P(PSTR("\n"), false);
    oled_write_ln_P(PSTR("MODE\n"), false);
    if (keymap_config.swap_lctl_lgui) {
        oled_write_ln_P(PSTR("Mac"), false);
    } else {
        oled_write_ln_P(PSTR("Win"), false);
    }

    oled_write_P(PSTR("\n\n"), false);
    // Print current layer
    oled_write_ln_P(PSTR("LAYER"), false);
    switch (get_highest_layer(layer_state)) {
        case _BASE:
            oled_write_P(PSTR("Base\n"), false);
            break;
        case _RAISE:
            oled_write_P(PSTR("Raise"), false);
            break;
        case _LOWER:
            oled_write_P(PSTR("Lower"), false);
            break;
        case _ADJUST:
            oled_write_P(PSTR("Adjst"), false);
            break;
        case _FOUR:
            oled_write_P(PSTR("Four\n"), false);
            break;
        case _FIVE:
            oled_write_P(PSTR("Five\n"), false);
            break;
        case _SIX:
            oled_write_P(PSTR("Six\n"), false);
            break;
        case _SEVEN:
            oled_write_P(PSTR("Seven"), false);
            break;
        case _EIGHT:
            oled_write_P(PSTR("Eight"), false);
            break;
        case _NINE:
            oled_write_P(PSTR("Nine\n"), false);
            break;
         case _TEN:
            oled_write_P(PSTR("Ten\n"), false);
            break;
         case _ELEVEN:
            oled_write_P(PSTR("Elevn"), false);
            break;
         case _TWELVE:
            oled_write_P(PSTR("Twlve"), false);
            break;
         case _THIRTEEN:
            oled_write_P(PSTR("Thrtn"), false);
            break;
         case _FOURTEEN:
            oled_write_P(PSTR("Frtn\n"), false);
            break;
         case _FIFTEEN:
            oled_write_P(PSTR("Fiftn"), false);
            break;
        default:
            oled_write_ln_P(PSTR("Undef"), false);
    }
    oled_write_P(PSTR("\n"), false);
    led_t led_usb_state = host_keyboard_led_state();
    oled_write_ln_P(PSTR("CPSLK"), led_usb_state.caps_lock);
}


// Used to draw on to the oled screen
bool oled_task_user(void) {
    if (is_keyboard_master()) {
        render_layer_state();
    } else {
        ergohaven_dark_draw();
}
    return false;
}

#endif

#ifdef RGBLIGHT_ENABLE
const rgblight_segment_t PROGMEM my_base_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 4, HSV_WHITE}
);
const rgblight_segment_t PROGMEM my_layer1_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 4, HSV_CYAN}
);
const rgblight_segment_t PROGMEM my_layer2_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 4, HSV_RED}
);
const rgblight_segment_t PROGMEM my_layer3_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 4, HSV_GOLDENROD}
);

// Now define the array of layers. Later layers take precedence
const rgblight_segment_t* const PROGMEM my_rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    my_base_layer,
    my_layer1_layer,
    my_layer2_layer,
    my_layer3_layer
);


void keyboard_post_init_user(void) {
    // Enable the LED layers
    rgblight_layers = my_rgb_layers;
}

layer_state_t default_layer_state_set_user(layer_state_t state) {
    rgblight_set_layer_state(0, layer_state_cmp(state, _BASE));
    return state;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    rgblight_set_layer_state(1, layer_state_cmp(state, _RAISE));
    rgblight_set_layer_state(2, layer_state_cmp(state, _LOWER));
    rgblight_set_layer_state(3, layer_state_cmp(state, _ADJUST));
    return state;

}
#endif
