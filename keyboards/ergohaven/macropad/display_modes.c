#include "display.h"
#include "qp.h"
#include "src/eh_ruen.h"
#include "hid.h"
#include "ergohaven.h"
#include "src/display/eh_symbols.h"
#include "src/display/eh_display.h"

extern const eh_screen_t eh_screen_layout;

static uint32_t screen_timer = 0;

typedef enum {
    SCREEN_OFF = -1,
    SCREEN_SPLASH,
    SCREEN_LAYOUT,
    SCREEN_VOLUME,
    SCREEN_HOME,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;

eh_screen_t current_screen;

void display_init_screens_kb(void) {
    eh_screen_splash.init();
    eh_screen_layout.init();
    eh_screen_home.init();
    eh_screen_volume.init();
    current_screen      = eh_screen_splash;
    change_screen_state = SCREEN_SPLASH;
    screen_state        = SCREEN_SPLASH;
    current_screen.load();
    display_turn_on();
    screen_timer = timer_read32();
}

void display_housekeeping_task(void) {
    if (!is_display_enabled()) return;

    static uint8_t prev_layer = 0;
    uint8_t        layer      = get_current_layer();
    if (layer != prev_layer) {
        prev_layer          = layer;
        change_screen_state = SCREEN_LAYOUT;
        return;
    }

    hid_data_t *hid_data   = get_hid_data();
    bool        hid_active = is_hid_active();
    if (hid_active && hid_data->hid_changed) {
        if (hid_data->volume_changed) {
            change_screen_state = SCREEN_VOLUME;
            screen_timer        = timer_read32();
        }
        if (hid_data->media_artist_changed) {
            change_screen_state = SCREEN_HOME;
        }
        if (hid_data->media_title_changed) {
            change_screen_state = SCREEN_HOME;
        }
    }

    if (screen_state == change_screen_state) {
        uint32_t screen_elapsed   = timer_elapsed32(screen_timer);
        uint32_t activity_elapsed = last_input_activity_elapsed();

        switch (screen_state) {
            case SCREEN_SPLASH:
                if (screen_elapsed > EH_DISPLAY_TIMEOUT_SPLASH_SCREEN) {
                    change_screen_state = SCREEN_LAYOUT;
                }
                break;

            case SCREEN_LAYOUT:
                if (hid_active && activity_elapsed > EH_DISPLAY_TIMEOUT_ACTIVITY) {
                    change_screen_state = SCREEN_HOME;
                } else if (activity_elapsed > EH_TIMEOUT) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_HOME:
                if (!hid_active) {
                    change_screen_state = SCREEN_LAYOUT;
                } else if (activity_elapsed > EH_TIMEOUT && screen_elapsed > EH_DISPLAY_TIMEOUT_ACTIVITY) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_VOLUME:
                if (screen_elapsed > EH_DISPLAY_TIMEOUT_VOLUME_SCREEN) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_OFF:
                if (activity_elapsed < EH_TIMEOUT) {
                    change_screen_state = SCREEN_LAYOUT;
                }
                break;
        }
    }

    if (change_screen_state != screen_state) {
        screen_timer = timer_read32();
        screen_state = change_screen_state;
        switch (screen_state) {
            case SCREEN_SPLASH:
                current_screen = eh_screen_splash;
                display_turn_on();
                break;
            case SCREEN_HOME:
                current_screen = eh_screen_home;
                display_turn_on();
                break;
            case SCREEN_LAYOUT:
                current_screen = eh_screen_layout;
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
        current_screen.load();
        return;
    }

    current_screen.housekeep();
}
