#include "ergohaven_ruen.h"
#include "hid.h"

static uint8_t cur_lang = LANG_EN;

static uint8_t stored_lang = LANG_EN;

static bool should_revert_macro = false;

static uint8_t tg_mode = TG_DEFAULT;

static uint32_t revert_time = 0;

static bool should_revert_ru = false;

static bool english_word = false;

static bool mac_layout = false;

void set_lang(uint8_t lang) {
    uint8_t mods = get_mods();
    switch (tg_mode) {
        case TG_DEFAULT:
            if (cur_lang == lang) return;
            if (mods != 0) del_mods(mods);
            if (keymap_config.swap_lctl_lgui) {
                register_code(KC_LCTL);
                tap_code(KC_SPACE);
                wait_ms(50);
                unregister_code(KC_LCTL);
                wait_ms(50);
            } else {
                register_code(KC_LGUI);
                tap_code(KC_SPACE);
                wait_ms(50);
                unregister_code(KC_LGUI);
                wait_ms(50);
            }
            if (mods != 0) add_mods(mods);
            break;
        case TG_M0:
            if (cur_lang == lang) return;
            if (mods != 0) del_mods(mods);
            dynamic_keymap_macro_send(QK_MACRO_0 - QK_MACRO);
            if (mods != 0) add_mods(mods);
            break;
        case TG_M1M2:
            if (lang == LANG_EN) {
                if (!should_revert_ru) {
                    if (mods != 0) del_mods(mods);
                    dynamic_keymap_macro_send(QK_MACRO_1 - QK_MACRO);
                    if (mods != 0) add_mods(mods);
                }
            } else {
                if (mods != 0) del_mods(mods);
                dynamic_keymap_macro_send(QK_MACRO_2 - QK_MACRO);
                if (mods != 0) add_mods(mods);
            }
            break;
        default:
            break;
    }
    set_cur_lang(lang);
}

void set_cur_lang(uint8_t lang) {
    if (cur_lang != lang) {
        cur_lang = lang;
        on_change_lang_cb(cur_lang);
    }
}

void set_ruen_toggle_mode(uint8_t mode) {
    switch (mode) {
        default:
        case TG_DEFAULT:
            tg_mode = TG_DEFAULT;
            break;

        case TG_M1M2:
            tg_mode = TG_M1M2;
            break;

        case TG_M0:
            tg_mode = TG_M0;
            break;
    }
}

uint8_t get_ruen_toggle_mode(void) {
    return tg_mode;
}

void set_ruen_mac_layout(bool layout) {
    mac_layout = layout;
}

bool get_ruen_mac_layout(void) {
    return mac_layout;
}

void lang_toggle(void) {
    set_lang((cur_lang == LANG_EN) ? LANG_RU : LANG_EN);
}

void lang_sync(void) {
    set_cur_lang((cur_lang == LANG_EN) ? LANG_RU : LANG_EN);
}

uint8_t get_cur_lang(void) {
    return cur_lang;
}

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

bool pre_process_record_ruen(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;

    switch (keycode) {
        case KC_A ... KC_Z:
        case S(KC_A)... S(KC_Z):
        case KC_LBRC ... KC_RBRC:
        case S(KC_LBRC)... S(KC_RBRC):
        case KC_SCLN ... KC_SLSH: // KC_QUOT KC_GRAVE KC_COMMA KC_DOT
        case S(KC_SCLN)... S(KC_SLSH):
            if (should_revert_ru) {
                should_revert_ru = false;
                set_lang(LANG_RU);
            }
            break;
        case LG_SET_EN:
        case LG_TOGGLE:
            if (should_revert_ru) {
                should_revert_ru = false;
                set_lang(LANG_EN);
            }
            break;
        case QK_UNICODE ... QK_UNICODE_MAX: {
            uint8_t lang = cur_lang;
            set_lang(LANG_EN);
            should_revert_ru = should_revert_ru || (cur_lang != lang);
            revert_time      = timer_read32();
            break;
        }
    }

    if (english_word) {
        switch (keycode & 0xFF) {
            case KC_SPACE:
            case KC_ENTER:
            case KC_ESCAPE:
            case KC_MINUS:
                english_word = false;
                caps_word_off();
                set_lang(LANG_RU);
                break;
            default:
                break;
        }
    }

    return true;
}

bool process_russian_letter(uint8_t keycode) {
    if (cur_lang == LANG_RU) {
        if (is_caps_word_on()) {
            add_weak_mods(MOD_BIT(KC_LSFT));
        }
        tap_code(keycode);
    }
    return false;
}

bool process_record_ruen(uint16_t keycode, keyrecord_t *record) {
    if (!(LG_START <= keycode && keycode < LG_END)) return true;

    if (keycode == LG_MOD) {
        static uint16_t press_timer = 0;
        if (record->event.pressed) {
            lang_toggle();
            press_timer = timer_read();
        } else {
            if (timer_elapsed(press_timer) >= get_tapping_term(keycode, record)) lang_toggle();
        }
        return false;
    }

    if (!record->event.pressed) return false;

    switch (keycode) {
        case LG_TOGGLE:
            lang_toggle();
            return false;

        case LG_SYNC:
            lang_sync();
            return false;

        case LG_SET_EN:
            set_lang(LANG_EN);
            return false;

        case LG_SET_RU:
            set_lang(LANG_RU);
            return false;

        case LG_SET_M0:
            tg_mode = TG_M0;
            kb_config_update_ruen_toggle_mode(tg_mode);
            return false;

        case LG_SET_M1M2:
            tg_mode = TG_M1M2;
            kb_config_update_ruen_toggle_mode(tg_mode);
            return false;

        case LG_SET_DFLT:
            tg_mode = TG_DEFAULT;
            kb_config_update_ruen_toggle_mode(tg_mode);
            return false;

        case LG_DOT: // .
            tap_code16(cur_lang == LANG_EN ? KC_DOT : mac_layout ? S(KC_7) : KC_SLASH);
            return false;

        case LG_COMMA: // ,
            tap_code16(cur_lang == LANG_EN ? KC_COMMA : mac_layout ? S(KC_6) : S(KC_SLASH));
            return false;

        case LG_SCLN: // ;
            tap_code16(cur_lang == LANG_EN ? KC_SCLN : mac_layout ? S(KC_8) : S(KC_4));
            return false;

        case LG_COLON: // :
            tap_code16(cur_lang == LANG_EN ? KC_COLON : mac_layout ? S(KC_5) : S(KC_6));
            return false;

        case LG_DQUO: // "
            tap_code16(cur_lang == LANG_EN ? KC_DQUO : S(KC_2));
            return false;

        case LG_QUES: // ?
            tap_code16(cur_lang == LANG_EN || mac_layout ? KC_QUES : S(KC_7));
            return false;

        case LG_SLASH: // /
            tap_code16(cur_lang == LANG_EN || mac_layout ? KC_SLASH : LSFT(KC_BSLS));
            return false;

        case LG_PERC: // %
            tap_code16(cur_lang == LANG_RU && mac_layout ? LSFT(KC_4) : LSFT(KC_5));
            return false;

        case LG_TG_MAC:
            mac_layout = !mac_layout;
            kb_config_update_ruen_mac_layout(mac_layout);
            return false;

        case LG_EN_START ... LG_QUOTE: {
            uint8_t lang = cur_lang;
            set_lang(LANG_EN);
            tap_code16(en_table[keycode - LG_EN_START]);
            should_revert_ru = should_revert_ru || (cur_lang != lang);
            revert_time      = timer_read32();
            return false;
        }

        case LG_RU_BE:
            return process_russian_letter(KC_COMMA);
        case LG_RU_YU:
            return process_russian_letter(KC_DOT);
        case LG_RU_ZHE:
            return process_russian_letter(KC_SEMICOLON);
        case LG_RU_E:
            return process_russian_letter(KC_QUOT);
        case LG_RU_HRD_SGN:
            return process_russian_letter(KC_RBRC);
        case LG_RU_KHA:
            return process_russian_letter(KC_LBRC);
            return false;
        case LG_RU_YO:
            return process_russian_letter(KC_GRAVE);
            return false;

        case LG_NUM: {
            uint8_t lang = cur_lang;
            set_lang(LANG_RU);
            tap_code16(LSFT(KC_3));
            set_lang(lang);
            return false;
        }

        case LG_WORD: {
            if (cur_lang == LANG_RU && !english_word) {
                english_word = true;
                bool shift   = (get_mods() | get_oneshot_mods() | get_weak_mods()) & MOD_MASK_SHIFT;
                if (get_oneshot_mods() & MOD_MASK_SHIFT) clear_oneshot_mods();
                set_lang(LANG_EN);
                if (shift) caps_word_on();
            }
            return false;
        }

        case LG_STORE:
            stored_lang         = cur_lang;
            should_revert_macro = true;
            return false;

        case LG_REVERT:
            set_lang(stored_lang);
            should_revert_macro = false;
            revert_time         = timer_read32();
            return false;
    }

    return true;
}

void housekeeping_task_ruen(void) {
    if (timer_elapsed32(revert_time) < 500) return;

    if (should_revert_ru) {
        should_revert_ru = false;
        set_lang(LANG_RU);
        return;
    }

    if (should_revert_macro) return;

    hid_data_t *hid_data = get_hid_data();
    if (hid_data->layout_changed) {
        if (hid_data->layout == LANG_EN)
            cur_lang = LANG_EN;
        else
            cur_lang = LANG_RU;
        hid_data->layout_changed = false;
    }
}

__attribute__((weak)) void on_change_lang_cb(uint8_t lang) {
}
