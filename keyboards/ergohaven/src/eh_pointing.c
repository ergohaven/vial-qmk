#include "src/eh_pointing.h"
#include "quantum.h"
#include "hid.h"
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
#    include "drivers/sensors/analog_joystick.h"
#endif
#ifdef POINTING_DEVICE_DRIVER_azoteq_iqs5xx
#    include "drivers/sensors/azoteq_iqs5xx.h"
#endif
#ifdef RGBLIGHT_ENABLE
#    include "ergohaven_rgb.h"
#endif
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h"
#endif
#include <string.h>

static kb_settings_pointing_t kb_settings_pointing;
static kb_settings_split_pointing_t kb_settings_phenom_devices;
pointing_mode_t                pointing_mode = POINTING_MODE_NORMAL;

#define PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK ((1u << POINTING_MODE_NORMAL) | (1u << POINTING_MODE_SNIPER) | (1u << POINTING_MODE_SCROLL) | (1u << POINTING_MODE_TEXT))
#define PHENOM_AUTO_MOUSE_TIMEOUT_IDX_COUNT 6
#define PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX 2
#define PHENOM_AUTO_MOUSE_MOTION_GRACE_MS 50
#define PHENOM_ENCODER_INTERVAL_IDX_COUNT 10
#define PHENOM_ENCODER_INTERVAL_DEFAULT_IDX 4
#define PHENOM_TOUCH_GESTURES_DEFAULT true
#define MODULE_ENCODER_INTERVAL_IDX_COUNT 10
#define MODULE_ENCODER_INTERVAL_DEFAULT_IDX 4
#ifdef EH_SPLIT_POINTING_INVERT_AXES
#    define PHENOM_SPLIT_POINTING_SETTINGS_VERSION 7
#else
#    define PHENOM_SPLIT_POINTING_SETTINGS_VERSION 6
#endif

static const uint8_t phenom_encoder_interval_ms_table[PHENOM_ENCODER_INTERVAL_IDX_COUNT] = {0, 5, 10, 15, 20, 30, 40, 60, 80, 100};
static const uint8_t module_encoder_interval_ms_table[MODULE_ENCODER_INTERVAL_IDX_COUNT] = {0, 5, 10, 15, 20, 30, 40, 60, 80, 100};

static uint8_t phenom_auto_mouse_mode_mask(pointing_mode_t mode) {
    if (mode < POINTING_MODE_NORMAL || mode > POINTING_MODE_USR3) {
        return 0;
    }
    return (uint8_t)(1u << (uint8_t)mode);
}

static uint16_t pointing_text_keycode_or_default(uint16_t keycode, uint16_t fallback) {
    return keycode == KC_NO || keycode == KC_TRNS ? fallback : keycode;
}

static void apply_auto_mouse_settings(void) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool enable = get_split_pointing_auto_mouse_enable();
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
    if (get_settings_pointing().mode == POINTING_MODE_TEXT) {
        enable = false;
    }
#endif
    auto_mouse_layer_off();
    set_auto_mouse_enable(enable);
    set_auto_mouse_layer(get_split_pointing_auto_mouse_layer());
#    ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    set_auto_mouse_timeout((uint16_t)((get_split_pointing_auto_mouse_timeout_idx() + 1) * 250));
#    endif
#    ifdef RGBLIGHT_ENABLE
    layer_state_set_rgb(layer_state | default_layer_state);
#    endif
#endif
}

#if defined(EH_KEYBOARD_SPLIT_POINTING_V2) && defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
uint8_t encoder_map_key_delay(uint8_t index) {
#    ifdef SPLIT_KEYBOARD
    split_pointing_side_t side = index < NUM_ENCODERS_LEFT ? SPLIT_POINTING_SIDE_LEFT : SPLIT_POINTING_SIDE_RIGHT;
#    else
    split_pointing_side_t side = SPLIT_POINTING_SIDE_LEFT;
#    endif
    return get_split_pointing_side_encoder_interval_ms(side);
}
#elif defined(EH_ENCODER_INTERVAL_SETTINGS) && defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
uint8_t encoder_map_key_delay(uint8_t index) {
    (void)index;
    return get_pointing_encoder_interval_ms();
}
#endif

static_assert(KB_SETTINGS_POINTING_SIZE == sizeof(kb_settings_pointing_t), "Invalid KB_SETTINGS_POINTING_SIZE");
static_assert(KB_SETTINGS_SPLIT_POINTING_SIZE == sizeof(kb_settings_split_pointing_t), "Invalid KB_SETTINGS_SPLIT_POINTING_SIZE");

#ifndef AUTO_MOUSE_DEFAULT_LAYER
#    define AUTO_MOUSE_DEFAULT_LAYER 4
#endif

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
        .invert_text   = false,
        .invert_scroll_h = false,
        .invert_text_h = false,
        .encoder_interval_slot = MODULE_ENCODER_INTERVAL_DEFAULT_IDX + 1,
        .touch_gestures_disabled = false,
    };
    return dflt;
}

kb_settings_split_pointing_t get_split_pointing_settings_default(void) {
    kb_settings_split_pointing_t dflt = {
        .axis              = {ROT_90, ROT_90, ROT_180, ROT_0},
        .dpi_idx           = {1, 1, 3, 3},
        .mode              = {POINTING_MODE_NORMAL, POINTING_MODE_NORMAL},
        .sens              = {{16, 4, 32}, {16, 4, 32}},
        .invert_scroll     = {false, false},
        .acceleration      = {false, false},
        .auto_mouse_enable = PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK,
        .auto_mouse_layer  = AUTO_MOUSE_DEFAULT_LAYER,
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
        .sticky_mode            = {true, true},
        .side_auto_mouse_enable = {PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK, PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK},
        .side_auto_mouse_layer  = {AUTO_MOUSE_DEFAULT_LAYER, AUTO_MOUSE_DEFAULT_LAYER},
        .invert_text            = {false, false},
        .version                = PHENOM_SPLIT_POINTING_SETTINGS_VERSION,
#    ifdef EH_SPLIT_POINTING_INVERT_AXES
        .invert_scroll_h        = {false, false},
        .invert_text_h          = {false, false},
#    endif
        .auto_mouse_timeout_idx = PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX,
        .encoder_interval_idx   = {PHENOM_ENCODER_INTERVAL_DEFAULT_IDX, PHENOM_ENCODER_INTERVAL_DEFAULT_IDX},
        .touch_gestures         = {PHENOM_TOUCH_GESTURES_DEFAULT, PHENOM_TOUCH_GESTURES_DEFAULT},
#endif
    };
    return dflt;
}

static bool config_all_zero(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

static kb_settings_split_pointing_t kb_settings_split_pointing_sanitize(kb_settings_split_pointing_t config) {
    if (config_all_zero(&config, sizeof(config))) {
        return get_split_pointing_settings_default();
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    bool legacy_config = config.version != PHENOM_SPLIT_POINTING_SETTINGS_VERSION;
    if (legacy_config) {
        uint8_t legacy_version = config.version;

        if (legacy_version < 5 || legacy_version > PHENOM_SPLIT_POINTING_SETTINGS_VERSION) {
            uint8_t legacy_auto_mouse_enable = config.auto_mouse_enable;
            uint8_t legacy_auto_mouse_layer  = config.auto_mouse_layer;
            bool    legacy_sticky_mode       = get_sticky_mode();

            if (legacy_auto_mouse_enable <= 1) {
                legacy_auto_mouse_enable = legacy_auto_mouse_enable ? PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK : 0;
            } else if (legacy_auto_mouse_enable & ~PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK) {
                legacy_auto_mouse_enable &= PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK;
            }
            if (legacy_auto_mouse_layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
                legacy_auto_mouse_layer = AUTO_MOUSE_DEFAULT_LAYER;
            }

            for (uint8_t side = 0; side < SPLIT_POINTING_SIDE_COUNT; ++side) {
                config.sticky_mode[side]            = legacy_sticky_mode;
                config.side_auto_mouse_enable[side] = legacy_auto_mouse_enable;
                config.side_auto_mouse_layer[side]  = legacy_auto_mouse_layer;
                config.invert_text[side]            = false;
            }
        }

#ifdef EH_SPLIT_POINTING_INVERT_AXES
        for (uint8_t side = 0; side < SPLIT_POINTING_SIDE_COUNT; ++side) {
            config.invert_scroll_h[side]        = config.invert_scroll[side];
            config.invert_text_h[side]          = false;
        }
#endif
        config.auto_mouse_timeout_idx = PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX;
        for (uint8_t side = 0; side < SPLIT_POINTING_SIDE_COUNT; ++side) {
            config.encoder_interval_idx[side] = PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
            config.touch_gestures[side]       = PHENOM_TOUCH_GESTURES_DEFAULT;
        }
        config.version = PHENOM_SPLIT_POINTING_SETTINGS_VERSION;
    }
#endif
    for (uint8_t side = 0; side < SPLIT_POINTING_SIDE_COUNT; ++side) {
        if (config.mode[side] > POINTING_MODE_TEXT) {
            config.mode[side] = POINTING_MODE_NORMAL;
        }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
        config.sticky_mode[side] = config.sticky_mode[side] != 0;
        config.invert_text[side] = config.invert_text[side] != 0;
#ifdef EH_SPLIT_POINTING_INVERT_AXES
        config.invert_scroll_h[side] = config.invert_scroll_h[side] != 0;
        config.invert_text_h[side]   = config.invert_text_h[side] != 0;
#endif
        if (config.side_auto_mouse_enable[side] & ~PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK) {
            config.side_auto_mouse_enable[side] &= PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK;
        }
        if (config.side_auto_mouse_layer[side] >= DYNAMIC_KEYMAP_LAYER_COUNT) {
            config.side_auto_mouse_layer[side] = AUTO_MOUSE_DEFAULT_LAYER;
        }
        if (config.encoder_interval_idx[side] >= PHENOM_ENCODER_INTERVAL_IDX_COUNT) {
            config.encoder_interval_idx[side] = PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
        }
        config.touch_gestures[side] = config.touch_gestures[side] != 0;
#endif
    }
    if (config.auto_mouse_enable <= 1) {
        config.auto_mouse_enable = config.auto_mouse_enable ? PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK : 0;
    } else if (config.auto_mouse_enable & ~PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK) {
        config.auto_mouse_enable &= PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK;
    }
    if (config.auto_mouse_layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
        config.auto_mouse_layer = AUTO_MOUSE_DEFAULT_LAYER;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    if (config.auto_mouse_timeout_idx >= PHENOM_AUTO_MOUSE_TIMEOUT_IDX_COUNT) {
        config.auto_mouse_timeout_idx = PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX;
    }
    config.version = PHENOM_SPLIT_POINTING_SETTINGS_VERSION;
#endif
    return config;
}

kb_settings_split_pointing_t get_split_pointing_settings(void) {
    return kb_settings_phenom_devices;
}

static void kb_settings_split_pointing_update(kb_settings_split_pointing_t new_config) {
    if (memcmp(&new_config, &kb_settings_phenom_devices, sizeof(new_config)) != 0) {
        kb_settings_phenom_devices = new_config;
        eeconfig_update_kb_datablock(&kb_settings_phenom_devices, KB_SETTINGS_SPLIT_POINTING_OFFSET, sizeof(kb_settings_phenom_devices));
    }
    apply_auto_mouse_settings();
}

void set_split_pointing_settings(kb_settings_split_pointing_t config) {
    kb_settings_split_pointing_update(config);
}

void kb_settings_split_pointing_init(void) {
    kb_settings_split_pointing_t stored;
    eeconfig_read_kb_datablock(&stored, KB_SETTINGS_SPLIT_POINTING_OFFSET, sizeof(stored));
    kb_settings_phenom_devices = kb_settings_split_pointing_sanitize(stored);
    if (memcmp(&stored, &kb_settings_phenom_devices, sizeof(stored)) != 0) {
        eeconfig_update_kb_datablock(&kb_settings_phenom_devices, KB_SETTINGS_SPLIT_POINTING_OFFSET, sizeof(kb_settings_phenom_devices));
    }
    apply_auto_mouse_settings();
}

void kb_settings_split_pointing_reset(void) {
    kb_settings_split_pointing_update(get_split_pointing_settings_default());
}

orientation_t get_split_pointing_device_orientation(split_pointing_device_id_t device) {
    if (device >= SPLIT_POINTING_DEVICE_COUNT) {
        return ROT_0;
    }
    return (orientation_t)kb_settings_phenom_devices.axis[device];
}

void set_split_pointing_device_orientation(split_pointing_device_id_t device, orientation_t orientation) {
    if (device >= SPLIT_POINTING_DEVICE_COUNT) {
        return;
    }
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.axis[device]               = (uint8_t)orientation;
    kb_settings_split_pointing_update(new_config);
}

uint8_t get_split_pointing_device_dpi_index(split_pointing_device_id_t device) {
    if (device >= SPLIT_POINTING_DEVICE_COUNT) {
        return 0;
    }
    return kb_settings_phenom_devices.dpi_idx[device];
}

void set_split_pointing_device_dpi_index(split_pointing_device_id_t device, uint8_t dpi_index) {
    if (device >= SPLIT_POINTING_DEVICE_COUNT) {
        return;
    }
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.dpi_idx[device]            = dpi_index;
    kb_settings_split_pointing_update(new_config);
}

static pointing_mode_t sanitize_split_pointing_mode(pointing_mode_t mode) {
    return mode > POINTING_MODE_TEXT ? POINTING_MODE_NORMAL : mode;
}

#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
// split-mode-restore-v0.0.11: mode keys are temporary overrides, not saved settings.
static bool            split_pointing_mode_override_active[SPLIT_POINTING_SIDE_COUNT] = {false, false};
static pointing_mode_t split_pointing_mode_override[SPLIT_POINTING_SIDE_COUNT]        = {POINTING_MODE_NORMAL, POINTING_MODE_NORMAL};

static void set_split_pointing_side_mode_override(split_pointing_side_t side, pointing_mode_t mode) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    split_pointing_mode_override[side]        = sanitize_split_pointing_mode(mode);
    split_pointing_mode_override_active[side] = true;
}

static void clear_split_pointing_side_mode_override(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    split_pointing_mode_override_active[side] = false;
}

static void restore_split_pointing_side_mode_override(split_pointing_side_t side, bool active, pointing_mode_t mode) {
    if (active) {
        set_split_pointing_side_mode_override(side, mode);
    } else {
        clear_split_pointing_side_mode_override(side);
    }
}

static void toggle_split_pointing_side_sticky_override(split_pointing_side_t side, pointing_mode_t mode) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    mode = sanitize_split_pointing_mode(mode);
    if (split_pointing_mode_override_active[side] && split_pointing_mode_override[side] == mode) {
        clear_split_pointing_side_mode_override(side);
    } else {
        set_split_pointing_side_mode_override(side, mode);
    }
}
#endif

pointing_mode_t get_split_pointing_side_mode(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return POINTING_MODE_NORMAL;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    if (split_pointing_mode_override_active[side]) {
        return split_pointing_mode_override[side];
    }
#endif
    return (pointing_mode_t)kb_settings_phenom_devices.mode[side];
}

void set_split_pointing_side_mode(split_pointing_side_t side, pointing_mode_t mode) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    mode = sanitize_split_pointing_mode(mode);
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.mode[side]                 = (uint8_t)mode;
    kb_settings_split_pointing_update(new_config);
}

uint8_t get_split_pointing_side_sens(split_pointing_side_t side, pointing_mode_t mode) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return 1;
    }
    switch (mode) {
        case POINTING_MODE_SNIPER:
            return kb_settings_phenom_devices.sens[side][0];
        case POINTING_MODE_SCROLL:
            return kb_settings_phenom_devices.sens[side][1];
        case POINTING_MODE_TEXT:
            return kb_settings_phenom_devices.sens[side][2];
        default:
            return 1;
    }
}

void set_split_pointing_side_sens(split_pointing_side_t side, pointing_mode_t mode, uint8_t sens) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    switch (mode) {
        case POINTING_MODE_SNIPER:
            new_config.sens[side][0] = sens;
            break;
        case POINTING_MODE_SCROLL:
            new_config.sens[side][1] = sens;
            break;
        case POINTING_MODE_TEXT:
            new_config.sens[side][2] = sens;
            break;
        default:
            return;
    }
    kb_settings_split_pointing_update(new_config);
}

bool get_split_pointing_side_invert_scroll(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return false;
    }
    return kb_settings_phenom_devices.invert_scroll[side] != 0;
}

void set_split_pointing_side_invert_scroll(split_pointing_side_t side, bool invert_scroll) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.invert_scroll[side]        = invert_scroll;
    kb_settings_split_pointing_update(new_config);
}

#ifdef EH_SPLIT_POINTING_INVERT_AXES
bool get_split_pointing_side_invert_scroll_h(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return false;
    }
    return kb_settings_phenom_devices.invert_scroll_h[side] != 0;
}

void set_split_pointing_side_invert_scroll_h(split_pointing_side_t side, bool invert_scroll) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.invert_scroll_h[side]       = invert_scroll;
    kb_settings_split_pointing_update(new_config);
}
#endif

bool get_split_pointing_side_acceleration(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return false;
    }
    return kb_settings_phenom_devices.acceleration[side] != 0;
}

void set_split_pointing_side_acceleration(split_pointing_side_t side, bool acceleration) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.acceleration[side]         = acceleration;
    kb_settings_split_pointing_update(new_config);
}

bool get_split_pointing_auto_mouse_enable(void) {
    return kb_settings_phenom_devices.auto_mouse_enable != 0;
}

void set_split_pointing_auto_mouse_enable(bool enable) {
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.auto_mouse_enable          = enable ? PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK : 0;
    kb_settings_split_pointing_update(new_config);
}

bool get_split_pointing_auto_mouse_mode_enabled(pointing_mode_t mode) {
    uint8_t mask = phenom_auto_mouse_mode_mask(mode);
    if (mask == 0) {
        return false;
    }
    return (kb_settings_phenom_devices.auto_mouse_enable & mask) != 0;
}

void set_split_pointing_auto_mouse_mode_enabled(pointing_mode_t mode, bool enabled) {
    uint8_t mask = phenom_auto_mouse_mode_mask(mode);
    if (mask == 0) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    if (enabled) {
        new_config.auto_mouse_enable |= mask;
    } else {
        new_config.auto_mouse_enable &= ~mask;
    }
    kb_settings_split_pointing_update(new_config);
}

uint8_t get_split_pointing_auto_mouse_layer(void) {
    if (kb_settings_phenom_devices.auto_mouse_layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
        return AUTO_MOUSE_DEFAULT_LAYER;
    }
    return kb_settings_phenom_devices.auto_mouse_layer;
}

void set_split_pointing_auto_mouse_layer(uint8_t layer) {
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
        layer = AUTO_MOUSE_DEFAULT_LAYER;
    }
    new_config.auto_mouse_layer = layer;
    kb_settings_split_pointing_update(new_config);
}

uint8_t get_split_pointing_auto_mouse_timeout_idx(void) {
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    if (kb_settings_phenom_devices.auto_mouse_timeout_idx >= PHENOM_AUTO_MOUSE_TIMEOUT_IDX_COUNT) {
        return PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX;
    }
    return kb_settings_phenom_devices.auto_mouse_timeout_idx;
#else
    return PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX;
#endif
}

void set_split_pointing_auto_mouse_timeout_idx(uint8_t idx) {
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    if (idx >= PHENOM_AUTO_MOUSE_TIMEOUT_IDX_COUNT) {
        idx = PHENOM_AUTO_MOUSE_TIMEOUT_DEFAULT_IDX;
    }
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.auto_mouse_timeout_idx      = idx;
    kb_settings_split_pointing_update(new_config);
#endif
}

bool get_split_pointing_side_invert_text(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return false;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    return kb_settings_phenom_devices.invert_text[side] != 0;
#else
    return false;
#endif
}

void set_split_pointing_side_invert_text(split_pointing_side_t side, bool invert_text) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.invert_text[side]           = invert_text;
    kb_settings_split_pointing_update(new_config);
#endif
}

#ifdef EH_SPLIT_POINTING_INVERT_AXES
bool get_split_pointing_side_invert_text_h(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return false;
    }
    return kb_settings_phenom_devices.invert_text_h[side] != 0;
}

void set_split_pointing_side_invert_text_h(split_pointing_side_t side, bool invert_text) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.invert_text_h[side]         = invert_text;
    kb_settings_split_pointing_update(new_config);
}
#endif

bool get_split_pointing_side_sticky_mode(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return true;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    return kb_settings_phenom_devices.sticky_mode[side] != 0;
#else
    return get_sticky_mode();
#endif
}

void set_split_pointing_side_sticky_mode(split_pointing_side_t side, bool sticky_mode) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.sticky_mode[side]           = sticky_mode;
    kb_settings_split_pointing_update(new_config);
#else
    set_sticky_mode(sticky_mode);
#endif
}

bool get_split_pointing_side_auto_mouse_mode_enabled(split_pointing_side_t side, pointing_mode_t mode) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return false;
    }
    uint8_t mask = phenom_auto_mouse_mode_mask(mode);
    if (mask == 0) {
        return false;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    return (kb_settings_phenom_devices.side_auto_mouse_enable[side] & mask) != 0;
#else
    return (kb_settings_phenom_devices.auto_mouse_enable & mask) != 0;
#endif
}

void set_split_pointing_side_auto_mouse_mode_enabled(split_pointing_side_t side, pointing_mode_t mode, bool enabled) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    uint8_t mask = phenom_auto_mouse_mode_mask(mode);
    if (mask == 0) {
        return;
    }

    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    if (enabled) {
        new_config.side_auto_mouse_enable[side] |= mask;
    } else {
        new_config.side_auto_mouse_enable[side] &= ~mask;
    }
#else
    if (enabled) {
        new_config.auto_mouse_enable |= mask;
    } else {
        new_config.auto_mouse_enable &= ~mask;
    }
#endif
    kb_settings_split_pointing_update(new_config);
}

uint8_t get_split_pointing_side_auto_mouse_layer(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return AUTO_MOUSE_DEFAULT_LAYER;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    if (kb_settings_phenom_devices.side_auto_mouse_layer[side] >= DYNAMIC_KEYMAP_LAYER_COUNT) {
        return AUTO_MOUSE_DEFAULT_LAYER;
    }
    return kb_settings_phenom_devices.side_auto_mouse_layer[side];
#else
    return get_split_pointing_auto_mouse_layer();
#endif
}

void set_split_pointing_side_auto_mouse_layer(split_pointing_side_t side, uint8_t layer) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
        layer = AUTO_MOUSE_DEFAULT_LAYER;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.side_auto_mouse_layer[side] = layer;
    kb_settings_split_pointing_update(new_config);
#else
    set_split_pointing_auto_mouse_layer(layer);
#endif
}

uint8_t get_split_pointing_side_encoder_interval_idx(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    uint8_t idx = kb_settings_phenom_devices.encoder_interval_idx[side];
    return idx < PHENOM_ENCODER_INTERVAL_IDX_COUNT ? idx : PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
#else
    return PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
#endif
}

void set_split_pointing_side_encoder_interval_idx(split_pointing_side_t side, uint8_t idx) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
    if (idx >= PHENOM_ENCODER_INTERVAL_IDX_COUNT) {
        idx = PHENOM_ENCODER_INTERVAL_DEFAULT_IDX;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.encoder_interval_idx[side]  = idx;
    kb_settings_split_pointing_update(new_config);
#endif
}

uint8_t get_split_pointing_side_encoder_interval_ms(split_pointing_side_t side) {
    return phenom_encoder_interval_ms_table[get_split_pointing_side_encoder_interval_idx(side)];
}

bool get_split_pointing_side_touch_gestures(split_pointing_side_t side) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return PHENOM_TOUCH_GESTURES_DEFAULT;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    return kb_settings_phenom_devices.touch_gestures[side] != 0;
#else
    return PHENOM_TOUCH_GESTURES_DEFAULT;
#endif
}

void set_split_pointing_side_touch_gestures(split_pointing_side_t side, bool enabled) {
    if (side >= SPLIT_POINTING_SIDE_COUNT) {
        return;
    }
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    kb_settings_split_pointing_t new_config = kb_settings_phenom_devices;
    new_config.touch_gestures[side]        = enabled;
    kb_settings_split_pointing_update(new_config);
#endif
}

static uint16_t apply_pointing_cpi(uint16_t cpi) {
#if defined(POINTING_DEVICE_ENABLE) && !defined(POINTING_DEVICE_DRIVER_analog_joystick)
    uint16_t actual_dpi = cpi;
    for (int i = 0; i < 5; ++i) { // bug in touchpad driver
        pointing_device_set_cpi(cpi);
        actual_dpi = pointing_device_get_cpi();
        dprintf("set dpi=%d actual dpi=%d\n", cpi, actual_dpi);
        if (actual_dpi == cpi) break;
    }
    return actual_dpi;
#else
    return cpi;
#endif
}

void kb_settings_pointing_update(kb_settings_pointing_t new_config) {
    if (new_config.dpi != kb_settings_pointing.dpi) {
        new_config.dpi = apply_pointing_cpi(new_config.dpi);
    }
    if (new_config.raw != kb_settings_pointing.raw) {
        kb_settings_pointing = new_config;
        dprintf("dpi=%d s1=%d s2=%d s3=%d acc=%d inv=%d\n", kb_settings_pointing.dpi, kb_settings_pointing.sens[1], kb_settings_pointing.sens[2], kb_settings_pointing.sens[3], kb_settings_pointing.acceleration, kb_settings_pointing.invert_scroll);
        eeconfig_update_kb_datablock(&kb_settings_pointing, KB_SETTINGS_POINTING_OFFSET, sizeof(kb_settings_pointing_t));
    }
}

void kb_settings_pointing_init(void) {
    eeconfig_read_kb_datablock(&kb_settings_pointing, KB_SETTINGS_POINTING_OFFSET, sizeof(kb_settings_pointing_t));
    if (config_all_zero(&kb_settings_pointing, sizeof(kb_settings_pointing)) || kb_settings_pointing.dpi < 100 || kb_settings_pointing.dpi > 10000) {
        kb_settings_pointing = get_settings_pointing_default();
        eeconfig_update_kb_datablock(&kb_settings_pointing, KB_SETTINGS_POINTING_OFFSET, sizeof(kb_settings_pointing_t));
    }
    pointing_mode = kb_settings_pointing.mode;
    {
        uint16_t actual_dpi = apply_pointing_cpi(kb_settings_pointing.dpi);
        if (actual_dpi != kb_settings_pointing.dpi) {
            kb_settings_pointing.dpi = actual_dpi;
            eeconfig_update_kb_datablock(&kb_settings_pointing, KB_SETTINGS_POINTING_OFFSET, sizeof(kb_settings_pointing_t));
        }
    }
    dprintf("dpi=%d s1=%d s2=%d s3=%d acc=%d inv=%d\n", kb_settings_pointing.dpi, kb_settings_pointing.sens[1], kb_settings_pointing.sens[2], kb_settings_pointing.sens[3], kb_settings_pointing.acceleration, kb_settings_pointing.invert_scroll);
}

void kb_settings_pointing_reset(void) {
    kb_settings_pointing_update(get_settings_pointing_default());
}

kb_settings_pointing_t get_settings_pointing(void) {
    return kb_settings_pointing;
}

void set_settings_pointing(kb_settings_pointing_t config) {
    kb_settings_pointing_update(config);
    pointing_mode = kb_settings_pointing.mode;
    apply_pointing_cpi(kb_settings_pointing.dpi);
    apply_auto_mouse_settings();
}

void set_cpi(uint16_t cpi) {
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

void set_invert_scroll_h(bool invert) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.invert_scroll_h        = invert;
    kb_settings_pointing_update(new_config);
}

bool get_invert_scroll_h(void) {
    return kb_settings_pointing.invert_scroll_h;
}

void set_invert_text(bool invert) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.invert_text            = invert;
    kb_settings_pointing_update(new_config);
}

bool get_invert_text(void) {
    return kb_settings_pointing.invert_text;
}

void set_invert_text_h(bool invert) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.invert_text_h          = invert;
    kb_settings_pointing_update(new_config);
}

bool get_invert_text_h(void) {
    return kb_settings_pointing.invert_text_h;
}

uint8_t get_pointing_encoder_interval_idx(void) {
    uint8_t slot = kb_settings_pointing.encoder_interval_slot;
    if (slot == 0) {
        return MODULE_ENCODER_INTERVAL_DEFAULT_IDX;
    }
    slot--;
    return slot < MODULE_ENCODER_INTERVAL_IDX_COUNT ? slot : MODULE_ENCODER_INTERVAL_DEFAULT_IDX;
}

void set_pointing_encoder_interval_idx(uint8_t idx) {
    if (idx >= MODULE_ENCODER_INTERVAL_IDX_COUNT) {
        idx = MODULE_ENCODER_INTERVAL_DEFAULT_IDX;
    }
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.encoder_interval_slot  = idx + 1;
    kb_settings_pointing_update(new_config);
}

uint8_t get_pointing_encoder_interval_ms(void) {
    return module_encoder_interval_ms_table[get_pointing_encoder_interval_idx()];
}

bool get_pointing_touch_gestures(void) {
    return !kb_settings_pointing.touch_gestures_disabled;
}

void set_pointing_touch_gestures(bool enabled) {
    kb_settings_pointing_t new_config    = kb_settings_pointing;
    new_config.touch_gestures_disabled   = !enabled;
    kb_settings_pointing_update(new_config);
#ifdef POINTING_DEVICE_DRIVER_azoteq_iqs5xx
    azoteq_iqs5xx_set_gesture_config(enabled, false);
#endif
}

#if defined(POINTING_DEVICE_DRIVER_azoteq_iqs5xx) && !defined(EH_KEYBOARD_SPLIT_POINTING_V2)
bool azoteq_iqs5xx_gestures_enabled(void) {
    return get_pointing_touch_gestures();
}
#endif

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
static bool is_mouse_active_override_enabled = false;
static bool is_mouse_active_override         = false;
static uint16_t mouse_motion_timer            = 0;

void set_pointing_auto_mouse_override(bool enabled, bool active) {
    is_mouse_active_override_enabled = enabled;
    is_mouse_active_override         = active;
}

bool auto_mouse_activation(report_mouse_t mouse_report) {
    return is_mouse_active_override_enabled ? is_mouse_active_override : is_mouse_active;
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

pointing_mode_t get_pointing_mode(void) {
    return kb_settings_pointing.mode;
}

void set_pointing_mode(pointing_mode_t mode) {
    kb_settings_pointing_t new_config = kb_settings_pointing;
    new_config.mode                   = mode;
    kb_settings_pointing_update(new_config);
    if (mode != pointing_mode) {
        pointing_mode = mode;
        if (is_hid_active()) {
            hid_send_pointing_mode(mode);
#if !defined(KEYBOARD_ergohaven_k03pro_rev1) && !defined(KEYBOARD_ergohaven_k03pro_rev2)
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
#endif
        }
    }
}

bool process_record_pointing(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
        case EH_SCR:
        case EH_TXT:
        case EH_SNP: {
            static uint16_t        press_timer                 = 0;
            static bool            prev_override_active_left   = false;
            static bool            prev_override_active_right  = false;
            static pointing_mode_t prev_override_mode_left     = POINTING_MODE_NORMAL;
            static pointing_mode_t prev_override_mode_right    = POINTING_MODE_NORMAL;
            const pointing_mode_t  NEW_MODE                   = POINTING_MODE_SNIPER + (keycode - EH_SNP);

            if (record->event.pressed) {
                prev_override_active_left  = split_pointing_mode_override_active[SPLIT_POINTING_SIDE_LEFT];
                prev_override_active_right = split_pointing_mode_override_active[SPLIT_POINTING_SIDE_RIGHT];
                prev_override_mode_left    = split_pointing_mode_override[SPLIT_POINTING_SIDE_LEFT];
                prev_override_mode_right   = split_pointing_mode_override[SPLIT_POINTING_SIDE_RIGHT];
                set_split_pointing_side_mode_override(SPLIT_POINTING_SIDE_LEFT, NEW_MODE);
                set_split_pointing_side_mode_override(SPLIT_POINTING_SIDE_RIGHT, NEW_MODE);
                press_timer = timer_read();
            } else {
                bool tapped = timer_elapsed(press_timer) < get_tapping_term(keycode, record);
                if (get_split_pointing_side_sticky_mode(SPLIT_POINTING_SIDE_LEFT) && tapped) {
                    restore_split_pointing_side_mode_override(SPLIT_POINTING_SIDE_LEFT, prev_override_active_left, prev_override_mode_left);
                    toggle_split_pointing_side_sticky_override(SPLIT_POINTING_SIDE_LEFT, NEW_MODE);
                } else {
                    restore_split_pointing_side_mode_override(SPLIT_POINTING_SIDE_LEFT, prev_override_active_left, prev_override_mode_left);
                }
                if (get_split_pointing_side_sticky_mode(SPLIT_POINTING_SIDE_RIGHT) && tapped) {
                    restore_split_pointing_side_mode_override(SPLIT_POINTING_SIDE_RIGHT, prev_override_active_right, prev_override_mode_right);
                    toggle_split_pointing_side_sticky_override(SPLIT_POINTING_SIDE_RIGHT, NEW_MODE);
                } else {
                    restore_split_pointing_side_mode_override(SPLIT_POINTING_SIDE_RIGHT, prev_override_active_right, prev_override_mode_right);
                }
            }
            return false;
        }
        case EH_L_SCR:
        case EH_L_TXT:
        case EH_L_SNP:
        case EH_USR1:
        case EH_USR2:
        case EH_USR3: {
            static uint16_t        press_timer_left             = 0;
            static uint16_t        press_timer_right            = 0;
            static bool            prev_override_active_left    = false;
            static bool            prev_override_active_right   = false;
            static pointing_mode_t prev_override_mode_left      = POINTING_MODE_NORMAL;
            static pointing_mode_t prev_override_mode_right     = POINTING_MODE_NORMAL;

            const bool left_side                    = keycode == EH_L_SNP || keycode == EH_L_SCR || keycode == EH_L_TXT;
            const split_pointing_side_t side        = left_side ? SPLIT_POINTING_SIDE_LEFT : SPLIT_POINTING_SIDE_RIGHT;
            uint16_t *const press_timer             = left_side ? &press_timer_left : &press_timer_right;
            bool *const prev_override_active        = left_side ? &prev_override_active_left : &prev_override_active_right;
            pointing_mode_t *const prev_override_mode = left_side ? &prev_override_mode_left : &prev_override_mode_right;
            const pointing_mode_t NEW_MODE          = POINTING_MODE_SNIPER + (left_side ? (keycode - EH_L_SNP) : (keycode - EH_USR1));

            if (record->event.pressed) {
                *prev_override_active = split_pointing_mode_override_active[side];
                *prev_override_mode   = split_pointing_mode_override[side];
                set_split_pointing_side_mode_override(side, NEW_MODE);
                *press_timer = timer_read();
            } else {
                if (get_split_pointing_side_sticky_mode(side) && timer_elapsed(*press_timer) < get_tapping_term(keycode, record)) {
                    restore_split_pointing_side_mode_override(side, *prev_override_active, *prev_override_mode);
                    toggle_split_pointing_side_sticky_override(side, NEW_MODE);
                } else {
                    restore_split_pointing_side_mode_override(side, *prev_override_active, *prev_override_mode);
                }
            }
            return false;
        }
#else
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
#endif

#ifndef EH_KEYBOARD_SPLIT_POINTING_V2
        case EH_LED_BL:
            if (record->event.pressed) set_led_blinks(!get_led_blinks());
            return false;
#endif
    }

    return true;
}

report_mouse_t pointing_device_task_user(report_mouse_t mrpt) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (!is_mouse_active_override_enabled) {
        bool mouse_report_active = abs(mrpt.x) >= 1 || abs(mrpt.y) >= 1 || abs(mrpt.v) >= 1 || abs(mrpt.h) >= 1 || mrpt.buttons;
        // Trackballs can emit zero-delta reports between motion samples.
        if (mouse_report_active) {
            is_mouse_active    = true;
            mouse_motion_timer = timer_read();
        } else if (is_mouse_active && timer_elapsed(mouse_motion_timer) >= PHENOM_AUTO_MOUSE_MOTION_GRACE_MS) {
            is_mouse_active = false;
        }
    }
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
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
    static uint8_t  text_button_sector       = 0;
    static uint8_t  text_button_candidate    = 0;
    static uint8_t  text_button_candidate_ct = 0;
    static bool     text_button_armed        = true;
    static bool     text_button_hold_active  = false;
    static uint16_t text_button_hold_kc      = KC_NO;
    static uint32_t text_button_hold_timer   = 0;
    static int16_t  text_button_motion_x     = 0;
    static int16_t  text_button_motion_y     = 0;
    static uint32_t text_button_idle_timer   = 0;
#endif

    // hpd-text-mode-v0.0.4: default Text mode to arrow keys when no custom text layer mapping exists.
    static uint16_t kc_up    = KC_UP;
    static uint16_t kc_down  = KC_DOWN;
    static uint16_t kc_left  = KC_LEFT;
    static uint16_t kc_right = KC_RIGHT;
#ifdef EH_HPD_LAYERS
    uint8_t layer = get_current_layer();
    kc_up         = dynamic_keymap_get_keycode(layer, 11, 0);
    kc_left       = dynamic_keymap_get_keycode(layer, 11, 1);
    kc_right      = dynamic_keymap_get_keycode(layer, 11, 2);
    kc_down       = dynamic_keymap_get_keycode(layer, 11, 3);
#    ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
    pmode = get_settings_pointing().mode == POINTING_MODE_TEXT ? POINTING_MODE_TEXT : POINTING_MODE_NORMAL;
#    else
    if ((kc_up != KC_NO && kc_up != KC_TRNS) || (kc_down != KC_NO && kc_down != KC_TRNS) || (kc_left != KC_NO && kc_left != KC_TRNS) || (kc_right != KC_NO && kc_right != KC_TRNS)) {
        pmode = POINTING_MODE_TEXT;
    }
#    endif
#endif
    kc_up    = pointing_text_keycode_or_default(kc_up, KC_UP);
    kc_down  = pointing_text_keycode_or_default(kc_down, KC_DOWN);
    kc_left  = pointing_text_keycode_or_default(kc_left, KC_LEFT);
    kc_right = pointing_text_keycode_or_default(kc_right, KC_RIGHT);

    if (pmode != POINTING_MODE_NORMAL) {
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
        if (pmode == POINTING_MODE_TEXT) {
            const int16_t enter_threshold = 6;
            const uint16_t idle_release_ms = 50;
            const uint8_t stable_samples  = 2;
            const int8_t sx               = mrpt.x;
            const int8_t sy               = mrpt.y;
            uint8_t detected_sector       = 0;

            mrpt.x = 0;
            mrpt.y = 0;
            mrpt.h = 0;
            mrpt.v = 0;

            if (sx == 0 && sy == 0) {
                if (text_button_idle_timer == 0) {
                    text_button_idle_timer = timer_read32();
                }
                if (timer_elapsed32(text_button_idle_timer) >= idle_release_ms) {
                    if (text_button_hold_active && text_button_hold_kc != KC_NO) {
                        unregister_code16(text_button_hold_kc);
                    }
                    text_button_sector       = 0;
                    text_button_candidate    = 0;
                    text_button_candidate_ct = 0;
                    text_button_armed        = true;
                    text_button_hold_active  = false;
                    text_button_hold_kc      = KC_NO;
                    text_button_hold_timer   = 0;
                    text_button_motion_x     = 0;
                    text_button_motion_y     = 0;
                    text_button_idle_timer   = 0;
                    accumulated_h            = 0;
                    accumulated_v            = 0;
                }
                return mrpt;
            }

            text_button_idle_timer = timer_read32();
            text_button_motion_x += sx;
            text_button_motion_y += sy;

            if (abs(text_button_motion_x) > abs(text_button_motion_y) && abs(text_button_motion_x) >= enter_threshold) {
                detected_sector = text_button_motion_x > 0 ? 1 : 2;
            } else if (abs(text_button_motion_y) > abs(text_button_motion_x) && abs(text_button_motion_y) >= enter_threshold) {
                detected_sector = text_button_motion_y > 0 ? 4 : 3;
            }

            if (get_invert_text()) {
                if (detected_sector == 3) {
                    detected_sector = 4;
                } else if (detected_sector == 4) {
                    detected_sector = 3;
                }
            }
            if (get_invert_text_h()) {
                if (detected_sector == 1) {
                    detected_sector = 2;
                } else if (detected_sector == 2) {
                    detected_sector = 1;
                }
            }

            if (detected_sector == 0) {
                return mrpt;
            }

            text_button_motion_x = 0;
            text_button_motion_y = 0;

            if (!text_button_armed && detected_sector == text_button_sector) {
                text_button_candidate    = 0;
                text_button_candidate_ct = 0;
                if (!text_button_hold_active && text_button_hold_kc != KC_NO && timer_elapsed32(text_button_hold_timer) >= 500) {
                    register_code16(text_button_hold_kc);
                    text_button_hold_active = true;
                }
                return mrpt;
            }

            if (text_button_candidate != detected_sector) {
                text_button_candidate    = detected_sector;
                text_button_candidate_ct = 1;
                return mrpt;
            }

            if (text_button_candidate_ct < stable_samples) {
                text_button_candidate_ct++;
                return mrpt;
            }

            if (!text_button_armed && text_button_hold_active && text_button_hold_kc != KC_NO) {
                unregister_code16(text_button_hold_kc);
            }

            text_button_sector       = detected_sector;
            text_button_candidate    = 0;
            text_button_candidate_ct = 0;
            text_button_armed        = false;
            text_button_hold_active  = false;
            text_button_hold_timer   = timer_read32();
            text_button_idle_timer   = 0;

            switch (text_button_sector) {
                case 1:
                    text_button_hold_kc = kc_right;
                    break;
                case 2:
                    text_button_hold_kc = kc_left;
                    break;
                case 3:
                    text_button_hold_kc = kc_up;
                    break;
                case 4:
                    text_button_hold_kc = kc_down;
                    break;
                default:
                    text_button_hold_kc = KC_NO;
                    break;
            }

            if (text_button_hold_kc != KC_NO) {
                tap_code16(text_button_hold_kc);
            }

            return mrpt;
        }
#endif
        int32_t divisor = kb_settings_pointing.sens[MIN(pmode, POINTING_MODE_TEXT)];

        accumulated_h += mrpt.x;
        accumulated_v += mrpt.y;

        int shift_x = accumulated_h / divisor;
        int shift_y = accumulated_v / divisor;

        accumulated_h -= shift_x * divisor;
        accumulated_v -= shift_y * divisor;

        mrpt.x = 0;
        mrpt.y = 0;
        mrpt.h = 0;
        mrpt.v = 0;

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
                kc_up    = pointing_text_keycode_or_default(kc_up, KC_UP);
                kc_down  = pointing_text_keycode_or_default(kc_down, KC_DOWN);
                kc_left  = pointing_text_keycode_or_default(kc_left, KC_LEFT);
                kc_right = pointing_text_keycode_or_default(kc_right, KC_RIGHT);
                if (abs(shift_x) > abs(shift_y)) {
                    shift_y       = 0;
                    accumulated_v = 0;
                } else if (abs(shift_x) < abs(shift_y)) {
                    shift_x       = 0;
                    accumulated_h = 0;
                }

                if (get_invert_text()) {
                    shift_y = -shift_y;
                }
                if (get_invert_text_h()) {
                    shift_x = -shift_x;
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
#ifdef EH_QMK_SETTINGS_SIMPLE_JOYSTICK
        if (text_button_hold_active && text_button_hold_kc != KC_NO) {
            unregister_code16(text_button_hold_kc);
        }
        text_button_sector       = 0;
        text_button_candidate    = 0;
        text_button_candidate_ct = 0;
        text_button_armed        = true;
        text_button_hold_active  = false;
        text_button_hold_kc      = KC_NO;
        text_button_hold_timer   = 0;
        text_button_motion_x     = 0;
        text_button_motion_y     = 0;
        text_button_idle_timer   = 0;
#endif
        accumulated_h = 0;
        accumulated_v = 0;
    }

    if (get_invert_scroll_h()) {
        mrpt.h = -mrpt.h;
    }
    if (get_invert_scroll()) {
        mrpt.v = -mrpt.v;
    }

    return mrpt;
}
