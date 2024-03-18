#include "lang_ru_en.h"

static uint8_t cur_lang = LANG_EN;

static uint8_t tg_mode = TG_SFT_ALT;

void set_lang(uint8_t lang) {
    if (cur_lang == lang) return;

    switch (lang) {
        case TG_MACRO30:
            dynamic_keymap_macro_send(QK_MACRO_30 - QK_MACRO);
            break;
        case TG_MACRO31:
            dynamic_keymap_macro_send(QK_MACRO_31 - QK_MACRO);
            break;
        case TG_SFT_ALT:
            register_code(KC_LALT);
            register_code(KC_LSFT);
            unregister_code(KC_LSFT);
            unregister_code(KC_LALT);
        default:
            break;
    }

    cur_lang = lang;
}

void lang_toggle(void) {
    if (cur_lang == LANG_EN)
        set_lang(LANG_RU);
    else
        set_lang(LANG_EN);
}

void lang_sync(void) {
    if (cur_lang == LANG_EN)
        cur_lang = LANG_RU;
    else
        cur_lang = LANG_EN;
}

void lang_sync_external(uint8_t lang) {
    if (lang == LANG_EN)
        cur_lang = LANG_EN;
    else
        cur_lang = LANG_RU;
}

uint8_t get_cur_lang(void) {
    return cur_lang;
}

typedef struct {
    uint16_t en;
    uint16_t ru;
} ru_en_symbol;

ru_en_symbol ru_en_table[] = {
    {KC_DOT, KC_SLASH},         // LG_DOT
    {KC_COMMA, LSFT(KC_SLASH)}, // LG_COMMA
    {KC_SCLN, LSFT(KC_4)},      // LG_SCLN
    {KC_COLON, LSFT(KC_6)},     // LG_COLON
    {KC_DQUO, LSFT(KC_2)},      // LG_DQUO
    {KC_QUES, LSFT(KC_7)},      // LG_QUES
    {KC_SLASH, LSFT(KC_BSLS)},  // LG_SLASH
};

uint16_t en_table[] = {
    KC_LBRC,  // LG_LBR
    KC_RBRC,  // LG_RBR
    KC_LCBR,  // LG_LCBR
    KC_RCBR,  // LG_RCBR
    KC_LT,    // LG_LT
    KC_GT,    // LG_GT
    KC_GRAVE, // LG_GRAVE
    KC_TILD,  // LG_TILD
    KC_AT,    // LG_AT
    KC_HASH,  // LG_HASH
    KC_DLR,   // LG_DLR
    KC_CIRC,  // LG_CIRC
    KC_AMPR,  // LG_AMPR
    KC_PIPE,  // LG_PIPE
    KC_QUOT,  // LG_QUOTE
};

bool process_record_lang(uint16_t keycode, keyrecord_t* record) {
    switch (keycode) {
        case LG_TOGGLE:
            if (record->event.pressed) lang_toggle();
            return false;

        case LG_SYNC:
            if (record->event.pressed) lang_sync();
            return false;

        case LG_SET_EN:
            if (record->event.pressed) set_lang(LANG_EN);
            return false;

        case LG_SET_RU:
            if (record->event.pressed) set_lang(LANG_RU);
            return false;

        case LG_SET_M30:
            tg_mode = TG_MACRO30;
            return false;

        case LG_SET_M31:
            tg_mode = TG_MACRO31;
            return false;

        case LG_SET_SFT_ALT:
            tg_mode = TG_SFT_ALT;
            return false;
    }

    if (LG_RU_EN_START <= keycode && keycode < LG_EN_START) {
        if (record->event.pressed) {
            if (cur_lang == 0)
                tap_code16(ru_en_table[keycode - LG_RU_EN_START].en);
            else
                tap_code16(ru_en_table[keycode - LG_RU_EN_START].ru);
        }
        return false;
    }

    if (LG_EN_START <= keycode && keycode < LG_END) {
        if (record->event.pressed) {
            if (record->event.pressed) {
                uint8_t lang = cur_lang;
                set_lang(LANG_EN);
                tap_code16(en_table[keycode - LG_EN_START]);
                set_lang(lang);
            }
        }
        return false;
    }

    return true;
}
