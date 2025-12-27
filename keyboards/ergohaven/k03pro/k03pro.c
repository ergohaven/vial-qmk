#include "display.h"
#include "transactions.h"
#include "ergohaven.h"
#include "ergohaven_ruen.h"
#include "ergohaven_rgb.h"
#include "ergohaven_display.h"
#include "ergohaven_pointing.h"
#include QMK_KEYBOARD_H

const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
          'L', 'L', '*', '*', '*', 'L',   'R', '*', '*', '*', 'R', 'R'
);


typedef union {
    uint32_t raw;
    struct {
        uint8_t text_mode : 3;
        uint8_t scroll_mode : 3;
        uint8_t sniper_mode : 2;
        uint8_t dpi_mode : 3;
        bool    invert_scroll : 1;
    };
} vial_config_t;

vial_config_t vial_config;

typedef union {
    uint8_t raw;
    struct {
        uint8_t lang : 1;
        bool    mac : 1;
        bool    caps_word : 1;
    };
} display_config_t;

display_config_t display_config;

typedef union {
    uint16_t raw;
    struct {
        uint16_t dpi;
    };
} touch_config_t;

touch_config_t touch_config;

uint8_t split_get_lang(void) {
    return is_keyboard_master() ? get_cur_lang() : display_config.lang;
}

bool split_get_mac(void) {
    return is_keyboard_master() ? keymap_config.swap_lctl_lgui : display_config.mac;
}

bool split_get_caps_word(void) {
    return is_keyboard_master() ? is_caps_word_on() : display_config.caps_word;
}

static const uint16_t DPI_TABLE[]      = {320, 400, 500, 630, 800, 1000};
const int32_t         SNIPER_TABLE[15] = {2, 3, 4, 5};
const int32_t         SCROLL_TABLE[15] = {6, 8, 11, 16, 23, 32, 45, 64};
const int32_t         TEXT_TABLE[15]   = {6, 8, 11, 16, 23, 32, 45, 64};

uint16_t get_dpi(uint8_t dpi_mode) {
    if (dpi_mode < ARRAY_SIZE(DPI_TABLE))
        return DPI_TABLE[dpi_mode];
    else
        return DPI_TABLE[0];
}

void via_set_layout_options_kb(uint32_t value) {
    dprintf("via_set_layout_options_kb %lx\n", value);
    vial_config.raw = value;

    touch_config.raw = 0;
    touch_config.dpi = get_dpi(vial_config.dpi_mode);

    set_scroll_sens(SCROLL_TABLE[vial_config.scroll_mode]);
    set_sniper_sens(SNIPER_TABLE[vial_config.sniper_mode]);
    set_text_sens(TEXT_TABLE[vial_config.text_mode]);
    set_invert_scroll(vial_config.invert_scroll);
}

void sync_touch(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&touch_config, in_data, sizeof(touch_config_t));
    pointing_device_set_cpi(touch_config.dpi);
    touch_config.dpi = pointing_device_get_cpi();
    memcpy(out_data, &touch_config, sizeof(touch_config_t));
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

    if (is_display_side() && is_keyboard_master()) {
        static uint32_t       last_sync = 0;
        static touch_config_t slave     = {.raw = 0};

        if (last_sync == 0 || timer_elapsed32(last_sync) > 500) {
            if (slave.raw != touch_config.raw) {
                if (transaction_rpc_exec(RPC_SYNC_TOUCH, sizeof(touch_config_t), &touch_config, sizeof(touch_config_t), &slave)) {
                    dprintf("sync touch settings %d (slave %d)\n", touch_config.dpi, slave.dpi);
                }
                last_sync = timer_read32();
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

        {
            static uint32_t       last_sync   = 0;
            static touch_config_t real_config = {.raw = 0};

            if ((last_sync == 0 || timer_elapsed32(last_sync) > 100)) {
                if (touch_config.raw != real_config.raw) {
                    pointing_device_set_cpi(touch_config.dpi);
                    real_config.dpi = pointing_device_get_cpi();
                    dprintf("sync touch settings %d (real %d)\n", touch_config.dpi, real_config.dpi);
                }
                last_sync = timer_read32();
            }
        }
    }
}

void keyboard_post_init_user(void) {
    if (is_display_side()) {
        display_init_kb();
    }

    vial_config.raw = via_get_layout_options();
    via_set_layout_options_kb(vial_config.raw);
    set_led_blinks(false);

    transaction_register_rpc(RPC_SYNC_TOUCH, sync_touch);
    transaction_register_rpc(RPC_SYNC_DISPLAY, sync_display);
}
