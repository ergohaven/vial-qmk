#include "display.h"
#include "qp.h"
#include "src/eh_ruen.h"
#include "hid.h"
#include "ergohaven.h"
#include "src/display/eh_symbols.h"
#include "src/display/eh_display.h"
#include "src/display/lvgl_helpers.h"

extern const eh_screen_t eh_screen_layout;
extern const eh_screen_t eh_screen_anim;



static uint32_t screen_timer = 0;

typedef enum {
    SCREEN_OFF = -1,
    SCREEN_ANIM,
    SCREEN_LAYOUT,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;

eh_screen_t current_screen;

void display_init_screens_kb(void) {
    eh_screen_anim.init();
    eh_screen_layout.init();
    current_screen      = eh_screen_anim;
    change_screen_state = SCREEN_ANIM;
    screen_state        = SCREEN_ANIM;
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

    if (screen_state == change_screen_state) {
        uint32_t activity_elapsed = last_input_activity_elapsed();

        switch (screen_state) {
            case SCREEN_ANIM:
                if (activity_elapsed < 100) {
                    change_screen_state = SCREEN_LAYOUT;
                } else if (activity_elapsed > EH_TIMEOUT) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_LAYOUT:
                if (activity_elapsed > EH_TIMEOUT) {
                    change_screen_state = SCREEN_OFF;
                } else if (activity_elapsed > 10000) {
                    change_screen_state = SCREEN_ANIM;
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
            case SCREEN_ANIM:
                current_screen = eh_screen_anim;
                display_turn_on();
                break;
            case SCREEN_LAYOUT:
                current_screen = eh_screen_layout;
                display_turn_on();
                break;
            case SCREEN_OFF:
                current_screen = eh_screen_layout;
                display_turn_off();
                break;
        }
        current_screen.load();
        return;
    }

    current_screen.housekeep();
}
