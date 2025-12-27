#include "ergohaven_pointing.h"
#include "quantum.h"
#include "hid.h"

pointing_mode_t pointing_mode = POINTING_MODE_NORMAL;

static int32_t sens[4] = {1, 2, 16, 32};

void set_sniper_sens(int32_t s) {
    sens[POINTING_MODE_SNIPER] = s;
}

void set_scroll_sens(int32_t s) {
    sens[POINTING_MODE_SCROLL] = s;
}

void set_text_sens(int32_t s) {
    sens[POINTING_MODE_TEXT] = s;
}

static bool invert_scroll = false;

void set_invert_scroll(bool invert) {
    invert_scroll = invert;
}

static orientation_t orientation;

void set_orientation(orientation_t o) {
    orientation = o;
}

static bool acceleration = false;

void set_acceleration(bool acc) {
    acceleration = acc;
}

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE

void set_automouse(uint8_t layer) {
    if (layer == 0) {
        set_auto_mouse_enable(false);
    } else {
        set_auto_mouse_layer(layer);
        set_auto_mouse_enable(true);
    }
}

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

static bool led_blinks = true;

void set_led_blinks(bool led) {
    led_blinks = led;
}

bool get_led_blinks(void) {
    return led_blinks;
}

void set_pointing_mode_from_hid(pointing_mode_t mode) {
    pointing_mode = mode;
}

void set_pointing_mode(pointing_mode_t mode) {
    if (mode != pointing_mode) {
        pointing_mode = mode;
        if (is_hid_active()) {
            hid_send_pointing_mode(mode);
        } else if (led_blinks) {
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
                if (timer_elapsed(press_timer) < get_tapping_term(keycode, record)) {
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
            if (record->event.pressed) led_blinks = !led_blinks;
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

    switch (orientation) {
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

    if (acceleration)
    {
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
        int32_t divisor = sens[MIN(pmode, POINTING_MODE_TEXT)];

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

    if (invert_scroll) {
        mrpt.v = -mrpt.v;
        mrpt.h = -mrpt.h;
    }

    return mrpt;
}
