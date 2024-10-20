#include "display.h"
#include "qp.h"
#include "ergohaven_ruen.h"
#include "hid.h"
#include "ergohaven.h"
#include "ergohaven_symbols.h"
#include "ergohaven_display.h"
#include "k03pro.h"

static uint32_t screen_timer = 0;

typedef enum {
    SCREEN_OFF = -1,
    SCREEN_SPLASH,
    SCREEN_HOME,
    SCREEN_VOLUME,
    SCREEN_HID,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;

eh_screen_t current_screen;

void display_init_screens_kb(void) {
    eh_screen_splash.init();
    eh_screen_home.init();
    eh_screen_hid.init();
    eh_screen_volume.init();
    current_screen      = eh_screen_splash;
    change_screen_state = SCREEN_SPLASH;
}

void display_process_layer_state(uint8_t layer) {
    if (!is_display_enabled()) return;
    change_screen_state = SCREEN_HOME;
    current_screen.update_layer(layer);
}

void update_screen_state(void) {
    screen_timer = timer_read32();
    screen_state = change_screen_state;
    switch (screen_state) {
        case SCREEN_SPLASH:
            current_screen = eh_screen_splash;
            display_turn_on();
            break;
        case SCREEN_HID:
            current_screen = eh_screen_hid;
            display_turn_on();
            break;
        case SCREEN_HOME:
            current_screen = eh_screen_home;
            display_turn_on();
            break;
        case SCREEN_VOLUME:
            current_screen = eh_screen_volume;
            display_turn_on();
            break;
        case SCREEN_OFF:
            display_turn_off();
            break;
    }
    load_screen(current_screen);
}

void display_housekeeping_task(void) {
    if (!is_display_enabled()) return;

    current_screen.housekeep();

    hid_data_t *hid_data   = get_hid_data();
    bool        hid_active = is_hid_active();
    if (hid_data->hid_changed) {
        if (hid_data->volume_changed) {
            change_screen_state      = SCREEN_VOLUME;
            hid_data->volume_changed = false;
            screen_timer             = timer_read32();
        }
        if (hid_data->media_artist_changed) {
            change_screen_state            = SCREEN_HID;
            hid_data->media_artist_changed = false;
        }
        if (hid_data->media_title_changed) {
            change_screen_state           = SCREEN_HID;
            hid_data->media_title_changed = false;
        }
        current_screen.update_hid(hid_data);
        hid_data->hid_changed = false;
    }

    static uint8_t prev_lang = 0;
    uint8_t        cur_lang  = get_lang();
    if (prev_lang != cur_lang) {
        current_screen.update_layout(cur_lang);
        change_screen_state = SCREEN_HID;
        prev_lang           = cur_lang;
    }

    static uint8_t prev_layer = 255;
    uint8_t        cur_layer  = get_current_layer();
    if (prev_layer != cur_layer) {
        current_screen.update_layer(cur_layer);
        prev_layer = cur_layer;
    }

    led_t led_state = host_keyboard_led_state();
    led_state.caps_lock |= get_caps_word();
    current_screen.update_leds(led_state);

    uint8_t mods = get_mods() | get_oneshot_mods();
    current_screen.update_mods(mods);

    if (screen_state == change_screen_state) {
        uint32_t screen_elapsed   = timer_elapsed32(screen_timer);
        uint32_t activity_elapsed = last_input_activity_elapsed();
        if (activity_elapsed > __UINT32_MAX__ - 1000) // possible overflow on split
            activity_elapsed = 0;

        switch (screen_state) {
            case SCREEN_SPLASH:
                if (screen_elapsed > 2 * 1000) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_HOME:
                if (hid_active && activity_elapsed > 10 * 1000) {
                    change_screen_state = SCREEN_HID;
                } else if (activity_elapsed > EH_TIMEOUT) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_HID:
                if (!hid_active) {
                    change_screen_state = SCREEN_HOME;
                } else if (activity_elapsed > EH_TIMEOUT && screen_elapsed > 10 * 1000) {
                    change_screen_state = SCREEN_OFF;
                } else if (activity_elapsed < 10 * 1000) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_VOLUME:
                if (screen_elapsed > 2 * 1000) {
                    change_screen_state = SCREEN_HID;
                }
                break;

            case SCREEN_OFF:
                if (activity_elapsed < EH_TIMEOUT) {
                    change_screen_state = SCREEN_HOME;
                }
                break;
        }
    }

    if (change_screen_state != screen_state) {
        update_screen_state();
    }
}
