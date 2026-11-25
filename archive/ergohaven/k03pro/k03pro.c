#include "display.h"
#include "transactions.h"
#include "ergohaven.h"
#include "src/eh_ruen.h"
#include "ergohaven_rgb.h"
#include "src/display/eh_display.h"
#include "src/eh_pointing.h"
#include QMK_KEYBOARD_H

// clang-format off
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                       'R', 'R', 'R', 'R', 'R', 'R',
          'L', 'L', '*', '*', '*', 'L',   'R', '*', '*', '*', 'R', 'R'
);
// clang-format on

typedef struct {
    uint8_t brightness;
    uint8_t timeout_mins;
    uint8_t valid;
} kb_settings_lcd_t;

typedef struct {
    kb_settings_pointing_t     pointing;
    kb_settings_split_pointing_t devices;
} k03pro_pointing_sync_t;

typedef struct {
    kb_settings_led_colors_t leds;
} k03pro_led_sync_t;

typedef struct {
    uint8_t brightness;
    uint8_t timeout_mins;
} k03pro_lcd_sync_t;

typedef struct {
    uint8_t layer;
} k03pro_active_layer_sync_t;

static_assert(KB_SETTINGS_LCD_SIZE == sizeof(kb_settings_lcd_t), "Invalid KB_SETTINGS_LCD_SIZE");

#define K03PRO_LCD_SETTINGS_VALID 0xA5

typedef union {
    uint8_t raw;
    struct {
        uint8_t lang : 1;
        bool    mac : 1;
        bool    caps_word : 1;
    };
} display_config_t;

static kb_settings_lcd_t kb_settings_lcd;
static k03pro_lcd_sync_t split_lcd_sync = {.brightness = BACKLIGHT_LEVELS, .timeout_mins = 10};
static bool              split_lcd_sync_valid = false;
display_config_t         display_config;

bool is_display_side(void);
bool is_touch_side(void);

static kb_settings_lcd_t get_lcd_settings_default(void) {
    kb_settings_lcd_t config = {
        .brightness   = BACKLIGHT_LEVELS,
        .timeout_mins = 10,
        .valid        = K03PRO_LCD_SETTINGS_VALID,
    };
    return config;
}

static kb_settings_lcd_t sanitize_lcd_settings(kb_settings_lcd_t config) {
    if (config.valid != K03PRO_LCD_SETTINGS_VALID) {
        return get_lcd_settings_default();
    }
    if (config.brightness > BACKLIGHT_LEVELS) {
        config.brightness = BACKLIGHT_LEVELS;
    }
    return config;
}

static void update_lcd_settings(kb_settings_lcd_t config) {
    config = sanitize_lcd_settings(config);
    if (memcmp(&config, &kb_settings_lcd, sizeof(config)) != 0) {
        kb_settings_lcd = config;
        eeconfig_update_kb_datablock(&kb_settings_lcd, KB_SETTINGS_LCD_OFFSET, sizeof(kb_settings_lcd));
    }
}

void kb_settings_lcd_init(void) {
    eeconfig_read_kb_datablock(&kb_settings_lcd, KB_SETTINGS_LCD_OFFSET, sizeof(kb_settings_lcd));
    kb_settings_lcd = sanitize_lcd_settings(kb_settings_lcd);
}

void kb_settings_lcd_reset(void) {
    update_lcd_settings(get_lcd_settings_default());
}

uint8_t get_lcd_brightness(void) {
    return kb_settings_lcd.brightness;
}

void set_lcd_brightness(uint8_t brightness) {
    kb_settings_lcd_t config = kb_settings_lcd;
    if (brightness > BACKLIGHT_LEVELS) {
        brightness = BACKLIGHT_LEVELS;
    }
    config.brightness = brightness;
    update_lcd_settings(config);
    backlight_level(brightness);
}

uint8_t get_lcd_timeout_mins(void) {
    return kb_settings_lcd.timeout_mins;
}

void set_lcd_timeout_mins(uint8_t timeout_mins) {
    kb_settings_lcd_t config = kb_settings_lcd;
    config.timeout_mins      = timeout_mins;
    update_lcd_settings(config);
}

uint32_t get_lcd_timeout_ms(void) {
    uint8_t timeout_mins = get_lcd_timeout_mins();
    if (timeout_mins == 0) {
        return 0;
    }
    return (uint32_t)timeout_mins * 60 * 1000;
}

uint8_t get_split_lcd_brightness(void) {
    if (is_keyboard_master() || !split_lcd_sync_valid) {
        return get_lcd_brightness();
    }
    return split_lcd_sync.brightness;
}

uint32_t get_split_lcd_timeout_ms(void) {
    uint8_t timeout_mins;
    if (is_keyboard_master() || !split_lcd_sync_valid) {
        timeout_mins = get_lcd_timeout_mins();
    } else {
        timeout_mins = split_lcd_sync.timeout_mins;
    }
    if (timeout_mins == 0) {
        return 0;
    }
    return (uint32_t)timeout_mins * 60 * 1000;
}

uint8_t split_get_lang(void) {
    return is_keyboard_master() ? get_cur_lang() : display_config.lang;
}

bool split_get_mac(void) {
    return is_keyboard_master() ? keymap_config.swap_lctl_lgui : display_config.mac;
}

bool split_get_caps_word(void) {
    return is_keyboard_master() ? is_caps_word_on() : display_config.caps_word;
}

void sync_display(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&display_config, in_data, sizeof(display_config_t));
}

void sync_lcd(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    if (in_buflen == sizeof(k03pro_lcd_sync_t) && in_data != NULL) {
        memcpy(&split_lcd_sync, in_data, sizeof(k03pro_lcd_sync_t));
        split_lcd_sync_valid = true;
        set_lcd_brightness(split_lcd_sync.brightness);
        set_lcd_timeout_mins(split_lcd_sync.timeout_mins);
    }
}

void sync_pointing(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    if (in_buflen == sizeof(k03pro_pointing_sync_t) && in_data != NULL) {
        k03pro_pointing_sync_t value;
        memcpy(&value, in_data, sizeof(value));
        value.pointing.led_blinks = false;
        set_settings_pointing(value.pointing);
        set_split_pointing_settings(value.devices);
    }
}

void sync_leds(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    if (in_buflen == sizeof(k03pro_led_sync_t) && in_data != NULL) {
        k03pro_led_sync_t value;
        memcpy(&value, in_data, sizeof(value));
        set_settings_led_colors(value.leds);
    }
}

void sync_active_layer(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    if (in_buflen == sizeof(k03pro_active_layer_sync_t) && in_data != NULL) {
        k03pro_active_layer_sync_t value;
        memcpy(&value, in_data, sizeof(value));
        if (value.layer <= _FIFTEEN) {
            layer_state_set_rgb((layer_state_t)1 << value.layer);
        }
    }
}

bool is_display_side(void) {
#ifdef K03_DISPLAY_RIGHT
    return !is_keyboard_left();
#endif
#ifdef K03_DISPLAY_LEFT
    return is_keyboard_left();
#endif
    return false;
}

bool is_touch_side(void) {
    return !is_display_side();
}

void housekeeping_task_user(void) {
    if (is_display_enabled() && is_display_side()) {
        static uint8_t last_display_brightness = 0xFF;
        uint8_t        current_brightness      = get_split_lcd_brightness();
        if (last_display_brightness != current_brightness) {
            backlight_level_noeeprom(current_brightness);
            last_display_brightness = current_brightness;
        }
        display_housekeeping_task();
    }

    if (is_touch_side() && is_keyboard_master()) {
        static bool is_display_on = true;

        uint32_t activity_elapsed = last_input_activity_elapsed();
        uint32_t timeout_ms       = get_split_lcd_timeout_ms();
        bool     timed_out        = timeout_ms != 0 && activity_elapsed > timeout_ms;
        if (timed_out) {
            if (is_display_on) {
                backlight_level_noeeprom(0);
                is_display_on = false;
            }
        } else {
            if (!is_display_on) {
                backlight_level_noeeprom(get_split_lcd_brightness());
                is_display_on = true;
            }
        }
    }

    if (is_keyboard_master()) {
        {
            static uint32_t                 last_sync         = 0;
            static display_config_t         slave_display     = {.raw = 0};
            static k03pro_lcd_sync_t        slave_lcd         = {.brightness = 0, .timeout_mins = 0};
            static k03pro_pointing_sync_t   slave_pointing;
            static k03pro_led_sync_t        slave_leds        = {.leds = {{0}, 0, 0}};
            static k03pro_active_layer_sync_t slave_active_layer = {.layer = 0xFF};

            if (last_sync == 0 || timer_elapsed32(last_sync) > 500) {
                display_config.lang      = split_get_lang();
                display_config.mac       = split_get_mac();
                display_config.caps_word = split_get_caps_word();

                if (slave_display.raw != display_config.raw) {
                    if (transaction_rpc_send(RPC_SYNC_DISPLAY, sizeof(display_config_t), &display_config)) {
                        slave_display.raw = display_config.raw;
                        dprintf("sync display settings %x\n", display_config.raw);
                    }
                }

                k03pro_lcd_sync_t lcd_sync = {
                    .brightness   = get_lcd_brightness(),
                    .timeout_mins = get_lcd_timeout_mins(),
                };
                if (memcmp(&slave_lcd, &lcd_sync, sizeof(lcd_sync)) != 0) {
                    if (transaction_rpc_send(RPC_SYNC_K03PRO_LCD, sizeof(k03pro_lcd_sync_t), &lcd_sync)) {
                        slave_lcd = lcd_sync;
                        dprintf("sync lcd settings b=%u t=%u\n", lcd_sync.brightness, lcd_sync.timeout_mins);
                    }
                }

                k03pro_pointing_sync_t pointing_sync = {
                    .pointing = get_settings_pointing(),
                    .devices  = get_split_pointing_settings(),
                };
                pointing_sync.pointing.led_blinks = false;
                if (memcmp(&slave_pointing, &pointing_sync, sizeof(pointing_sync)) != 0) {
                    if (transaction_rpc_send(RPC_SYNC_K03PRO_POINTING, sizeof(k03pro_pointing_sync_t), &pointing_sync)) {
                        slave_pointing = pointing_sync;
                        dprintf("sync pointing settings raw=%llx auto=%u layer=%u\n", (unsigned long long)pointing_sync.pointing.raw, pointing_sync.devices.auto_mouse_enable, pointing_sync.devices.auto_mouse_layer);
                    }
                }

                k03pro_led_sync_t led_sync = {
                    .leds = get_settings_led_colors(),
                };
                if (memcmp(&slave_leds.leds, &led_sync.leds, sizeof(led_sync.leds)) != 0) {
                    if (transaction_rpc_send(RPC_SYNC_K03PRO_LED_COLORS, sizeof(k03pro_led_sync_t), &led_sync)) {
                        slave_leds = led_sync;
                        dprintf("sync led settings\n");
                    }
                }

                k03pro_active_layer_sync_t active_layer_sync = {
                    .layer = get_highest_layer(layer_state | default_layer_state),
                };
                if (slave_active_layer.layer != active_layer_sync.layer) {
                    if (transaction_rpc_send(RPC_SYNC_K03PRO_ACTIVE_LAYER, sizeof(k03pro_active_layer_sync_t), &active_layer_sync)) {
                        slave_active_layer = active_layer_sync;
                        dprintf("sync active layer %u\n", active_layer_sync.layer);
                    }
                }

                last_sync = timer_read32();
            }
        }
    }
}

void keyboard_post_init_user(void) {
    if (is_display_side()) {
        display_init_kb();
        backlight_level_noeeprom(get_split_lcd_brightness());
    }

    set_led_blinks(false);
    {
        kb_settings_pointing_t pointing = get_settings_pointing();
        pointing.led_blinks             = false;
        set_settings_pointing(pointing);
    }
    transaction_register_rpc(RPC_SYNC_DISPLAY, sync_display);
    transaction_register_rpc(RPC_SYNC_K03PRO_LCD, sync_lcd);
    transaction_register_rpc(RPC_SYNC_K03PRO_POINTING, sync_pointing);
    transaction_register_rpc(RPC_SYNC_K03PRO_LED_COLORS, sync_leds);
    transaction_register_rpc(RPC_SYNC_K03PRO_ACTIVE_LAYER, sync_active_layer);
}
