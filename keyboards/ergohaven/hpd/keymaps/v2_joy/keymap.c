#include QMK_KEYBOARD_H
#include "ergohaven.h"
#include "ergohaven_pointing.h"

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
        [_BASE] = LAYOUT(
         KC_GRV,   KC_1,   KC_2,    KC_3,     KC_4,     KC_5,                                              KC_6,  KC_7,     KC_8,     KC_9,   KC_0,    KC_BSPC,
         KC_ESC,   KC_Q,   KC_W,    KC_E,     KC_R,     KC_T,                                              KC_Y,  KC_U,     KC_I,     KC_O,   KC_P,    KC_BSLS,
         KC_TAB,   KC_A,   KC_S,    KC_D,     KC_F,     KC_G,                                              KC_H,  KC_J,     KC_K,     KC_L,   KC_SCLN, KC_QUOT,
         KC_LSFT,  KC_Z,   KC_X,    KC_C,     KC_V,     KC_B,                                              KC_N,  KC_M,     KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,
                           KC_PGDN, KC_PGUP,  KC_LCTL,  LOWER, KC_LALT, KC_SPACE,         KC_ENT, KC_RGUI, RAISE, KC_RCTL,  KC_LBRC,  KC_RBRC,
                                                                        KC_MUTE

        ),

        [_LOWER] = LAYOUT(
         KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,    KC_F6,                                              KC_F7,   KC_F8,   KC_F9,   KC_F10, KC_F11,  KC_F12,
         _______, _______, KC_HOME, KC_UP,   KC_END,   KC_INS,                                             _______, KC_7,    KC_8,    KC_9,   _______, _______,
         _______, _______, KC_LEFT, KC_DOWN, KC_RIGHT, KC_ENT,                                             _______, KC_4,    KC_5,    KC_6,   _______, _______,
         _______, _______, PREVWRD, KC_CAPS, NEXTWRD,  KC_DEL,                                             KC_PSCR, KC_1,    KC_2,    KC_3,   _______, _______,
                           _______, _______, _______, _______, _______, _______,         _______, _______, ADJUST, _______, KC_0, _______,
                                                                        _______
        ),

        [_RAISE] = LAYOUT(
         _______, _______, KC_AT,   KC_DLR,  KC_HASH, _______,                                             _______, _______, KC_CIRC, _______, _______,  _______,
         _______, _______, KC_LT,   KC_EQL,  KC_GT,   KC_GRV,                                              _______, KC_LBRC, KC_UNDS, KC_RBRC, _______,  _______,
         _______, KC_BSLS, KC_LPRN, KC_MINS, KC_RPRN, KC_PLUS,                                             KC_PERC, KC_LCBR, KC_SCLN, KC_RCBR, KC_EXLM,  _______,
         _______, _______, KC_ASTR, KC_COLN, KC_SLSH, _______,                                             _______, KC_PIPE, KC_TILD, KC_AMPR, _______,  _______,
                           _______, _______, _______, ADJUST, _______, _______,         _______, _______, _______, _______, _______, _______,
                                                                       _______
        ),

      [_ADJUST] = LAYOUT(
        QK_BOOT, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, DM_PLY1, DM_REC1,
        _______, _______, _______, _______, _______, _______,                                              _______, KC_VOLD, KC_MUTE, KC_VOLU, DM_PLY2, DM_REC2,
        _______, _______, _______, _______, _______, _______,                                              _______, KC_MPRV, KC_MPLY, KC_MNXT, _______, DM_RSTP,
                          _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______,
                                                                       _______
       ),

      [_FOUR] = LAYOUT(
        _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______, _______,
        _______, EH_SCR,  KC_BTN3, KC_BTN2, KC_BTN1, EH_SNP,                                               _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, EH_TXT,                                               _______, _______, _______, _______, _______, _______,
                          _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______,
                                                                       _______
       ),
};
// clang-format on

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [1] = {ENCODER_CCW_CW(_______, _______)},
    [2] = {ENCODER_CCW_CW(_______, _______)},
    [3] = {ENCODER_CCW_CW(_______, _______)},
    [4] = {ENCODER_CCW_CW(_______, _______)},
};
#endif

typedef union {
    uint32_t raw;
    struct {
        uint8_t text_mode : 3;
        uint8_t scroll_mode : 3;
        uint8_t sniper_mode : 2;
        uint8_t dpi_mode : 4;
        uint8_t auto_mouse_layer : 4;
    };
} vial_config_t;

static vial_config_t vial_config;

const int     DPI_TABLE[15]    = {100, 200, 300, 400, 500, 600, 800, 1000, 1200, 1600, 2000, 2500, 3200, 4000, 5000};
const int32_t SNIPER_TABLE[15] = {2, 3, 4, 5};
const int32_t SCROLL_TABLE[15] = {6, 8, 11, 16, 23, 32, 45, 64};
const int32_t TEXT_TABLE[15]   = {6, 8, 11, 16, 23, 32, 45, 64};

int get_dpi(uint8_t dpi_mode) {
    if (dpi_mode < ARRAY_SIZE(DPI_TABLE))
        return DPI_TABLE[dpi_mode];
    else
        return DPI_TABLE[0];
}

void via_set_layout_options_kb(uint32_t value) {
    dprintf("via_set_layout_options_kb %lx\n", value);
    vial_config.raw = value;
    pointing_device_set_cpi(get_dpi(vial_config.dpi_mode));
    set_scroll_sens(SCROLL_TABLE[vial_config.scroll_mode]);
    set_sniper_sens(SNIPER_TABLE[vial_config.sniper_mode]);
    set_text_sens(TEXT_TABLE[vial_config.text_mode]);
    set_automouse(vial_config.auto_mouse_layer);
}

void keyboard_post_init_user(void) {
    vial_config.raw = via_get_layout_options();
    via_set_layout_options_kb(vial_config.raw);
    set_led_blinks(false);
}
