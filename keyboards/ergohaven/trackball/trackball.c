#include QMK_KEYBOARD_H

#include "quantum.h"
#include "src/eh_pointing.h"

typedef enum { NL_SCROLL_SL_SNIPER, NL_SNIPER_SL_SCROLL, NL_TEXT_SL_SCROLL, LED_BLINKS } trackball_mode_t;
extern const int DPI_TABLE[15];
const int32_t    SNIPER_TABLE[15] = {2, 3, 4, 5};
const int32_t    SCROLL_TABLE[15] = {6, 8, 11, 16, 23, 32, 45, 64};
const int32_t    TEXT_TABLE[15]   = {6, 8, 11, 16, 23, 32, 45, 64};

typedef union {
    uint32_t raw;
    struct {
        uint8_t text_mode : 3;
        uint8_t scroll_mode : 3;
        uint8_t sniper_mode : 2;
        uint8_t dpi_mode : 4;
        uint8_t orientation : 2;
        uint8_t mode : 2;
        bool    invert_scroll : 1;
        bool    acceleration : 1;
    };
} vial_config_t;

static vial_config_t vial_config;

int get_dpi(uint8_t dpi_mode) {
    if (dpi_mode < ARRAY_SIZE(DPI_TABLE))
        return DPI_TABLE[dpi_mode];
    else
        return DPI_TABLE[0];
}

void update_settings(led_t leds) {
    switch (vial_config.mode) {
        case NL_SCROLL_SL_SNIPER:
            if (leds.num_lock && !leds.scroll_lock)
                set_pointing_mode(POINTING_MODE_SCROLL);
            else if (!leds.num_lock && leds.scroll_lock)
                set_pointing_mode(POINTING_MODE_SNIPER);
            else
                set_pointing_mode(POINTING_MODE_NORMAL);
            break;

        case NL_SNIPER_SL_SCROLL:
            if (leds.num_lock && !leds.scroll_lock)
                set_pointing_mode(POINTING_MODE_SNIPER);
            else if (!leds.num_lock && leds.scroll_lock)
                set_pointing_mode(POINTING_MODE_SCROLL);
            else
                set_pointing_mode(POINTING_MODE_NORMAL);
            break;

        case NL_TEXT_SL_SCROLL:
            if (leds.num_lock && !leds.scroll_lock)
                set_pointing_mode(POINTING_MODE_TEXT);
            else if (!leds.num_lock && leds.scroll_lock)
                set_pointing_mode(POINTING_MODE_SCROLL);
            else
                set_pointing_mode(POINTING_MODE_NORMAL);
            break;

        default:
            break;
    }
}

void via_set_layout_options_kb(uint32_t value) {
    dprintf("via_set_layout_options_kb %lx\n", value);
    vial_config.raw = value;
    pointing_device_set_cpi(get_dpi(vial_config.dpi_mode));
    set_scroll_sens(SCROLL_TABLE[vial_config.scroll_mode]);
    set_sniper_sens(SNIPER_TABLE[vial_config.sniper_mode]);
    set_text_sens(TEXT_TABLE[vial_config.text_mode]);
    set_orientation(vial_config.orientation);
    set_invert_scroll(vial_config.invert_scroll);
    set_acceleration(vial_config.acceleration);
    update_settings(host_keyboard_led_state());
}

static led_t cur_leds;
static led_t start_leds;
static led_t end_leds;

bool led_update_user(led_t led_state) {
    if (vial_config.mode == LED_BLINKS) {
        static uint32_t t = 0;

        uint32_t elapsed = timer_elapsed32(t);

        t = timer_read32();

        led_t changed = {.raw = cur_leds.raw ^ led_state.raw};
        cur_leds      = led_state;

        if (elapsed > 100) {
            start_leds.raw = changed.raw;
            end_leds.raw   = 0;
        } else if (elapsed < 20 && end_leds.raw == 0)
            start_leds.raw |= changed.raw;
        else if (elapsed >= 20)
            end_leds.raw = changed.raw;
        else if (elapsed < 20 && end_leds.raw != 0)
            end_leds.raw |= changed.raw;

        dprintf("t=%lu num=%d scl=%d cps=%d\n", elapsed, changed.num_lock, changed.scroll_lock, changed.caps_lock);
        dprintf("start=%d end=%d\n", start_leds.raw, end_leds.raw);
    } else {
        update_settings(led_state);
    }
    return true;
}

void pointing_device_init_kb(void) {
#ifdef CONSOLE_ENABLE
    debug_enable = true;
#endif
    via_set_layout_options_kb(via_get_layout_options());
    set_led_blinks(false);
}

void housekeeping_task_user(void) {
#ifdef CONSOLE_ENABLE
    {
        static uint32_t last_sync = 0;

        if ((last_sync == 0 || timer_elapsed32(last_sync) > 500)) {
            uint16_t real_dpi   = pointing_device_get_cpi();
            uint16_t config_dpi = get_dpi(vial_config.dpi_mode);
            if (real_dpi != config_dpi) {
                pointing_device_set_cpi(config_dpi);
                dprintf("sync touch settings %d (real %d)\n", config_dpi, real_dpi);
            }
            last_sync = timer_read32();
        }
    }
#endif

    if (vial_config.mode == LED_BLINKS && start_leds.raw != 0 && start_leds.raw == end_leds.raw) {
        switch (start_leds.raw) {
            case 1:
                set_pointing_mode(POINTING_MODE_NORMAL);
                dprintf("set normal mode\n");
                break;
            case 4:
                set_pointing_mode(POINTING_MODE_SNIPER);
                dprintf("set sniper mode\n");
                break;
            case 2:
                set_pointing_mode(POINTING_MODE_SCROLL);
                dprintf("set scroll mode\n");
                break;
            case 5:
                set_pointing_mode(POINTING_MODE_TEXT);
                dprintf("set text mode\n");
                break;
            case 3:
                set_pointing_mode(POINTING_MODE_USR1);
                dprintf("set mode user 1\n");
                break;
            case 6:
                set_pointing_mode(POINTING_MODE_USR2);
                dprintf("set mode user 2\n");
                break;
            case 7:
                set_pointing_mode(POINTING_MODE_USR3);
                dprintf("set mode user 3\n");
                break;
            default:
                break;
        }
        start_leds.raw = 0;
    }
}
