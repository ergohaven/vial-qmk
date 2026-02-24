#include "src/eh_pointing.h"
#include "quantum.h"
#include "hid.h"

static kb_settings_pointing_t kb_settings_pointing;

static_assert(KB_SETTINGS_POINTING_SIZE == sizeof(kb_settings_pointing_t), "Invalid KB_SETTINGS_POINTING_SIZE");

__attribute__((weak)) kb_settings_pointing_t get_settings_pointing_default(void) {
    kb_settings_pointing_t dflt = {
        .sens          = {1, 2, 16, 32},
        .dpi           = 400,
        .invert_scroll = false,
        .acceleration  = false,
        .orientation   = ROT_0,
        .mode          = 0,
        .sticky_mode   = true,
        .led_blinks    = false,
    };
    return dflt;
}

void kb_settings_pointing_update(kb_settings_pointing_t new_config) {
#if defined(POINTING_DEVICE_ENABLE) && !defined(POINTING_DEVICE_DRIVER_analog_joystick)
    if (new_config.dpi != kb_settings_pointing.dpi) {
        pointing_device_set_cpi(new_config.dpi);
    }
#endif
    if (new_config.raw != kb_settings_pointing.raw) {
        kb_settings_pointing = new_config;
        dprintf("dpi=%d s1=%d s2=%d s3=%d acc=%d inv=%d\n", kb_settings_pointing.dpi, kb_settings_pointing.sens[1], kb_settings_pointing.sens[2], kb_settings_pointing.sens[3], kb_settings_pointing.acceleration, kb_settings_pointing.invert_scroll);
        eeconfig_update_kb_datablock(&kb_settings_pointing, KB_SETTINGS_POINTING_OFFSET, sizeof(kb_settings_pointing_t));
    }
}

void kb_settings_pointing_init(void) {
    eeconfig_read_kb_datablock(&kb_settings_pointing, KB_SETTINGS_POINTING_OFFSET, sizeof(kb_settings_pointing_t));
#if defined(POINTING_DEVICE_ENABLE) && !defined(POINTING_DEVICE_DRIVER_analog_joystick)
    pointing_device_set_cpi(kb_settings_pointing.dpi);
#endif
    dprintf("dpi=%d s1=%d s2=%d s3=%d acc=%d inv=%d\n", kb_settings_pointing.dpi, kb_settings_pointing.sens[1], kb_settings_pointing.sens[2], kb_settings_pointing.sens[3], kb_settings_pointing.acceleration, kb_settings_pointing.invert_scroll);
}

void kb_settings_pointing_reset(void) {
    kb_settings_pointing_update(get_settings_pointing_default());
}

pointing_mode_t pointing_mode = POINTING_MODE_NORMAL;

void set_cpi(uint16_t cpi) {
#if defined(POINTING_DEVICE_ENABLE) && !defined(POINTING_DEVICE_DRIVER_analog_joystick)
    uint16_t actual_dpi;
    for (int i = 0; i < 5; ++i) { // bug in touchpad driver
        pointing_device_set_cpi(cpi);
        actual_dpi = pointing_device_get_cpi();
        dprintf("set dpi=%d actual dpi=%d\n", cpi, actual_dpi);
        if (actual_dpi == cpi) break;
    }
    cpi = actual_dpi;
#endif

    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.dpi                    = cpi;
    kb_settings_pointing_update(new_config);
}

uint16_t get_cpi(void) {
    return kb_settings_pointing.dpi;
}

void set_sniper_sens(uint8_t s) {
    kb_settings_pointing_t new_config     = kb_settings_pointing;
    new_config.sens[POINTING_MODE_SNIPER] = s;
    kb_settings_pointing_update(new_config);
}

uint8_t get_sniper_sens(void) {
    return kb_settings_pointing.sens[POINTING_MODE_SNIPER];
}

void set_scroll_sens(uint8_t s) {
    kb_settings_pointing_t new_config     = kb_settings_pointing;
    new_config.sens[POINTING_MODE_SCROLL] = s;
    kb_settings_pointing_update(new_config);
}

uint8_t get_scroll_sens(void) {
    return kb_settings_pointing.sens[POINTING_MODE_SCROLL];
}

void set_text_sens(uint8_t s) {
    kb_settings_pointing_t new_config   = kb_settings_pointing;
    new_config.sens[POINTING_MODE_TEXT] = s;
    kb_settings_pointing_update(new_config);
}

uint8_t get_text_sens(void) {
    return kb_settings_pointing.sens[POINTING_MODE_TEXT];
}

void set_invert_scroll(bool invert) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.invert_scroll          = invert;
    kb_settings_pointing_update(new_config);
}

bool get_invert_scroll(void) {
    return kb_settings_pointing.invert_scroll;
}

void set_orientation(orientation_t o) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.orientation            = o;
    kb_settings_pointing_update(new_config);
}

orientation_t get_orientation(void) {
    return kb_settings_pointing.orientation;
}

void set_acceleration(bool acc) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.acceleration           = acc;
    kb_settings_pointing_update(new_config);
}

bool get_acceleration(void) {
    return kb_settings_pointing.acceleration;
}

void set_sticky_mode(bool sticky_mode) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.sticky_mode            = sticky_mode;
    kb_settings_pointing_update(new_config);
}

bool get_sticky_mode(void) {
    return kb_settings_pointing.sticky_mode;
}

void set_led_blinks(bool led) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.led_blinks             = led;
    kb_settings_pointing_update(new_config);
}

bool get_led_blinks(void) {
    return kb_settings_pointing.led_blinks;
}

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE

bool is_mouse_active = false;

bool auto_mouse_activation(report_mouse_t mouse_report) {
    return is_mouse_active;
}

bool is_mouse_record_kb(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case EH_SCR:
        case EH_SNP:
        case EH_TXT:
        case EH_USR1:
        case EH_USR2:
        case EH_USR3:
            return true;
        default:
            return false;
    }

    return is_mouse_record_user(keycode, record);
}

#endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

void set_pointing_mode_from_hid(pointing_mode_t mode) {
    pointing_mode = mode;
}

void set_pointing_mode(pointing_mode_t mode) {
    if (mode != pointing_mode) {
        pointing_mode = mode;
        if (is_hid_active()) {
            hid_send_pointing_mode(mode);
        } else if (get_led_blinks()) {
            switch (pointing_mode) {
                case POINTING_MODE_NORMAL:
                    register_code(KC_NUM_LOCK);
                    wait_ms(10);
                    unregister_code(KC_NUM_LOCK);
                    wait_ms(20);
                    register_code(KC_NUM_LOCK);
                    wait_ms(10);
                    unregister_code(KC_NUM_LOCK);
                    break;
                case POINTING_MODE_SNIPER:
                    register_code(KC_SCROLL_LOCK);
                    wait_ms(10);
                    unregister_code(KC_SCROLL_LOCK);
                    wait_ms(20);
                    register_code(KC_SCROLL_LOCK);
                    wait_ms(10);
                    unregister_code(KC_SCROLL_LOCK);
                    break;
                case POINTING_MODE_SCROLL:
                    register_code(KC_CAPS_LOCK);
                    wait_ms(10);
                    unregister_code(KC_CAPS_LOCK);
                    wait_ms(20);
                    register_code(KC_CAPS_LOCK);
                    wait_ms(10);
                    unregister_code(KC_CAPS_LOCK);
                    break;
                case POINTING_MODE_TEXT:
                    register_code(KC_NUM_LOCK);
                    register_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_NUM_LOCK);
                    unregister_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    wait_ms(20);
                    register_code(KC_NUM_LOCK);
                    register_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_SCROLL_LOCK);
                    unregister_code(KC_NUM_LOCK);
                    send_keyboard_report();
                    break;
                case POINTING_MODE_USR1:
                    register_code(KC_NUM_LOCK);
                    register_code(KC_CAPS_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_NUM_LOCK);
                    unregister_code(KC_CAPS_LOCK);
                    send_keyboard_report();
                    wait_ms(20);
                    register_code(KC_NUM_LOCK);
                    register_code(KC_CAPS_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_CAPS_LOCK);
                    unregister_code(KC_NUM_LOCK);
                    send_keyboard_report();
                    break;
                case POINTING_MODE_USR2:
                    register_code(KC_SCROLL_LOCK);
                    register_code(KC_CAPS_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_SCROLL_LOCK);
                    unregister_code(KC_CAPS_LOCK);
                    send_keyboard_report();
                    wait_ms(20);
                    register_code(KC_SCROLL_LOCK);
                    register_code(KC_CAPS_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_CAPS_LOCK);
                    unregister_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    break;
                case POINTING_MODE_USR3:
                    register_code(KC_NUM_LOCK);
                    register_code(KC_CAPS_LOCK);
                    register_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_NUM_LOCK);
                    unregister_code(KC_CAPS_LOCK);
                    unregister_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    wait_ms(20);
                    register_code(KC_NUM_LOCK);
                    register_code(KC_CAPS_LOCK);
                    register_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    wait_ms(10);
                    unregister_code(KC_NUM_LOCK);
                    unregister_code(KC_CAPS_LOCK);
                    unregister_code(KC_SCROLL_LOCK);
                    send_keyboard_report();
                    break;
                default:
                    break;
            }
        }
    }
}

bool process_record_pointing(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case EH_SCR:
        case EH_TXT:
        case EH_SNP:
        case EH_USR1:
        case EH_USR2:
        case EH_USR3: {
            static uint16_t        press_timer        = 0;
            static pointing_mode_t prev_pointing_mode = POINTING_MODE_NORMAL;

            const pointing_mode_t NEW_MODE = POINTING_MODE_SNIPER + (keycode - EH_SNP);

            if (record->event.pressed) {
                prev_pointing_mode = pointing_mode;
                set_pointing_mode(NEW_MODE);
                press_timer = timer_read();
            } else {
                if (get_sticky_mode() && timer_elapsed(press_timer) < get_tapping_term(keycode, record)) {
                    if (prev_pointing_mode == NEW_MODE)
                        set_pointing_mode(POINTING_MODE_NORMAL);
                    else
                        set_pointing_mode(NEW_MODE);
                } else
                    set_pointing_mode(POINTING_MODE_NORMAL);
            }
            return false;
        }

        case EH_LED_BL:
            if (record->event.pressed) set_led_blinks(!get_led_blinks());
            return false;
    }

    return true;
}

report_mouse_t pointing_device_task_user(report_mouse_t mrpt) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    is_mouse_active = abs(mrpt.x) >= 1 || abs(mrpt.y) >= 1 || abs(mrpt.v) >= 1 || abs(mrpt.h) >= 1 || mrpt.buttons;
#endif
    pointing_mode_t pmode = pointing_mode;

    // dealing with two finger gesture on touch
    if (mrpt.h != 0 || mrpt.v != 0) {
        pmode  = POINTING_MODE_SCROLL;
        mrpt.x = mrpt.h;
        mrpt.y = mrpt.v;
        mrpt.h = 0;
        mrpt.v = 0;
    }

    switch (get_orientation()) {
        int8_t tmp;
        case ROT_0:
            break;
        case ROT_270:
            tmp    = mrpt.x;
            mrpt.x = mrpt.y;
            mrpt.y = -tmp;
            break;
        case ROT_180:
            mrpt.x = -mrpt.x;
            mrpt.y = -mrpt.y;
            break;
        case ROT_90:
            tmp    = mrpt.x;
            mrpt.x = -mrpt.y;
            mrpt.y = tmp;
            break;
    }

    if (get_acceleration()) {
        mouse_xy_report_t x = mrpt.x;
        mouse_xy_report_t y = mrpt.y;

        mrpt.x = (mouse_xy_report_t)(x > 0 ? x * x / 16 + x : -x * x / 16 + x);
        mrpt.y = (mouse_xy_report_t)(y > 0 ? y * y / 16 + y : -y * y / 16 + y);
    }

    static int32_t accumulated_h = 0;
    static int32_t accumulated_v = 0;

    static uint16_t kc_up    = KC_UP;
    static uint16_t kc_down  = KC_DOWN;
    static uint16_t kc_left  = KC_LEFT;
    static uint16_t kc_right = KC_RIGHT;
#ifdef EH_HPD_LAYERS
    static uint8_t cur_layer = 255;
    uint8_t        layer     = get_current_layer();
    if (cur_layer != layer) {
        kc_up     = dynamic_keymap_get_keycode(layer, 11, 0);
        kc_left   = dynamic_keymap_get_keycode(layer, 11, 1);
        kc_right  = dynamic_keymap_get_keycode(layer, 11, 2);
        kc_down   = dynamic_keymap_get_keycode(layer, 11, 3);
        cur_layer = layer;
    }
    if (kc_up != kc_down || kc_up != kc_left || kc_up != kc_right || kc_up != KC_NO) {
        pmode = POINTING_MODE_TEXT;
    }
#endif

    if (pmode != POINTING_MODE_NORMAL) {
        int32_t divisor = kb_settings_pointing.sens[MIN(pmode, POINTING_MODE_TEXT)];

        accumulated_h += mrpt.x;
        accumulated_v += mrpt.y;

        int shift_x = accumulated_h / divisor;
        int shift_y = accumulated_v / divisor;

        accumulated_h -= shift_x * divisor;
        accumulated_v -= shift_y * divisor;

        mrpt.x = 0;
        mrpt.y = 0;

        if (shift_x == 0 && shift_y == 0) return mrpt;

        switch (pmode) {
            case POINTING_MODE_SNIPER:
                mrpt.x = shift_x;
                mrpt.y = shift_y;
                break;

            case POINTING_MODE_SCROLL:
                if (abs(shift_x) > abs(shift_y)) {
                    shift_y       = 0;
                    accumulated_v = 0;
                } else if (abs(shift_x) < abs(shift_y)) {
                    shift_x       = 0;
                    accumulated_h = 0;
                }
                mrpt.h = shift_x;
                mrpt.v = -shift_y;
                break;

            case POINTING_MODE_TEXT:
            case POINTING_MODE_USR1:
            case POINTING_MODE_USR2:
            case POINTING_MODE_USR3: {
#ifdef EH_TRACKBALL_LAYERS
                uint8_t layer = pmode - POINTING_MODE_TEXT;

                kc_up    = dynamic_keymap_get_keycode(layer, 0, 0);
                kc_down  = dynamic_keymap_get_keycode(layer, 0, 1);
                kc_left  = dynamic_keymap_get_keycode(layer, 1, 0);
                kc_right = dynamic_keymap_get_keycode(layer, 1, 1);
#endif
                if (abs(shift_x) > abs(shift_y)) {
                    shift_y       = 0;
                    accumulated_v = 0;
                } else if (abs(shift_x) < abs(shift_y)) {
                    shift_x       = 0;
                    accumulated_h = 0;
                }

                for (; shift_x > 0; shift_x--)
                    tap_code16(kc_right);

                for (; shift_x < 0; shift_x++)
                    tap_code16(kc_left);

                for (; shift_y < 0; shift_y++)
                    tap_code16(kc_up);

                for (; shift_y > 0; shift_y--)
                    tap_code16(kc_down);

                break;
            }

            default:
            case POINTING_MODE_NORMAL:
                break;
        }
    } else {
        accumulated_h = 0;
        accumulated_v = 0;
    }

    if (get_invert_scroll()) {
        mrpt.v = -mrpt.v;
        mrpt.h = -mrpt.h;
    }

    return mrpt;
}
