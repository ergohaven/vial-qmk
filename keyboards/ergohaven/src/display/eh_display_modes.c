#include "display.h"
#include "qp.h"
#include "src/eh_ruen.h"
#include "hid.h"
#include "ergohaven.h"
#include "eh_symbols.h"
#include "eh_display.h"

static uint32_t screen_timer = 0;

typedef enum {
    SCREEN_OFF = -1,
    SCREEN_SPLASH,
    SCREEN_HOME,
    SCREEN_VOLUME,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;

eh_screen_t current_screen;

void display_init_screens_kb(void) {
    eh_screen_splash.init();
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

    hid_data_t *hid_data   = get_hid_data();
    bool        hid_active = is_hid_active();
    if (hid_active && hid_data->hid_changed) {
        if (hid_data->volume_changed) {
            change_screen_state = SCREEN_VOLUME;
            screen_timer        = timer_read32();
        }
    }

    uint32_t activity_elapsed = last_input_activity_elapsed();
    if (activity_elapsed > __UINT32_MAX__ - 1000) // possible overflow on split
        activity_elapsed = 0;

    if (screen_state == change_screen_state) {
        uint32_t screen_elapsed = timer_elapsed32(screen_timer);

        switch (screen_state) {
            case SCREEN_SPLASH:
                if (screen_elapsed > EH_DISPLAY_TIMEOUT_SPLASH_SCREEN) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_HOME:
                if (activity_elapsed > EH_TIMEOUT) {
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
                    change_screen_state = SCREEN_HOME;
                }
                break;
        }
    }

    if (change_screen_state != screen_state) {
        bool typing = activity_elapsed < 500; // prevent display updates when typing
        if (typing) return;

        dprintf("change screen state %d->%d\n", screen_state, change_screen_state);
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
