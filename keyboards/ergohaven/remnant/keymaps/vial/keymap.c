#include QMK_KEYBOARD_H

#define _QWERTY 0
#define _LOWER 1
#define _RAISE 2
#define _ADJUST 3
#define _FOURTH 4
#define _FIFTH 5
#define _SIXTH 6
#define _SEVENTH 7
#define _EIGHTH 8
#define _NINTH 9
#define _TENTH 10
#define _ELEVENTH 11
#define _TWELTH 12
#define _THIRTEENTH 13
#define _FOURTEENTH 14
#define _FIFTEENTH 15
#define _SIXTEENTH 16

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
                                        KC_LALT, LOWER, KC_SPC,       KC_ENT, RAISE, KC_BSPC
      ),
    
      [_LOWER] = LAYOUT_5x6(
    
         _______, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC,              KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, _______,
         _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                 KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
         _______, KC_CAPS, KC_INS,  KC_HOME, KC_END,   KC_PSCR,             _______, _______, _______, _______, _______, _______,
         _______, KC_UNDO, KC_CUT,  KC_COPY, KC_PASTE, _______,             _______, _______, _______, _______, _______ , _______,
                           _______, _______,                                                 KC_LCBR, KC_RCBR,
                                            _______, _______, _______,    _______, ADJUST,  _______
    
      ),
    
      [_RAISE] = LAYOUT_5x6(
         _______,_______, _______, _______, _______,  _______,             _______, _______, _______, _______, _______, _______,
         _______, _______, _______, KC_UP,   _______,  _______,             _______, _______, _______, _______, _______, _______,
         _______, _______, KC_LEFT, KC_DOWN, KC_RIGHT, _______,             _______, _______, _______, _______, _______, _______,
         _______, KC_F1,   KC_F2,   KC_F3,   KC_F4,    KC_F5,               KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  _______,
                           _______, _______,                                                  KC_F11, KC_F12,
                                              _______, ADJUST, _______,   _______, _______, _______
      ),
    
      [_ADJUST] = LAYOUT_5x6(
        _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______, \
        _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______, \
        RGB_TOG, RGB_HUI, RGB_SAI, RGB_VAI, _______, _______,          _______, KC_VOLD, KC_MUTE, KC_VOLU, _______, _______, \
        RGB_MOD, RGB_HUD, RGB_SAD, RGB_VAD, _______, _______,          _______, KC_MPRV, KC_MPLY, KC_MNXT, _______, _______, \
                          _______, _______,                                     _______, _______,                            \
                                           _______, _______, _______,   _______, _______,  _______                                           \
       ),
};

//// Light LEDs 6 to 9 and 12 to 15 red when caps lock is active. Hard to ignore!
//const rgblight_segment_t PROGMEM my_layer0_layer[] = RGBLIGHT_LAYER_SEGMENTS(
//    {6, 4, HSV_RED},       // Light 4 LEDs, starting with LED 6
//    {8, 4, HSV_RED}       // Light 4 LEDs, starting with LED 12
//);
// Light LEDs 9 & 10 in cyan when keyboard layer 1 is active
const rgblight_segment_t PROGMEM my_layer1_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 58, HSV_CYAN}
);
// Light LEDs 11 & 12 in purple when keyboard layer 2 is active
const rgblight_segment_t PROGMEM my_layer2_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 58, HSV_RED}
);
// Light LEDs 13 & 14 in green when keyboard layer 3 is active
const rgblight_segment_t PROGMEM my_layer3_layer[] = RGBLIGHT_LAYER_SEGMENTS(
    {0, 58, HSV_GOLDENROD}
);

// Now define the array of layers. Later layers take precedence
const rgblight_segment_t* const PROGMEM my_rgb_layers[] = RGBLIGHT_LAYERS_LIST(
//    my_layer0_layer,
    my_layer1_layer,    // Overrides caps lock layer
    my_layer2_layer,    // Overrides other layers
    my_layer3_layer     // Overrides other layers
);

void keyboard_post_init_user(void) {
    // Enable the LED layers
    rgblight_layers = my_rgb_layers;
}
//
//
//layer_state_t default_layer_state_set_user(layer_state_t state) {
//    rgblight_set_layer_state(0, layer_state_cmp(state, _QWERTY));
//    return state;
//}

layer_state_t layer_state_set_user(layer_state_t state) {
    rgblight_set_layer_state(0, layer_state_cmp(state, _RAISE));
    rgblight_set_layer_state(1, layer_state_cmp(state, _LOWER));
    rgblight_set_layer_state(2, layer_state_cmp(state, _ADJUST));
    return state;
}
