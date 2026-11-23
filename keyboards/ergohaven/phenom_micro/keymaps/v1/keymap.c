#include QMK_KEYBOARD_H
#include "ergohaven.h"
#include "src/eh_pointing.h"
#include "vial.h"

enum phenom_micro_keycodes {
    HRM_GUI_SCR   = EH_PRINFO,
    HRM_RGUI_EXLM = EH_RSRV1,
    HRM_LALT_LPRN = EH_RSRV5,
    HRM_LCTL_RPRN,
    HRM_RCTL_LCBR,
    HRM_RALT_RCBR,
};

typedef struct {
    uint16_t keycode;
    uint16_t tap_keycode;
    uint8_t  mod;
} phenom_micro_hrm_t;

typedef struct {
    uint16_t timer;
    uint8_t  mod;
    bool     pressed;
    bool     held;
} phenom_micro_hrm_state_t;

static const phenom_micro_hrm_t phenom_micro_hrms[] = {
    {HRM_GUI_SCR,   EH_SCR,   MOD_BIT(KC_LEFT_GUI)},
    {HRM_RGUI_EXLM, KC_EXLM,  MOD_BIT(KC_RIGHT_GUI)},
    {HRM_LALT_LPRN, KC_LPRN,  MOD_BIT(KC_LEFT_ALT)},
    {HRM_LCTL_RPRN, KC_RPRN,  MOD_BIT(KC_LEFT_CTRL)},
    {HRM_RCTL_LCBR, KC_LCBR,  MOD_BIT(KC_RIGHT_CTRL)},
    {HRM_RALT_RCBR, KC_RCBR,  MOD_BIT(KC_LEFT_ALT)}, // Preserve the legacy keycode identity; hold uses Left Alt.
};

static phenom_micro_hrm_state_t phenom_micro_hrm_states[ARRAY_SIZE(phenom_micro_hrms)];

static int8_t phenom_micro_hrm_index(uint16_t keycode) {
    for (uint8_t i = 0; i < ARRAY_SIZE(phenom_micro_hrms); ++i) {
        if (phenom_micro_hrms[i].keycode == keycode) {
            return i;
        }
    }
    return -1;
}

static void phenom_micro_hrm_hold(uint8_t index) {
    phenom_micro_hrm_state_t *state = &phenom_micro_hrm_states[index];
    if (state->pressed && !state->held) {
        register_mods(state->mod);
        state->held = true;
    }
}

static void phenom_micro_hrm_hold_pending_except(uint16_t keycode) {
    for (uint8_t i = 0; i < ARRAY_SIZE(phenom_micro_hrms); ++i) {
        if (phenom_micro_hrms[i].keycode != keycode) {
            phenom_micro_hrm_hold(i);
        }
    }
}

// clang-format off
// phenom-micro-layout-v0.0.18: align Base/Lower/Raise with the published Phenom Micro layout image.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,                                             KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,
LGUI_T(KC_A), LALT_T(KC_S), LSFT_T(KC_D), LCTL_T(KC_F), KC_G,                              KC_H,     RCTL_T(KC_J), RSFT_T(KC_K), LALT_T(KC_L), RGUI_T(KC_SCLN),
KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,                                             KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_QUOT,
          KC_GRV,   KC_ESC,   KC_BSPC,  LOWER,    KC_SPC,                                 KC_ENT,   RAISE,    KC_TAB,   KC_LBRC,  KC_RBRC,
                                                            KC_MUTE,            KC_MUTE
    ),

    [_LOWER] = LAYOUT(
KC_1,     KC_2,     KC_3,     KC_4,     KC_5,                                             KC_6,     KC_7,     KC_8,     KC_9,     KC_0,
LGUI_T(KC_HOME), LALT_T(KC_INS), LSFT_T(KC_DEL), LCTL_T(KC_END), KC_ENT,                   _______,  RCTL_T(KC_LEFT), RSFT_T(KC_DOWN), LALT_T(KC_UP), RGUI_T(KC_RIGHT),
_______,  _______,  _______,  _______,  KC_PSCR,                                          _______,  _______,  _______,  _______,  _______,
          KC_PGUP,  KC_PGDN,  _______,  _______,  _______,                                _______,  ADJUST,   _______,  KC_CAPS,  CW_TOGG,
                                                            _______,            _______
    ),

    [_RAISE] = LAYOUT(
KC_AT,    KC_LT,    KC_EQL,   KC_GT,    KC_GRV,                                           KC_CIRC,  KC_DQT,   KC_UNDS,  KC_QUOT,  _______,
LGUI_T(KC_BSLS), HRM_LALT_LPRN, LSFT_T(KC_MINS), HRM_LCTL_RPRN, KC_PLUS,                  KC_PERC,  HRM_RCTL_LCBR, RSFT_T(KC_SCLN), HRM_RALT_RCBR, HRM_RGUI_EXLM,
KC_HASH,  KC_ASTR,  KC_COLN,  KC_SLSH,  _______,                                          _______,  KC_PIPE,  KC_TILD,  KC_AMPR,  KC_DLR,
          _______,  _______,  _______,  ADJUST,   _______,                                _______,  _______,  _______,  _______,  _______,
                                                            _______,            _______
    ),

    [_ADJUST] = LAYOUT(
KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,                                            KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,
KC_LGUI,  LALT_T(KC_MPRV), LSFT_T(KC_MPLY), LCTL_T(KC_MNXT), _______,                     _______,  RCTL_T(KC_VOLD), RSFT_T(KC_MUTE), LALT_T(KC_VOLU), RGUI_T(KC_F11),
_______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  KC_F12,
          _______,  _______,  _______,  _______,  _______,                                _______,  _______,  _______,  _______,  _______,
                                                            _______,            _______
    ),

    [_FOUR] = LAYOUT(
_______,  _______,  _______,  _______,  _______,                                          _______,  _______,  _______,  _______,  _______,
HRM_GUI_SCR, KC_BTN3,  KC_BTN2,  KC_BTN1,  EH_SNP,                                       EH_SNP,   KC_BTN1,  KC_BTN2,  KC_BTN3,  HRM_GUI_SCR,
_______,  _______,  _______,  _______,  EH_TXT,                                           EH_TXT,   _______,  _______,  _______,  _______,
          _______,  _______,  _______,  _______,  _______,                                _______,  _______,  _______,  _______,  _______,
                                                            _______,            _______
    ),
};
// clang-format on

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_BASE]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [_LOWER]  = {ENCODER_CCW_CW(RGB_VAD, RGB_VAI), ENCODER_CCW_CW(RGB_VAD, RGB_VAI)},
    [_RAISE]  = {ENCODER_CCW_CW(KC_LEFT, KC_RIGHT), ENCODER_CCW_CW(KC_LEFT, KC_RIGHT)},
    [_ADJUST] = {ENCODER_CCW_CW(KC_WH_D, KC_WH_U), ENCODER_CCW_CW(KC_WH_D, KC_WH_U)},
    [_FOUR]   = {ENCODER_CCW_CW(KC_WH_D, KC_WH_U), ENCODER_CCW_CW(KC_WH_D, KC_WH_U)},
};
#endif

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        phenom_micro_hrm_hold_pending_except(keycode);
    }
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    int8_t index = phenom_micro_hrm_index(keycode);
    if (index < 0) {
        return true;
    }

    phenom_micro_hrm_state_t *state = &phenom_micro_hrm_states[index];
    if (record->event.pressed) {
        state->timer   = timer_read();
        state->mod     = phenom_micro_hrms[index].mod;
        if (keycode == HRM_GUI_SCR && record->event.key.row >= MATRIX_ROWS / 2) {
            state->mod = MOD_BIT(KC_RIGHT_GUI);
        }
        state->pressed = true;
        state->held    = false;
    } else {
        if (state->held) {
            unregister_mods(state->mod);
        } else if (phenom_micro_hrms[index].tap_keycode != KC_NO) {
            vial_keycode_tap(phenom_micro_hrms[index].tap_keycode);
        }
        state->pressed = false;
        state->held    = false;
    }
    return false;
}

void matrix_scan_user(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(phenom_micro_hrms); ++i) {
        if (phenom_micro_hrm_states[i].pressed && timer_elapsed(phenom_micro_hrm_states[i].timer) >= TAPPING_TERM) {
            phenom_micro_hrm_hold(i);
        }
    }
}
