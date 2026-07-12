#include "ergohaven.h"
#include "src/eh_settings.h"
#include "src/eh_ruen.h"
#include "src/oled/eh_oled.h"
#include "ergohaven_rgb.h"
#ifdef EH_SYNC_LED_COLORS
#    include "transactions.h"
#endif
#include "src/display/eh_display.h"
#include "src/eh_pointing.h"
#include "hid.h"
#include "version.h"


#ifdef AUDIO_ENABLE
float base_sound[][2] = SONG(TERMINAL_SOUND);
float caps_sound[][2] = SONG(CAPS_LOCK_ON_SOUND);
#endif

bool     is_alt_tab_active = false;
uint16_t alt_tab_timer     = 0;

#ifdef EH_SYNC_LED_COLORS
static kb_settings_led_colors_t synced_led_colors;
static bool synced_led_colors_valid = false;

static void sync_led_colors_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    (void)out_len;
    (void)out_data;
    if (in_len == sizeof(kb_settings_led_colors_t) && in_data != NULL) {
        kb_settings_led_colors_t value;
        memcpy(&value, in_data, sizeof(value));
        set_settings_led_colors(value);
    }
}
#endif

#ifdef EH_SYNC_POINTING_SETTINGS
static kb_settings_pointing_t synced_pointing_settings;
static bool synced_pointing_settings_valid = false;
static kb_settings_split_pointing_t synced_split_pointing_settings;
static bool synced_split_pointing_settings_valid = false;

static void sync_pointing_settings_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    (void)out_len;
    (void)out_data;
    if (in_len == sizeof(kb_settings_pointing_t) && in_data != NULL) {
        kb_settings_pointing_t value;
        memcpy(&value, in_data, sizeof(value));
        set_settings_pointing(value);
    }
}

static void sync_split_pointing_settings_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    (void)out_len;
    (void)out_data;
    if (in_len == sizeof(kb_settings_split_pointing_t) && in_data != NULL) {
        kb_settings_split_pointing_t value;
        memcpy(&value, in_data, sizeof(value));
        set_split_pointing_settings(value);
    }
}
#endif

#if defined(SPLIT_KEYBOARD) && defined(VIAL_ENABLE) && defined(VIAL_UNLOCK_COMBO_ROWS) && defined(VIAL_UNLOCK_COMBO_COLS)
static const uint8_t ergohaven_vial_unlock_combo_rows[] = VIAL_UNLOCK_COMBO_ROWS;
static const uint8_t ergohaven_vial_unlock_combo_cols[] = VIAL_UNLOCK_COMBO_COLS;
#define ERGOHAVEN_VIAL_UNLOCK_NUM_KEYS ((uint8_t)(sizeof(ergohaven_vial_unlock_combo_rows) / sizeof(ergohaven_vial_unlock_combo_rows[0])))

static uint8_t ergohaven_vial_unlock_row_for_current_side(uint8_t row) {
    if (is_keyboard_left()) {
        return row;
    }
    return row + MATRIX_ROWS / 2;
}

bool vial_unlock_combo_active(void) {
    bool holding = true;
    for (uint8_t i = 0; i < ERGOHAVEN_VIAL_UNLOCK_NUM_KEYS; ++i) {
        if (!matrix_is_on(ergohaven_vial_unlock_row_for_current_side(ergohaven_vial_unlock_combo_rows[i]), ergohaven_vial_unlock_combo_cols[i])) {
            holding = false;
            break;
        }
    }
    return holding;
}

void vial_get_unlock_combo_coords(uint8_t *rows, uint8_t *cols, size_t count) {
    if (count < ERGOHAVEN_VIAL_UNLOCK_NUM_KEYS) {
        return;
    }

    for (uint8_t i = 0; i < ERGOHAVEN_VIAL_UNLOCK_NUM_KEYS; ++i) {
        rows[i] = ergohaven_vial_unlock_row_for_current_side(ergohaven_vial_unlock_combo_rows[i]);
        cols[i] = ergohaven_vial_unlock_combo_cols[i];
    }
}
#endif

bool pre_process_record_kb(uint16_t keycode, keyrecord_t* record) {
    return pre_process_record_ruen(keycode, record) && pre_process_record_user(keycode, record);
}

bool process_record_kb(uint16_t keycode, keyrecord_t* record) {
    // Some Vial keymaps need to consume keyboard-range user slots before shared
    // Ergohaven handlers see them.
#ifdef EH_PROCESS_RECORD_USER_FIRST
    if (!process_record_user(keycode, record)) {
        return false;
    }
#endif

    // #ifdef WPM_ENABLE
    //   if (record->event.pressed) {
    //       extern uint32_t tap_timer;
    //       tap_timer = timer_read32();
    //   }
    // #endif

    switch (keycode) { // This will do most of the grunt work with the keycodes.
        case WRD_NXT:
            if (record->event.pressed) {
                register_code16(keymap_config.swap_lctl_lgui ? A(KC_RIGHT) : C(KC_RIGHT));
            } else
                unregister_code16(keymap_config.swap_lctl_lgui ? A(KC_RIGHT) : C(KC_RIGHT));
            return false;

        case WRD_PRV:
            if (record->event.pressed) {
                register_code16(keymap_config.swap_lctl_lgui ? A(KC_LEFT) : C(KC_LEFT));
            } else
                unregister_code16(keymap_config.swap_lctl_lgui ? A(KC_LEFT) : C(KC_LEFT));
            return false;

        case WNEXT:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    register_code(keymap_config.swap_lctl_lgui ? KC_LGUI : KC_LALT);
                }
                alt_tab_timer = timer_read();
                register_code(KC_TAB);
            } else {
                unregister_code(KC_TAB);
            }
            return false;

        case WPREV:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    register_code(keymap_config.swap_lctl_lgui ? KC_LGUI : KC_LALT);
                }
                alt_tab_timer = timer_read();
                register_code16(S(KC_TAB));
            } else {
                unregister_code16(S(KC_TAB));
            }
            return false;

        case KC_CAPS:
            if (record->event.pressed) {
#ifdef AUDIO_ENABLE
                PLAY_SONG(caps_sound);
#endif
            }
            return true; // Let QMK send the enter press/release events

        case LAYER_NEXT:
        case LAYER_PREV:
            // Our logic will happen on presses, nothing is done on releases
            if (!record->event.pressed) {
                // We've already handled the keycode (doing nothing), let QMK know so no further code is run unnecessarily
                return false;
            }

            int current_layer = get_highest_layer(layer_state);

            // Check if we are within the range, if not quit
            if (current_layer > LAYER_CYCLE_END || current_layer < LAYER_CYCLE_START) {
                return false;
            }

            int next_layer = keycode == LAYER_NEXT ? current_layer + 1 : current_layer - 1;
            if (next_layer > LAYER_CYCLE_END) {
                next_layer = LAYER_CYCLE_START;
            } else if (next_layer < LAYER_CYCLE_START) {
                next_layer = LAYER_CYCLE_END;
            }

            layer_move(next_layer);
            return false;

        case EH_PRINFO: {
            if (record->event.pressed) {
                send_string("Product: " PRODUCT "\n");
                send_string("FW version: " EH_VERSION_STR "\n");
                send_string("Build date: " QMK_BUILDDATE "\n");
                send_string("Git hash: " QMK_GIT_HASH "\n");

                send_string("Mac mode: ");
                send_string(keymap_config.swap_lctl_lgui ? "on\n" : "off\n");

                send_string("Unicode mode: ");
                uint8_t uc_input_mode = get_unicode_input_mode();
                switch (uc_input_mode) {
                    case UNICODE_MODE_MACOS:
                        send_string("Mac\n");
                        break;
                    case UNICODE_MODE_LINUX:
                        send_string("Linux\n");
                        break;
                    case UNICODE_MODE_WINDOWS:
                        send_string("Windows\n");
                        break;
                    case UNICODE_MODE_WINCOMPOSE:
                        send_string("WinCompose\n");
                        break;
                    default:
                        send_string("error\n");
                        break;
                }

                send_string("RuEn mode: ");
                uint8_t ruen_mode = get_ruen_toggle_mode();
                if (ruen_mode == TG_DEFAULT)
                    send_string("default\n");
                else if (ruen_mode == TG_M0)
                    send_string("M0\n");
                else if (ruen_mode == TG_M1M2)
                    send_string("M1M2\n");
                else
                    send_string("error\n");

                send_string("RuEn layout: ");
                send_string(get_ruen_mac_layout() ? "Mac\n" : "PC\n");

                send_string("Led blinks: ");
                send_string(get_led_blinks() ? "enabled\n" : "disabled\n");
            }
            return false;
        }
    }

    if (!process_record_ruen(keycode, record)) return false;

    if (!process_record_pointing(keycode, record)) return false;

#ifdef EH_PROCESS_RECORD_USER_FIRST
    return true;
#else
    return process_record_user(keycode, record);
#endif
}

bool caps_word_press_user(uint16_t keycode) {
    switch (keycode) {
        // Keycodes for russian symbols
        case KC_SCLN:
        case KC_QUOT:
        case KC_LBRC:
        case KC_RBRC:
        case KC_GRAVE:
        case KC_COMMA:
        case KC_DOT:
            if (get_cur_lang() == LANG_RU) {
                add_weak_mods(MOD_BIT(KC_LSFT));
                return true;
            } else
                return false;

        // Keycodes that continue Caps Word, with shift applied.
        case KC_A ... KC_Z:
        case KC_MINS:
        // For some reason weak mode doesn't work on this keycodes
        // so we additionaly add weak mode in process_russian_letter(...)
        case LG_RU_BE:
        case LG_RU_YU:
        case LG_RU_ZHE:
        case LG_RU_E:
        case LG_RU_HRD_SGN:
        case LG_RU_KHA:
        case LG_RU_YO:
            add_weak_mods(MOD_BIT(KC_LSFT)); // Apply shift to next key.
            return true;

        // Keycodes that continue Caps Word, without shifting.
        case KC_1 ... KC_0:
        case KC_BSPC:
        case KC_DEL:
        case KC_UNDS:
            return true;

        default:
            return false; // Deactivate Caps Word.
    }
}

void matrix_scan_kb(void) { // The very important timer.
    if (is_alt_tab_active) {
        if (timer_elapsed(alt_tab_timer) > 650) {
            unregister_code(keymap_config.swap_lctl_lgui ? KC_LGUI : KC_LALT);
            is_alt_tab_active = false;
        }
    }

    matrix_scan_user();
}

void keyboard_post_init_kb(void) {
#ifdef CONSOLE_ENABLE
    debug_enable = true;
#endif

    kb_settings_init();

#ifdef RGBLIGHT_ENABLE
    keyboard_post_init_rgb();
#endif
#ifdef EH_SYNC_LED_COLORS
    transaction_register_rpc(RPC_SYNC_LED_COLORS, sync_led_colors_rpc);
    synced_led_colors_valid = false;
#endif
#ifdef EH_SYNC_POINTING_SETTINGS
    transaction_register_rpc(RPC_SYNC_POINTING_SETTINGS, sync_pointing_settings_rpc);
    transaction_register_rpc(RPC_SYNC_SPLIT_POINTING_SETTINGS, sync_split_pointing_settings_rpc);
    synced_pointing_settings_valid = false;
    synced_split_pointing_settings_valid      = false;
#endif
    keyboard_post_init_hid();
    keyboard_post_init_user();
}

layer_state_t default_layer_state_set_kb(layer_state_t state) {
    state = default_layer_state_set_user(state);
#ifdef RGBLIGHT_ENABLE
    layer_state_set_rgb(layer_state | state);
#endif
    return state;
}

layer_state_t layer_state_set_kb(layer_state_t state) {
    if (is_alt_tab_active) {
        unregister_code(keymap_config.swap_lctl_lgui ? KC_LGUI : KC_LALT);
        is_alt_tab_active = false;
    }
    state = layer_state_set_user(state);
#ifdef RGBLIGHT_ENABLE
    layer_state_set_rgb(state | default_layer_state);
#endif
    return state;
}

void housekeeping_task_kb(void) {
#ifdef CONSOLE_ENABLE
    {
        static uint32_t t0 = 0;
        uint32_t        dt = timer_elapsed32(t0);
        if (t0 == 0) dt = 0;
        t0 = timer_read32();

        static uint32_t last_print = 0;
        static uint32_t max_dt     = 0;
        static uint32_t hz         = 0;

        max_dt = MAX(max_dt, dt);
        hz += 1;
        if (last_print == 0 || timer_elapsed32(last_print) > 1000) {
            dprintf("hz=%ld max_dt=%ld \n", hz, max_dt);
            max_dt     = 0;
            hz         = 0;
            last_print = timer_read32();
        }
    }
#endif

    uint32_t activity_elapsed = last_input_activity_elapsed();

    uint32_t led_timeout_ms = get_led_rgb_timeout_ms();
    if (led_timeout_ms > 0 && activity_elapsed > led_timeout_ms) {
#ifdef RGBLIGHT_ENABLE
        rgb_off();
#endif
    } else {
#ifdef RGBLIGHT_ENABLE
        rgb_on();
#endif
    }

#ifdef EH_SYNC_LED_COLORS
    {
        static uint32_t last_led_sync = 0;
        if (is_keyboard_master() && timer_elapsed32(last_led_sync) >= 100) {
            last_led_sync = timer_read32();
            kb_settings_led_colors_t led_colors = get_settings_led_colors();
            if (!synced_led_colors_valid || memcmp(&synced_led_colors, &led_colors, sizeof(led_colors)) != 0) {
                transaction_rpc_send(RPC_SYNC_LED_COLORS, sizeof(led_colors), &led_colors);
                synced_led_colors = led_colors;
                synced_led_colors_valid = true;
            }
        }
    }
#endif
#ifdef EH_SYNC_POINTING_SETTINGS
    {
        static uint32_t last_pointing_sync = 0;
        if (is_keyboard_master() && timer_elapsed32(last_pointing_sync) >= 100) {
            last_pointing_sync = timer_read32();

            kb_settings_pointing_t pointing_settings = get_settings_pointing();
            if (!synced_pointing_settings_valid || memcmp(&synced_pointing_settings, &pointing_settings, sizeof(pointing_settings)) != 0) {
                transaction_rpc_send(RPC_SYNC_POINTING_SETTINGS, sizeof(pointing_settings), &pointing_settings);
                synced_pointing_settings = pointing_settings;
                synced_pointing_settings_valid = true;
            }

            kb_settings_split_pointing_t phenom_devices = get_split_pointing_settings();
            if (!synced_split_pointing_settings_valid || memcmp(&synced_split_pointing_settings, &phenom_devices, sizeof(phenom_devices)) != 0) {
                transaction_rpc_send(RPC_SYNC_SPLIT_POINTING_SETTINGS, sizeof(phenom_devices), &phenom_devices);
                synced_split_pointing_settings = phenom_devices;
                synced_split_pointing_settings_valid = true;
            }
        }
    }
#endif

#if defined(OLED_ENABLE) && defined(SPLIT_KEYBOARD)
    housekeeping_task_split_oled();
#endif
    housekeeping_task_ruen();
    housekeeping_task_user();
}

void suspend_power_down_kb(void) {
#ifdef EH_HAS_DISPLAY
    display_turn_off();
#endif
#ifdef RGBLIGHT_ENABLE
    rgb_off();
#endif
#ifdef OLED_ENABLE
    oled_off();
#endif
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
#ifdef EH_HAS_DISPLAY
    display_turn_on();
#endif
#ifdef RGBLIGHT_ENABLE
    rgb_on();
#endif
#ifdef OLED_ENABLE
    oled_on();
#endif
    suspend_wakeup_init_user();
}

uint8_t get_current_layer(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

__attribute__((weak)) uint8_t split_get_lang(void) {
    return get_cur_lang();
}

__attribute__((weak)) bool split_get_mac(void) {
    return keymap_config.swap_lctl_lgui;
}

__attribute__((weak)) bool split_get_caps_word(void) {
    return is_caps_word_on();
}
