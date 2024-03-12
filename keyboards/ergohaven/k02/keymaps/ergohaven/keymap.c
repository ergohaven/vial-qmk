#include QMK_KEYBOARD_H
#include "ergohaven.h"
// #include "oled/bongocat.c"
// #include "oled/ergohaven_light.c"
#include "oled/ergohaven_dark.c"
// #include "font_block.h"
// #include "game/game.h"
// #include "layers.c"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
        [_BASE] = LAYOUT( \
          KC_GRV,   KC_1,    KC_2,    KC_3,     KC_4,     KC_5,                         KC_6,  KC_7,     KC_8,     KC_9,   KC_0,    KC_BSPC, \
          KC_ESC,   KC_Q,    KC_W,    KC_E,     KC_R,     KC_T,                         KC_Y,  KC_U,     KC_I,     KC_O,   KC_P,    KC_BSLS, \
          KC_TAB,   KC_A,    KC_S,    KC_D,     KC_F,     KC_G,                         KC_H,  KC_J,     KC_K,     KC_L,   KC_SCLN, KC_QUOT, \
          KC_LSFT,  KC_Z,    KC_X,    KC_C,     KC_V,     KC_B,                         KC_N,  KC_M,     KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT, \
                             KC_LGUI, KC_LALT,  KC_LCTL,  LOWER, KC_SPC,        KC_ENT, RAISE, KC_RCTL, KC_RALT,  KC_RGUI \
        ),

        [_LOWER] = LAYOUT( \
         KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,    KC_F6,                           KC_F7,   KC_F8,   KC_F9,   KC_F10, KC_F11,  KC_F12,
         QK_BOOT, _______, KC_HOME, KC_UP,   KC_END,   KC_INS,                          KC_PGUP, KC_7,    KC_8,    KC_9,   _______, _______,
         _______, _______, KC_LEFT, KC_DOWN, KC_RIGHT, KC_ENT,                          KC_PGDN, KC_4,    KC_5,    KC_6,   _______, _______,
         _______, _______, PREVWRD, KC_CAPS, NEXTWRD,  KC_DEL,                          KC_PSCR, KC_1,    KC_2,    KC_3,   _______, _______,
                           _______, _______, _______, _______, _______,        _______, ADJUST, _______, _______, _______\
        ),

        [_RAISE] = LAYOUT( \
         _______, _______, KC_AT,   KC_DLR,  KC_HASH, _______,                          _______, _______, KC_CIRC, _______, _______,  QK_BOOT,
         _______, _______, KC_LT,   KC_EQL,  KC_GT,   KC_GRV,                           _______, KC_LBRC, KC_UNDS, KC_RBRC, _______,  _______,
         _______, KC_BSLS, KC_LPRN, KC_MINS, KC_RPRN, KC_PLUS,                          KC_PERC, KC_LCBR, KC_SCLN, KC_RCBR, KC_EXLM,  _______,
         _______, _______, KC_ASTR, KC_COLN, KC_SLSH, _______,                          _______, KC_PIPE, KC_TILD, KC_AMPR, _______,  QK_RBT,
                           _______, _______, _______, ADJUST, _______,         _______, _______, _______, _______, _______ \
        ),

      [_ADJUST] = LAYOUT(
        _______, _______, _______, _______, _______, _______,                           _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                           _______, _______, _______, _______, DM_PLY1, DM_REC1,
        RGB_TOG, RGB_HUI, RGB_SAI, RGB_VAI, _______, _______,                           _______, KC_VOLD, KC_MUTE, KC_VOLU, DM_PLY2, DM_REC2,
        RGB_MOD, RGB_HUD, RGB_SAD, RGB_VAD, _______, CG_TOGG,                           _______, KC_MPRV, KC_MPLY, KC_MNXT, _______, DM_RSTP,
                          _______, _______, _______, _______, _______,         _______, _______,  _______, _______, _______

       ),
};

#ifdef OLED_ENABLE

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
  if (!is_keyboard_master()) {
    return OLED_ROTATION_180;  // bongocat, ergohaven_dark/light
    // return OLED_ROTATION_270;  // luna
     }
    else {
    return OLED_ROTATION_270;
    }
  return rotation;
}

void render_layer_state(void) {
    // Print current mode
    oled_write_ln_P(PSTR("K:02\n"), false);
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
         // if (!isGamingMode()) {
         //     render_layer_state();
         // }
    } else {
    // render_bongocat();  // bongocat
    // render_luna_status();  // luna
        // ergohaven_light_draw();
        ergohaven_dark_draw();
}
    return false;
}

// void matrix_scan_keymap(void) {
//     if (isGamingMode()) {
//         if (countMainTimer() > 0) {
//             game_main();
//         }
//     }
// }

#endif
