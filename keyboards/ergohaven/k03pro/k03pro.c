#include "display.h"
#include "transactions.h"
#include "ergohaven.h"
#include "src/eh_ruen.h"
#include "ergohaven_rgb.h"
#include "src/display/eh_display.h"
#include "src/eh_pointing.h"
#include QMK_KEYBOARD_H

// clang-format off
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
          'L', 'L', '*', '*', '*', 'L',   'R', '*', '*', '*', 'R', 'R'
);
// clang-format on

typedef union {
    uint8_t raw;
    struct {
        uint8_t lang : 1;
        bool    mac : 1;
        bool    caps_word : 1;
    };
} display_config_t;

display_config_t display_config;

uint8_t split_get_lang(void) {
    return is_keyboard_master() ? get_cur_lang() : display_config.lang;
}

bool split_get_mac(void) {
    return is_keyboard_master() ? keymap_config.swap_lctl_lgui : display_config.mac;
}

bool split_get_caps_word(void) {
    return is_keyboard_master() ? is_caps_word_on() : display_config.caps_word;
}

void sync_display(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&display_config, in_data, sizeof(display_config_t));
}

bool is_display_side(void) {
#ifdef K03_DISPLAY_RIGHT
    return !is_keyboard_left();
#endif
#ifdef K03_DISPLAY_LEFT
    return is_keyboard_left();
#endif
    return false;
}

bool is_touch_side(void) {
    return !is_display_side();
}

void housekeeping_task_user(void) {
    if (is_display_enabled() && is_display_side()) {
        display_housekeeping_task();
    }

    if (is_touch_side() && is_keyboard_master()) {
        static bool is_display_on = true;

        uint32_t activity_elapsed = last_input_activity_elapsed();
        if (activity_elapsed > EH_TIMEOUT) {
            if (is_display_on) {
                backlight_level_noeeprom(0);
                is_display_on = false;
            }
        } else {
            if (!is_display_on) {
                backlight_init();
                is_display_on = true;
            }
        }
    }

    if (is_touch_side() && is_keyboard_master()) {
        {
            static uint32_t         last_sync = 0;
            static display_config_t slave     = {.raw = 0};

            if (last_sync == 0 || timer_elapsed32(last_sync) > 500) {
                display_config.lang      = split_get_lang();
                display_config.mac       = split_get_mac();
                display_config.caps_word = split_get_caps_word();

                if (slave.raw != display_config.raw) {
                    if (transaction_rpc_send(RPC_SYNC_DISPLAY, sizeof(display_config_t), &display_config)) {
                        slave.raw = display_config.raw;
                        dprintf("sync display settings %x\n", display_config.raw);
                    }
                    last_sync = timer_read32();
                }
            }
        }
    }
}

void keyboard_post_init_user(void) {
    if (is_display_side()) {
        display_init_kb();
    }

    set_led_blinks(false);
    transaction_register_rpc(RPC_SYNC_DISPLAY, sync_display);
}
