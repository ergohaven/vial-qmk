#include "eh_settings.h"
#include "eh_ruen.h"
#include "eh_pointing.h"
#include "ergohaven_rgb.h"
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
#    include "drivers/sensors/analog_joystick.h"
#endif
#include <eeconfig.h>
#include <debug.h>
#include <qmk_settings.h>
#include "via.h"

char layer_names[DYNAMIC_KEYMAP_LAYER_COUNT][LAYER_LABEL_SIZE];

__attribute__((weak)) void kb_settings_led_colors_init(void) {}
__attribute__((weak)) void kb_settings_led_colors_reset(void) {}
__attribute__((weak)) void kb_settings_lcd_init(void) {}
__attribute__((weak)) void kb_settings_lcd_reset(void) {}
__attribute__((weak)) uint8_t get_layer_rgb_color(uint8_t layer) {
    (void)layer;
    return 0;
}
__attribute__((weak)) void set_layer_rgb_color(uint8_t layer, uint8_t color) {
    (void)layer;
    (void)color;
}
__attribute__((weak)) uint8_t get_led_rgb_brightness(void) {
    return 128;
}
__attribute__((weak)) void set_led_rgb_brightness(uint8_t brightness) {
    (void)brightness;
}
__attribute__((weak)) uint8_t get_led_rgb_timeout_mins(void) {
    return 10;
}
__attribute__((weak)) void set_led_rgb_timeout_mins(uint8_t timeout_mins) {
    (void)timeout_mins;
}
__attribute__((weak)) uint32_t get_led_rgb_timeout_ms(void) {
    return 10 * 60 * 1000;
}
__attribute__((weak)) uint8_t get_lcd_brightness(void) {
    return 10;
}
__attribute__((weak)) void set_lcd_brightness(uint8_t brightness) {
    (void)brightness;
}
__attribute__((weak)) uint8_t get_lcd_timeout_mins(void) {
    return 10;
}
__attribute__((weak)) void set_lcd_timeout_mins(uint8_t timeout_mins) {
    (void)timeout_mins;
}
__attribute__((weak)) uint32_t get_lcd_timeout_ms(void) {
    return 10 * 60 * 1000;
}
__attribute__((weak)) uint8_t get_split_lcd_brightness(void) {
    return get_lcd_brightness();
}
__attribute__((weak)) uint32_t get_split_lcd_timeout_ms(void) {
    return get_lcd_timeout_ms();
}

__attribute__((weak)) const char *default_layer_label(uint8_t layer) {
    static const char *PROGMEM default_layer_labels[] = {
        "BASE", "LOWER", "RAISE", "ADJST", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVN", "TWLVE", "THRTN", "FRTN", "FIFTN",
    };
    return default_layer_labels[layer];
}

void kb_settings_layer_labels_reset(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i) {
        eeconfig_update_kb_datablock(default_layer_label(i), KB_SETTINGS_LAYER_LABELS_OFFSET + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
    }
    layer_name_updated = true;
}

void kb_settings_layer_labels_init(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i)
        eeconfig_read_kb_datablock(layer_names[i], KB_SETTINGS_LAYER_LABELS_OFFSET + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
}

void kb_settings_reset(void) {
    kb_settings_ruen_reset();
    kb_settings_layer_labels_reset();
    kb_settings_pointing_reset();
    kb_settings_split_pointing_reset();
    kb_settings_led_colors_reset();
    kb_settings_lcd_reset();
    kb_settings_init();
}

void eeconfig_init_kb(void) {
    kb_settings_reset();
    eeconfig_init_user();
}

void kb_settings_init(void) {
    kb_settings_ruen_init();
    kb_settings_layer_labels_init();
    kb_settings_pointing_init();
    kb_settings_split_pointing_init();
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
    maxCursorSpeed = get_settings_pointing().sens[POINTING_MODE_NORMAL] ? get_settings_pointing().sens[POINTING_MODE_NORMAL] : ANALOG_JOYSTICK_SPEED_MAX;
    uint8_t joystick_regulator = (get_settings_pointing().dpi >= 101 && get_settings_pointing().dpi <= 150) ? (uint8_t)(get_settings_pointing().dpi - 100) : ANALOG_JOYSTICK_SPEED_REGULATOR;
    speedRegulator = joystick_regulator;
#endif
    kb_settings_led_colors_init();
    kb_settings_lcd_init();
}

#define DECLARE_SETTING_NOTIFY(id, _get, _set, _notify) {.qsid = id, .get = _get, .set = _set, .notify = _notify}
#define DECLARE_SETTING(id, _get, _set) DECLARE_SETTING_NOTIFY(id, _get, _set, NULL)

static int ruen_toggle_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t mode = get_ruen_toggle_mode();
    if (maxsz < sizeof(mode)) return -1;
    memcpy(setting, &mode, sizeof(mode));
    return 0;
}

static int ruen_toggle_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t mode;
    if (maxsz < sizeof(mode)) return -1;
    memcpy(&mode, setting, sizeof(mode));
    set_ruen_toggle_mode(mode);
    return 0;
}

static int ruen_macos_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool mac = get_ruen_mac_layout();
    if (maxsz < sizeof(mac)) return -1;
    memcpy(setting, &mac, sizeof(mac));
    return 0;
}

static int ruen_macos_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool mac;
    if (maxsz < sizeof(mac)) return -1;
    memcpy(&mac, setting, sizeof(mac));
    set_ruen_mac_layout(mac);
    return 0;
}

// should match with json/settings_ruen.json
// and UNICODE_SELECTED_MODES in config.h
static int unicode_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t unicode_mode = get_unicode_input_mode();
    uint8_t mode         = 0;
    switch (unicode_mode) {
        case UNICODE_MODE_MACOS:
            mode = 0;
            break;
        case UNICODE_MODE_LINUX:
            mode = 1;
            break;
        case UNICODE_MODE_WINDOWS:
            mode = 2;
            break;
        case UNICODE_MODE_WINCOMPOSE:
            mode = 3;
            break;
        default:
            return -1;
    }
    if (maxsz < sizeof(mode)) return -1;
    memcpy(setting, &mode, sizeof(mode));
    return 0;
}

static int unicode_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t mode = 0;
    if (maxsz < sizeof(mode)) return -1;
    memcpy(&mode, setting, sizeof(mode));
    switch (mode) {
        case 0:
            set_unicode_input_mode(UNICODE_MODE_MACOS);
            break;
        case 1:
            set_unicode_input_mode(UNICODE_MODE_LINUX);
            break;
        case 2:
            set_unicode_input_mode(UNICODE_MODE_WINDOWS);
            break;
        case 3:
            set_unicode_input_mode(UNICODE_MODE_WINCOMPOSE);
            break;
        default:
            return -1;
    }
    return 0;
}

const char *layer_name(uint8_t layer) {
    if (layer >= 0 && layer <= 15)
        return layer_names[layer];
    else
        return "UNDEF";
}

static int layer_name_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    int layer = proto->qsid - 200;
    if (layer < 0 || layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return -1;
    strcpy(setting, layer_names[layer]);
    return 0;
}

bool layer_name_updated = false;

static int layer_name_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    int layer = proto->qsid - 200;
    if (layer < 0 || layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return -1;
    dprintf("layer_name_set %d %s\n", layer, (const char *)setting);
    snprintf(layer_names[layer], sizeof(layer_names[layer]), (const char *)setting);
    eeconfig_update_kb_datablock(layer_names[layer], KB_SETTINGS_LAYER_LABELS_OFFSET + LAYER_LABEL_SIZE * layer, LAYER_LABEL_SIZE);
    layer_name_updated = true;
    return 0;
}

#if !defined(EH_KEYBOARD_SPLIT_POINTING_V2) && !defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD) && !defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL)
static const uint16_t phenom_trackball_cpi_table[] = {200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000, 3200};
static const uint16_t phenom_touchpad_cpi_table[]  = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
#endif

static split_pointing_device_id_t split_pointing_device_for_qsid(uint16_t qsid) {
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (qsid) {
        case 120:
        case 130:
            return SPLIT_POINTING_DEVICE_LEFT_BALL;
        case 121:
        case 131:
            return SPLIT_POINTING_DEVICE_RIGHT_BALL;
        case 122:
        case 132:
            return SPLIT_POINTING_DEVICE_LEFT_TOUCH;
        case 123:
        case 133:
            return SPLIT_POINTING_DEVICE_RIGHT_TOUCH;
        default:
            return SPLIT_POINTING_DEVICE_LEFT_BALL;
    }
#else
    switch (qsid) {
        case 120:
        case 129:
            return SPLIT_POINTING_DEVICE_LEFT_BALL;
        case 121:
        case 130:
            return SPLIT_POINTING_DEVICE_RIGHT_BALL;
        case 122:
        case 131:
            return SPLIT_POINTING_DEVICE_LEFT_TOUCH;
        case 123:
        case 132:
            return SPLIT_POINTING_DEVICE_RIGHT_TOUCH;
        default:
            return SPLIT_POINTING_DEVICE_LEFT_BALL;
    }
#endif
}

#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
static split_pointing_side_t split_pointing_side_for_qsid(uint16_t qsid) {
    switch (qsid) {
        case 124:
        case 125:
        case 126:
        case 134:
        case 136:
        case 137:
        case 140:
        case 147:
        case 149:
        case 151:
        case 325:
        case 327:
            return SPLIT_POINTING_SIDE_LEFT;
        case 127:
        case 128:
        case 129:
        case 135:
        case 138:
        case 139:
        case 141:
        case 148:
        case 150:
        case 152:
        case 326:
        case 328:
            return SPLIT_POINTING_SIDE_RIGHT;
        default:
            return SPLIT_POINTING_SIDE_LEFT;
    }
}
#endif

#if !defined(EH_KEYBOARD_SPLIT_POINTING_V2) && !defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD) && !defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL)
static uint8_t phenom_index_from_value_u16(const uint16_t *table, size_t count, uint16_t value) {
    uint8_t best    = 0;
    uint16_t best_d = UINT16_MAX;
    for (size_t i = 0; i < count; ++i) {
        uint16_t d = table[i] > value ? table[i] - value : value - table[i];
        if (d < best_d) {
            best_d = d;
            best   = (uint8_t)i;
        }
    }
    return best;
}

static uint16_t phenom_value_from_index_u16(const uint16_t *table, size_t count, uint8_t idx) {
    if (idx >= count) idx = (uint8_t)(count - 1);
    return table[idx];
}

static const uint16_t *phenom_dpi_table_for_qsid(uint16_t qsid, size_t *count) {
    switch (qsid) {
        case 120:
        case 121:
            *count = ARRAY_SIZE(phenom_trackball_cpi_table);
            return phenom_trackball_cpi_table;
        case 122:
        case 123:
            *count = ARRAY_SIZE(phenom_touchpad_cpi_table);
            return phenom_touchpad_cpi_table;
        default:
            *count = 0;
            return NULL;
    }
}

__attribute__((unused)) static int modules_trackball_dpi_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    size_t count = 0;
    const uint16_t *table = phenom_dpi_table_for_qsid(proto->qsid, &count);
    if (!table) return -1;
    uint16_t cpi = phenom_value_from_index_u16(table, count, get_split_pointing_device_dpi_index(split_pointing_device_for_qsid(proto->qsid)));
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(setting, &cpi, sizeof(cpi));
    return 0;
}

__attribute__((unused)) static int modules_trackball_dpi_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    size_t count = 0;
    const uint16_t *table = phenom_dpi_table_for_qsid(proto->qsid, &count);
    if (!table) return -1;
    uint16_t cpi;
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(&cpi, setting, sizeof(cpi));
    set_split_pointing_device_dpi_index(split_pointing_device_for_qsid(proto->qsid), phenom_index_from_value_u16(table, count, cpi));
    return 0;
}
#endif

#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
static int modules_dpi_select_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = get_split_pointing_device_dpi_index(split_pointing_device_for_qsid(proto->qsid));
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int modules_dpi_select_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    set_split_pointing_device_dpi_index(split_pointing_device_for_qsid(proto->qsid), value);
    return 0;
}
#endif

static int modules_sens_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t sens = 0;
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (proto->qsid) {
        case 124:
        case 127:
            sens = get_split_pointing_side_sens(split_pointing_side_for_qsid(proto->qsid), POINTING_MODE_SNIPER);
            break;
        case 125:
        case 128:
            sens = get_split_pointing_side_sens(split_pointing_side_for_qsid(proto->qsid), POINTING_MODE_SCROLL);
            break;
        case 126:
        case 129:
            sens = get_split_pointing_side_sens(split_pointing_side_for_qsid(proto->qsid), POINTING_MODE_TEXT);
            break;
        default:
            return -1;
    }
#else
    switch (proto->qsid) {
        case 124:
            sens = get_sniper_sens();
            break;
        case 125:
            sens = get_scroll_sens();
            break;
        case 126:
            sens = get_text_sens();
            break;
        default:
            return -1;
    }
#endif
    if (maxsz < sizeof(sens)) return -1;
    memcpy(setting, &sens, sizeof(sens));
    return 0;
}

static int modules_sens_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t sens;
    if (maxsz < sizeof(sens)) return -1;
    memcpy(&sens, setting, sizeof(sens));
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (proto->qsid) {
        case 124:
        case 127:
            set_split_pointing_side_sens(split_pointing_side_for_qsid(proto->qsid), POINTING_MODE_SNIPER, sens);
            break;
        case 125:
        case 128:
            set_split_pointing_side_sens(split_pointing_side_for_qsid(proto->qsid), POINTING_MODE_SCROLL, sens);
            break;
        case 126:
        case 129:
            set_split_pointing_side_sens(split_pointing_side_for_qsid(proto->qsid), POINTING_MODE_TEXT, sens);
            break;
        default:
            return -1;
    }
#else
    switch (proto->qsid) {
        case 124:
            set_sniper_sens(sens);
            break;
        case 125:
            set_scroll_sens(sens);
            break;
        case 126:
            set_text_sens(sens);
            break;
        default:
            return -1;
    }
#endif
    return 0;
}

static int modules_bool_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool value = false;
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (proto->qsid) {
        case 136: value = get_split_pointing_side_invert_scroll(SPLIT_POINTING_SIDE_LEFT); break;
        case 137: value = get_split_pointing_side_acceleration(SPLIT_POINTING_SIDE_LEFT); break;
        case 138: value = get_split_pointing_side_invert_scroll(SPLIT_POINTING_SIDE_RIGHT); break;
        case 139: value = get_split_pointing_side_acceleration(SPLIT_POINTING_SIDE_RIGHT); break;
        case 140:
        case 141: value = get_split_pointing_side_sticky_mode(split_pointing_side_for_qsid(proto->qsid)); break;
        case 142: value = get_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_NORMAL); break;
        case 144: value = get_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_SNIPER); break;
        case 145: value = get_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_SCROLL); break;
        case 146: value = get_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_TEXT); break;
        case 147:
        case 148: value = get_split_pointing_side_invert_text(split_pointing_side_for_qsid(proto->qsid)); break;
#    ifdef EH_SPLIT_POINTING_INVERT_AXES
        case 149:
        case 150: value = get_split_pointing_side_invert_scroll_h(split_pointing_side_for_qsid(proto->qsid)); break;
        case 151:
        case 152: value = get_split_pointing_side_invert_text_h(split_pointing_side_for_qsid(proto->qsid)); break;
#    endif
        case 327:
        case 328: value = get_split_pointing_side_touch_gestures(split_pointing_side_for_qsid(proto->qsid)); break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 127: value = get_invert_scroll(); break;
        case 128: value = get_acceleration(); break;
        case 136: value = get_invert_text(); break;
        case 327: value = get_pointing_touch_gestures(); break;
        case 133:
        case 140: value = get_sticky_mode(); break;
        case 134:
        case 141:
        case 142: value = get_led_blinks(); break;
        default: return -1;
    }
#endif
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int modules_bool_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool value;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (proto->qsid) {
        case 136: set_split_pointing_side_invert_scroll(SPLIT_POINTING_SIDE_LEFT, value); break;
        case 137: set_split_pointing_side_acceleration(SPLIT_POINTING_SIDE_LEFT, value); break;
        case 138: set_split_pointing_side_invert_scroll(SPLIT_POINTING_SIDE_RIGHT, value); break;
        case 139: set_split_pointing_side_acceleration(SPLIT_POINTING_SIDE_RIGHT, value); break;
        case 140:
        case 141: set_split_pointing_side_sticky_mode(split_pointing_side_for_qsid(proto->qsid), value); break;
        case 142: set_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_NORMAL, value); break;
        case 144: set_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_SNIPER, value); break;
        case 145: set_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_SCROLL, value); break;
        case 146: set_split_pointing_auto_mouse_mode_enabled(POINTING_MODE_TEXT, value); break;
        case 147:
        case 148: set_split_pointing_side_invert_text(split_pointing_side_for_qsid(proto->qsid), value); break;
#    ifdef EH_SPLIT_POINTING_INVERT_AXES
        case 149:
        case 150: set_split_pointing_side_invert_scroll_h(split_pointing_side_for_qsid(proto->qsid), value); break;
        case 151:
        case 152: set_split_pointing_side_invert_text_h(split_pointing_side_for_qsid(proto->qsid), value); break;
#    endif
        case 327:
        case 328: set_split_pointing_side_touch_gestures(split_pointing_side_for_qsid(proto->qsid), value); break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 127: set_invert_scroll(value); break;
        case 128: set_acceleration(value); break;
        case 136: set_invert_text(value); break;
        case 327: set_pointing_touch_gestures(value); break;
        case 133:
        case 140: set_sticky_mode(value); break;
        case 134:
        case 141:
        case 142: set_led_blinks(value); break;
        default: return -1;
    }
#endif
    return 0;
}

static int modules_select_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t v = 0;
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (proto->qsid) {
        case 130:
        case 131:
        case 132:
        case 133:
            v = (uint8_t)get_split_pointing_device_orientation(split_pointing_device_for_qsid(proto->qsid));
            break;
        case 134:
        case 135:
            v = (uint8_t)get_split_pointing_side_mode(split_pointing_side_for_qsid(proto->qsid));
            break;
        case 143:
            v = get_split_pointing_auto_mouse_layer();
            break;
        case 324:
            v = get_split_pointing_auto_mouse_timeout_idx();
            break;
        case 325:
        case 326:
            v = get_split_pointing_side_encoder_interval_idx(split_pointing_side_for_qsid(proto->qsid));
            break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 324: {
            static const uint16_t timeout_variants[] = {250, 500, 750, 1000, 1250, 1500};
            uint16_t              timeout_ms         = 750;
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
            if (qmk_settings_get(32, &timeout_ms, sizeof(timeout_ms)) < 0) {
                timeout_ms = 750;
            }
#    endif
            uint16_t best_delta = UINT16_MAX;
            for (uint8_t i = 0; i < ARRAY_SIZE(timeout_variants); ++i) {
                uint16_t delta = timeout_variants[i] > timeout_ms ? timeout_variants[i] - timeout_ms : timeout_ms - timeout_variants[i];
                if (delta < best_delta) {
                    best_delta = delta;
                    v          = i;
                }
            }
            break;
        }
        case 325:
            v = get_pointing_encoder_interval_idx();
            break;
        case 129:
        case 130:
        case 131:
        case 132:
            v = (uint8_t)get_split_pointing_device_orientation(split_pointing_device_for_qsid(proto->qsid));
            break;
        case 135:
        case 143:
            v = (uint8_t)get_pointing_mode();
            break;
        default:
            return -1;
    }
#endif
    if (maxsz < sizeof(v)) return -1;
    memcpy(setting, &v, sizeof(v));
    return 0;
}

static int modules_select_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t v;
    if (maxsz < sizeof(v)) return -1;
    memcpy(&v, setting, sizeof(v));
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    switch (proto->qsid) {
        case 130:
        case 131:
        case 132:
        case 133:
            set_split_pointing_device_orientation(split_pointing_device_for_qsid(proto->qsid), (orientation_t)v);
            break;
        case 134:
        case 135:
            set_split_pointing_side_mode(split_pointing_side_for_qsid(proto->qsid), (pointing_mode_t)v);
            break;
        case 143:
            set_split_pointing_auto_mouse_layer(v);
            break;
        case 324:
            set_split_pointing_auto_mouse_timeout_idx(v);
            break;
        case 325:
        case 326:
            set_split_pointing_side_encoder_interval_idx(split_pointing_side_for_qsid(proto->qsid), v);
            break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 324: {
            static const uint16_t timeout_variants[] = {250, 500, 750, 1000, 1250, 1500};
            if (v >= ARRAY_SIZE(timeout_variants)) {
                v = 2;
            }
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
            uint16_t timeout_ms = timeout_variants[v];
            if (qmk_settings_set(32, &timeout_ms, sizeof(timeout_ms)) < 0) {
                return -1;
            }
#    endif
            break;
        }
        case 325:
            set_pointing_encoder_interval_idx(v);
            break;
        case 129:
        case 130:
        case 131:
        case 132:
            set_split_pointing_device_orientation(split_pointing_device_for_qsid(proto->qsid), (orientation_t)v);
            break;
        case 135:
        case 143:
            set_pointing_mode((pointing_mode_t)v);
            break;
        default:
            return -1;
    }
#endif
    return 0;
}

#if defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD) || defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL) || defined(EH_QMK_SETTINGS_SIMPLE_JOYSTICK)
#ifdef EH_QMK_SETTINGS_SIMPLE_TOUCHPAD
static const uint16_t simple_touchpad_cpi_table[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
#else
static const uint16_t simple_trackball_cpi_table[] = {200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000, 3200};
#endif

__attribute__((unused)) static int simple_touchpad_dpi_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t  value = 0;
    uint16_t cpi   = get_cpi();
    (void)proto;
#ifdef EH_QMK_SETTINGS_SIMPLE_TOUCHPAD
    const uint16_t *cpi_table = simple_touchpad_cpi_table;
    const uint8_t cpi_table_size = ARRAY_SIZE(simple_touchpad_cpi_table);
#else
    const uint16_t *cpi_table = simple_trackball_cpi_table;
    const uint8_t cpi_table_size = ARRAY_SIZE(simple_trackball_cpi_table);
#endif
    for (uint8_t i = 0; i < cpi_table_size; ++i) {
        if (cpi_table[i] == cpi) {
            value = i;
            break;
        }
    }
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

__attribute__((unused)) static int simple_touchpad_dpi_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
#ifdef EH_QMK_SETTINGS_SIMPLE_TOUCHPAD
    const uint16_t *cpi_table = simple_touchpad_cpi_table;
    const uint8_t cpi_table_size = ARRAY_SIZE(simple_touchpad_cpi_table);
#else
    const uint16_t *cpi_table = simple_trackball_cpi_table;
    const uint8_t cpi_table_size = ARRAY_SIZE(simple_trackball_cpi_table);
#endif
    if (value >= cpi_table_size) {
        value = cpi_table_size - 1;
    }
    set_cpi(cpi_table[value]);
    return 0;
}

static int simple_touchpad_sens_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value;
    switch (proto->qsid) {
        case 121: value = get_sniper_sens(); break;
        case 122: value = get_scroll_sens(); break;
        case 123: value = get_text_sens(); break;
        default: return -1;
    }
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_touchpad_sens_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    switch (proto->qsid) {
        case 121: set_sniper_sens(value); break;
        case 122: set_scroll_sens(value); break;
        case 123: set_text_sens(value); break;
        default: return -1;
    }
    return 0;
}

static int simple_touchpad_flags_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = 0;
    (void)proto;
    value |= get_invert_scroll() ? (1 << 0) : 0;
    value |= get_acceleration() ? (1 << 1) : 0;
    value |= get_sticky_mode() ? (1 << 2) : 0;
    value |= get_invert_text() ? (1 << 3) : 0;
    value |= get_invert_scroll_h() ? (1 << 4) : 0;
    value |= get_invert_text_h() ? (1 << 5) : 0;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_touchpad_flags_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    set_invert_scroll(value & (1 << 0));
    set_acceleration(value & (1 << 1));
    set_sticky_mode(value & (1 << 2));
    set_invert_text(value & (1 << 3));
    set_invert_scroll_h(value & (1 << 4));
    set_invert_text_h(value & (1 << 5));
    return 0;
}

static int simple_touchpad_auto_layer_enable_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool value = get_split_pointing_auto_mouse_enable();
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_touchpad_auto_layer_enable_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    set_split_pointing_auto_mouse_enable(value);
    return 0;
}

static int simple_touchpad_auto_layer_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = get_split_pointing_auto_mouse_layer();
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_touchpad_auto_layer_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    set_split_pointing_auto_mouse_layer(value);
    return 0;
}
#endif

#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
static int simple_joystick_speed_max_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = get_settings_pointing().sens[POINTING_MODE_NORMAL] ? get_settings_pointing().sens[POINTING_MODE_NORMAL] : ANALOG_JOYSTICK_SPEED_MAX;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_joystick_speed_max_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    if (value < 1) value = 1;
    kb_settings_pointing_t config = get_settings_pointing();
    config.sens[POINTING_MODE_NORMAL] = value;
    kb_settings_pointing_update(config);
    maxCursorSpeed = value;
    return 0;
}

static int simple_joystick_regulator_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = (get_settings_pointing().dpi >= 101 && get_settings_pointing().dpi <= 150) ? (uint8_t)(get_settings_pointing().dpi - 100) : ANALOG_JOYSTICK_SPEED_REGULATOR;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_joystick_regulator_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    if (value < 1) value = 1;
    kb_settings_pointing_t config = get_settings_pointing();
    config.dpi = (uint16_t)(100 + value);
    kb_settings_pointing_update(config);
    speedRegulator = value;
    return 0;
}

static int simple_joystick_mode_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = get_settings_pointing().mode == POINTING_MODE_TEXT ? 1 : 0;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int simple_joystick_mode_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    kb_settings_pointing_t config = get_settings_pointing();
    config.mode = value == 1 ? POINTING_MODE_TEXT : POINTING_MODE_NORMAL;
    set_settings_pointing(config);
    return 0;
}

#endif

#ifndef EH_QMK_SETTINGS_DISABLE_LAYER_LEDS
static int leds_color_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t layer = (uint8_t)(proto->qsid - 300);
    uint8_t value;
    if (layer >= EH_RGB_LAYER_COUNT || maxsz < sizeof(value)) return -1;
    value = get_layer_rgb_color(layer);
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int leds_color_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t layer = (uint8_t)(proto->qsid - 300);
    uint8_t value;
    if (layer >= EH_RGB_LAYER_COUNT || maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    set_layer_rgb_color(layer, value);
    return 0;
}

static int leds_brightness_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint16_t value = get_led_rgb_brightness();
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int leds_brightness_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint16_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    if (value > 255) value = 255;
    set_led_rgb_brightness((uint8_t)value);
    return 0;
}

static int leds_timeout_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};
    uint8_t              minutes            = get_led_rgb_timeout_mins();
    uint8_t              value              = 4; // 10 min
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    for (uint8_t i = 0; i < sizeof(timeout_variants); ++i) {
        if (timeout_variants[i] == minutes) {
            value = i;
            break;
        }
    }
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int leds_timeout_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};
    uint8_t              value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    if (value < sizeof(timeout_variants)) {
        set_led_rgb_timeout_mins(timeout_variants[value]);
    } else {
        set_led_rgb_timeout_mins(value);
    }
    return 0;
}
#endif

static int lcd_brightness_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t value = get_lcd_brightness();
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int lcd_brightness_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    set_lcd_brightness(value);
    return 0;
}

static int lcd_timeout_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};
    uint8_t              minutes            = get_lcd_timeout_mins();
    uint8_t              value              = 4; // 10 min
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    for (uint8_t i = 0; i < sizeof(timeout_variants); ++i) {
        if (timeout_variants[i] == minutes) {
            value = i;
            break;
        }
    }
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int lcd_timeout_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};
    uint8_t              value;
    (void)proto;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    if (value < sizeof(timeout_variants)) {
        set_lcd_timeout_mins(timeout_variants[value]);
    } else {
        set_lcd_timeout_mins(value);
    }
    return 0;
}

qmk_settings_proto_t kb_protos[KB_SETTINGS_NPROTOS] PROGMEM = {
    // clang-format off
    DECLARE_SETTING(100, ruen_toggle_get, ruen_toggle_set),
    DECLARE_SETTING(101, ruen_macos_get, ruen_macos_set),
    DECLARE_SETTING(102, unicode_get, unicode_set),
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    DECLARE_SETTING(120, modules_dpi_select_get, modules_dpi_select_set),
    DECLARE_SETTING(121, modules_dpi_select_get, modules_dpi_select_set),
    DECLARE_SETTING(122, modules_dpi_select_get, modules_dpi_select_set),
    DECLARE_SETTING(123, modules_dpi_select_get, modules_dpi_select_set),
#else
#    if defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD) || defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL)
    DECLARE_SETTING(120, simple_touchpad_dpi_get, simple_touchpad_dpi_set),
    DECLARE_SETTING(121, simple_touchpad_sens_get, simple_touchpad_sens_set),
    DECLARE_SETTING(122, simple_touchpad_sens_get, simple_touchpad_sens_set),
    DECLARE_SETTING(123, simple_touchpad_sens_get, simple_touchpad_sens_set),
#    elif defined(EH_QMK_SETTINGS_SIMPLE_JOYSTICK)
    DECLARE_SETTING(121, simple_touchpad_sens_get, simple_touchpad_sens_set),
    DECLARE_SETTING(122, simple_touchpad_sens_get, simple_touchpad_sens_set),
    DECLARE_SETTING(123, simple_touchpad_sens_get, simple_touchpad_sens_set),
#    else
    DECLARE_SETTING(120, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(121, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(122, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(123, modules_trackball_dpi_get, modules_trackball_dpi_set),
#    endif
#endif
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    DECLARE_SETTING(124, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(125, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(126, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(127, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(128, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(129, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(130, modules_select_get, modules_select_set),
    DECLARE_SETTING(131, modules_select_get, modules_select_set),
    DECLARE_SETTING(132, modules_select_get, modules_select_set),
    DECLARE_SETTING(133, modules_select_get, modules_select_set),
    DECLARE_SETTING(134, modules_select_get, modules_select_set),
    DECLARE_SETTING(135, modules_select_get, modules_select_set),
    DECLARE_SETTING(136, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(137, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(138, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(139, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(140, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(141, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(142, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(143, modules_select_get, modules_select_set),
    DECLARE_SETTING(144, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(145, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(146, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(147, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(148, modules_bool_get, modules_bool_set),
#    ifdef EH_SPLIT_POINTING_INVERT_AXES
    DECLARE_SETTING(149, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(150, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(151, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(152, modules_bool_get, modules_bool_set),
#    endif
#else
#    if defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD) || defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL) || defined(EH_QMK_SETTINGS_SIMPLE_JOYSTICK)
    DECLARE_SETTING(124, simple_touchpad_flags_get, simple_touchpad_flags_set),
    DECLARE_SETTING(125, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(126, modules_sens_get, modules_sens_set),
#    else
    DECLARE_SETTING(124, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(125, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(126, modules_sens_get, modules_sens_set),
#    endif
    DECLARE_SETTING(127, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(128, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(129, modules_select_get, modules_select_set),
    DECLARE_SETTING(130, modules_select_get, modules_select_set),
    DECLARE_SETTING(131, modules_select_get, modules_select_set),
    DECLARE_SETTING(132, modules_select_get, modules_select_set),
    DECLARE_SETTING(133, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(134, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(135, modules_select_get, modules_select_set),
    DECLARE_SETTING(136, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(137, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(138, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(139, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(140, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(141, modules_bool_get, modules_bool_set),
#    if defined(EH_QMK_SETTINGS_SIMPLE_TOUCHPAD) || defined(EH_QMK_SETTINGS_SIMPLE_TRACKBALL) || defined(EH_QMK_SETTINGS_SIMPLE_JOYSTICK)
    DECLARE_SETTING(142, simple_touchpad_auto_layer_enable_get, simple_touchpad_auto_layer_enable_set),
    DECLARE_SETTING(143, simple_touchpad_auto_layer_get, simple_touchpad_auto_layer_set),
#        if defined(EH_QMK_SETTINGS_SIMPLE_JOYSTICK)
    DECLARE_SETTING(144, simple_joystick_speed_max_get, simple_joystick_speed_max_set),
    DECLARE_SETTING(145, simple_joystick_regulator_get, simple_joystick_regulator_set),
    DECLARE_SETTING(146, simple_joystick_mode_get, simple_joystick_mode_set),
#        endif
#    else
    DECLARE_SETTING(142, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(143, modules_select_get, modules_select_set),
#    endif
#endif
    DECLARE_SETTING(200, layer_name_get, layer_name_set),
    DECLARE_SETTING(201, layer_name_get, layer_name_set),
    DECLARE_SETTING(202, layer_name_get, layer_name_set),
    DECLARE_SETTING(203, layer_name_get, layer_name_set),
    DECLARE_SETTING(204, layer_name_get, layer_name_set),
    DECLARE_SETTING(205, layer_name_get, layer_name_set),
    DECLARE_SETTING(206, layer_name_get, layer_name_set),
    DECLARE_SETTING(207, layer_name_get, layer_name_set),
    DECLARE_SETTING(208, layer_name_get, layer_name_set),
    DECLARE_SETTING(209, layer_name_get, layer_name_set),
    DECLARE_SETTING(210, layer_name_get, layer_name_set),
    DECLARE_SETTING(211, layer_name_get, layer_name_set),
    DECLARE_SETTING(212, layer_name_get, layer_name_set),
    DECLARE_SETTING(213, layer_name_get, layer_name_set),
    DECLARE_SETTING(214, layer_name_get, layer_name_set),
    DECLARE_SETTING(215, layer_name_get, layer_name_set),
#ifndef EH_QMK_SETTINGS_DISABLE_LAYER_LEDS
    DECLARE_SETTING(300, leds_color_get, leds_color_set),
    DECLARE_SETTING(301, leds_color_get, leds_color_set),
    DECLARE_SETTING(302, leds_color_get, leds_color_set),
    DECLARE_SETTING(303, leds_color_get, leds_color_set),
    DECLARE_SETTING(304, leds_color_get, leds_color_set),
    DECLARE_SETTING(305, leds_color_get, leds_color_set),
    DECLARE_SETTING(306, leds_color_get, leds_color_set),
    DECLARE_SETTING(307, leds_color_get, leds_color_set),
    DECLARE_SETTING(308, leds_color_get, leds_color_set),
    DECLARE_SETTING(309, leds_color_get, leds_color_set),
    DECLARE_SETTING(310, leds_color_get, leds_color_set),
    DECLARE_SETTING(311, leds_color_get, leds_color_set),
    DECLARE_SETTING(312, leds_color_get, leds_color_set),
    DECLARE_SETTING(313, leds_color_get, leds_color_set),
    DECLARE_SETTING(314, leds_color_get, leds_color_set),
    DECLARE_SETTING(315, leds_color_get, leds_color_set),
    DECLARE_SETTING(316, leds_brightness_get, leds_brightness_set),
    DECLARE_SETTING(317, leds_timeout_get, leds_timeout_set),
#endif
    DECLARE_SETTING(318, lcd_brightness_get, lcd_brightness_set),
    DECLARE_SETTING(319, lcd_timeout_get, lcd_timeout_set),
#if defined(EH_KEYBOARD_SPLIT_POINTING_V2)
    DECLARE_SETTING(324, modules_select_get, modules_select_set),
    DECLARE_SETTING(325, modules_select_get, modules_select_set),
    DECLARE_SETTING(326, modules_select_get, modules_select_set),
    DECLARE_SETTING(327, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(328, modules_bool_get, modules_bool_set),
#else
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    DECLARE_SETTING(324, modules_select_get, modules_select_set),
#    endif
#    ifdef EH_ENCODER_INTERVAL_SETTINGS
    DECLARE_SETTING(325, modules_select_get, modules_select_set),
#    endif
#    ifdef POINTING_DEVICE_DRIVER_azoteq_iqs5xx
    DECLARE_SETTING(327, modules_bool_get, modules_bool_set),
#    endif
#endif
    // clang-format on
};
