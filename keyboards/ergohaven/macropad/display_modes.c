#ifdef EH_DATE_SETTINGS_ENABLE
#include "src/display/eh_date_settings.h"
#endif
#include "display.h"
#include "qp.h"
#include "src/eh_ruen.h"
#include "src/eh_settings.h"
#include "hid.h"
#include "ergohaven.h"
#include "src/display/eh_symbols.h"
#include "src/display/eh_display.h"

extern const eh_screen_t eh_screen_layout;
void screen_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed);
void screen_layout_process_encoder_event(uint8_t index, bool clockwise);
void screen_layout_apply_accent_color(void);
void screen_layout_apply_background_color(void);
void screen_layout_apply_button_style(uint8_t style);
bool screen_layout_has_active_key_animation(void);
void screen_volume_apply_accent_color(void);
void screen_home_apply_clock_settings(void);
void last_matrix_activity_trigger(void);
uint32_t last_matrix_activity_time(void);

static uint32_t screen_timer             = 0;
static uint32_t last_user_activity_timer = 0;
static uint32_t handled_matrix_activity  = 0;


typedef enum {
    SCREEN_OFF = -1,
    SCREEN_SPLASH,
    SCREEN_LAYOUT,
    SCREEN_VOLUME,
    SCREEN_HOME,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;
static screen_t volume_return_state = SCREEN_HOME;

eh_screen_t current_screen;

bool display_should_wake_on_usb_resume(void) {
    return screen_state != SCREEN_OFF;
}

static void apply_screen_state(void) {
    if (change_screen_state == screen_state) return;

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
            return;
    }
    current_screen.load();
}

void display_process_matrix_press(void) {
    if (!is_display_enabled()) return;
    last_matrix_activity_trigger();
    last_user_activity_timer = timer_read32();
    change_screen_state      = SCREEN_LAYOUT;
    screen_timer             = last_user_activity_timer;
    apply_screen_state();
}

void display_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
    if (pressed) {
        // Redundant fallback for synthetic/special key events which do not
        // produce a rising edge in the physical matrix scanner.
        display_process_matrix_press();
    }
    screen_layout_process_keyevent(row, col, pressed);
}

static int8_t volume_direction_for_keycode(uint16_t keycode) {
    switch (keycode) {
        case KC_AUDIO_VOL_UP:
        case KC_KB_VOLUME_UP:
            return 1;
        case KC_AUDIO_VOL_DOWN:
        case KC_KB_VOLUME_DOWN:
            return -1;
        default:
            return 0;
    }
}

void display_process_encoder_event(uint8_t index, bool clockwise, uint16_t keycode) {
    if (index != 0) return;
    last_user_activity_timer = timer_read32();

    int8_t volume_direction = volume_direction_for_keycode(keycode);
    if (screen_state == SCREEN_LAYOUT && change_screen_state == SCREEN_LAYOUT) {
        screen_layout_process_encoder_event(index, clockwise);
    }

    // Encoder movement is user input, but it does not change the current
    // display mode. Volume keycodes temporarily open the volume screen and
    // then return to the screen from which the encoder was turned.
    // Without a synchronized host value, keep the current screen. This only
    // controls the overlay; the encoder's USB volume action is unchanged.
    if (volume_direction == 0 || !is_hid_volume_active()) return;
    if (screen_state != SCREEN_VOLUME) {
        volume_return_state = screen_state == SCREEN_SPLASH ? SCREEN_LAYOUT : screen_state;
    }

    change_screen_state   = SCREEN_VOLUME;
    screen_timer          = timer_read32();
    apply_screen_state();
}

void display_accent_color_changed_kb(void) {
    last_user_activity_timer = timer_read32();
    screen_layout_apply_accent_color();
    screen_volume_apply_accent_color();
}

void display_background_color_changed_kb(void) {
    last_user_activity_timer = timer_read32();
    screen_layout_apply_background_color();
}

void display_button_style_changed_kb(uint8_t style) {
    last_user_activity_timer = timer_read32();
    screen_layout_apply_button_style(style);
}

void display_clock_settings_changed_kb(void) {
    last_user_activity_timer = timer_read32();
    screen_home_apply_clock_settings();
}

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
    screen_timer             = timer_read32();
    last_user_activity_timer = screen_timer;
    handled_matrix_activity  = last_matrix_activity_time();
}

void display_housekeeping_task(void) {
    if (!is_display_enabled()) return;

    // QMK updates this timestamp whenever the physical matrix changes. It is
    // independent of keycode processing, so it also catches layer/tap-dance
    // keys and remains a fallback if pre_process_record_kb is bypassed.
    uint32_t matrix_activity = last_matrix_activity_time();
    bool     matrix_changed  = matrix_activity != handled_matrix_activity;
    if (matrix_changed) handled_matrix_activity = matrix_activity;

    // The normal key callback wakes synchronously. Keep the global matrix
    // timestamp as an independent fallback for events consumed before
    // pre_process_record_kb (or any future special matrix event).
    if (matrix_changed) {
        last_user_activity_timer = timer_read32();
        change_screen_state      = SCREEN_LAYOUT;
        screen_timer             = last_user_activity_timer;
        apply_screen_state();
    }

    bool key_animation_active = screen_state == SCREEN_LAYOUT && screen_layout_has_active_key_animation();
    if (key_animation_active) {
        change_screen_state = SCREEN_LAYOUT;
        screen_timer        = timer_read32();
    }

    static uint8_t prev_layer = 0;
    uint8_t        layer      = get_current_layer();
    if (layer != prev_layer) {
        prev_layer               = layer;
        last_user_activity_timer = timer_read32();
        if (screen_state != SCREEN_OFF) change_screen_state = SCREEN_LAYOUT;
    }

    uint32_t user_activity_elapsed = timer_elapsed32(last_user_activity_timer);
    // Host volume packets synchronize data; they are not encoder input.
    // Only display_process_encoder_event opens the volume screen. While it
    // is already visible, its housekeep callback renders the latest host
    // value without extending the user's notification timeout.
    if (screen_state != SCREEN_VOLUME) get_hid_data()->volume_changed = false;

    if (screen_state == change_screen_state) {
        uint32_t screen_elapsed   = timer_elapsed32(screen_timer);
        uint32_t activity_elapsed = last_input_activity_elapsed();
#ifdef EH_DATE_SETTINGS_ENABLE
        // One standby-entry delay, independent of which elements are enabled.
        static const uint32_t standby_delays[] = {0,5000,10000,15000,30000,60000,120000,20000};
        uint32_t clock_delay = standby_delays[MIN(eh_date_get(9),7)];
#else
        uint32_t clock_delay = get_clock_delay_ms();
#endif
        uint32_t display_timeout  = get_lcd_timeout_ms();
        bool     display_expired  = display_timeout > 0 && activity_elapsed > display_timeout && user_activity_elapsed > 500;

        switch (screen_state) {
            case SCREEN_SPLASH:
                if (screen_elapsed > EH_DISPLAY_TIMEOUT_SPLASH_SCREEN) {
                    change_screen_state = SCREEN_LAYOUT;
                }
                break;

            case SCREEN_LAYOUT:
                if (display_expired) {
                    change_screen_state = SCREEN_OFF;
                } else if (clock_delay > 0 && user_activity_elapsed > clock_delay) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_HOME:
                if (display_expired && screen_elapsed > 1000) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_VOLUME:
                if (!is_hid_volume_active() || screen_elapsed > EH_DISPLAY_TIMEOUT_VOLUME_SCREEN) {
                    change_screen_state = volume_return_state;
                }
                break;

            case SCREEN_OFF:
                break;
        }
    }

    if (change_screen_state != screen_state) {
        apply_screen_state();
        return;
    }

    if (screen_state != SCREEN_OFF) current_screen.housekeep();
}
