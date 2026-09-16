#include "src/display/eh_date_settings.h"
#include "display.h"
#include "backlight.h"
#include "eeconfig.h"
#include "ergohaven.h"
#include "ergohaven_rgb.h"
#include "src/display/eh_display.h"

#define MACROPAD_RGB_TIMEOUT_DEFAULT_MINS 10
#define MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET (KB_SETTINGS_LED_COLORS_OFFSET + offsetof(kb_settings_led_colors_t, timeout_mins))
#define MACROPAD_DISPLAY_SETTINGS_LEGACY_MAGIC 0xD3
#define MACROPAD_DISPLAY_SETTINGS_V010_MAGIC 0xE0
#define MACROPAD_DISPLAY_SETTINGS_V010_MAGIC_MASK 0xF0
#define MACROPAD_DISPLAY_SETTINGS_V011_MAGIC 0xA0
#define MACROPAD_DISPLAY_SETTINGS_V011_MAGIC_MASK 0xE0
#define MACROPAD_DISPLAY_SETTINGS_V011_STYLE_MASK 0x1F
#define MACROPAD_DISPLAY_SETTINGS_V012_MAGIC 0xC7
#define MACROPAD_DISPLAY_SETTINGS_V018_MAGIC 0xD8
#define MACROPAD_DISPLAY_SETTINGS_V021_MAGIC 0xD9
#define MACROPAD_DISPLAY_SETTINGS_V024_MAGIC 0xDA
#define MACROPAD_DISPLAY_SETTINGS_V026_MAGIC 0xDB
#define MACROPAD_DISPLAY_SETTINGS_MAGIC 0xDC
#define MACROPAD_DISPLAY_BUTTON_STYLE_COUNT 33
#define MACROPAD_DISPLAY_BRIGHTNESS_DEFAULT 100
#define MACROPAD_CLOCK_STYLE_COUNT 10
#define MACROPAD_CLOCK_SIZE_COUNT 4
#define MACROPAD_CLOCK_ALIGNMENT_COUNT 3
#define MACROPAD_CLOCK_DELAY_VARIANT_COUNT 8
#define MACROPAD_CLOCK_DELAY_DEFAULT 7

typedef struct __attribute__((packed)) {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t style;
    uint8_t brightness;
    uint8_t background_red;
    uint8_t background_green;
    uint8_t background_blue;
    uint8_t legacy_magic;
    uint8_t clock_text_red;
    uint8_t clock_text_green;
    uint8_t clock_text_blue;
    uint8_t clock_background_red;
    uint8_t clock_background_green;
    uint8_t clock_background_blue;
    uint8_t clock_style;
    uint8_t clock_size;
    uint8_t clock_alignment;
    uint8_t clock_delay_variant;
    uint8_t lcd_timeout_mins;
    uint8_t magic;
    uint8_t clock_colon_blink;
    uint8_t clock_info_red;
    uint8_t clock_info_green;
    uint8_t clock_info_blue;
    uint8_t clock_background_dim;
    uint8_t clock_visible;
    uint8_t clock_opacity;
    uint8_t clock_info_visible;
    uint8_t clock_info_opacity;
    uint8_t clock_modifiers_visible;
    uint8_t clock_modifiers_red;
    uint8_t clock_modifiers_green;
    uint8_t clock_modifiers_blue;
    uint8_t clock_modifiers_opacity;
} macropad_display_settings_t;

#ifndef EH_DISPLAY_SETTINGS_FLASH
_Static_assert(sizeof(macropad_display_settings_t) == KB_SETTINGS_LCD_SIZE, "macropad display settings size mismatch");
#endif

static uint8_t rgb_timeout_mins = MACROPAD_RGB_TIMEOUT_DEFAULT_MINS;
static macropad_display_settings_t display_settings = {
    .red = 200, .green = 178, .blue = 146, .style = 0, .brightness = MACROPAD_DISPLAY_BRIGHTNESS_DEFAULT,
    .background_red = 0, .background_green = 0, .background_blue = 0, .legacy_magic = MACROPAD_DISPLAY_SETTINGS_V012_MAGIC,
    .clock_text_red = 255, .clock_text_green = 255, .clock_text_blue = 255,
    .clock_background_red = 0, .clock_background_green = 0, .clock_background_blue = 0,
    .clock_style = 0, .clock_size = 2, .clock_alignment = 1, .clock_delay_variant = MACROPAD_CLOCK_DELAY_DEFAULT,
    .lcd_timeout_mins = 10, .magic = MACROPAD_DISPLAY_SETTINGS_MAGIC, .clock_colon_blink = 0,
    .clock_info_red = 255, .clock_info_green = 255, .clock_info_blue = 255,
    .clock_background_dim = 30,
    .clock_visible = 1, .clock_opacity = 100, .clock_info_visible = 1, .clock_info_opacity = 100,
    .clock_modifiers_visible = 1, .clock_modifiers_red = 255, .clock_modifiers_green = 255, .clock_modifiers_blue = 255,
    .clock_modifiers_opacity = 100,
};

void kb_settings_lcd_reset(void);

static void persist_display_settings(void) {
    display_settings.legacy_magic = MACROPAD_DISPLAY_SETTINGS_V012_MAGIC;
    display_settings.magic        = MACROPAD_DISPLAY_SETTINGS_MAGIC;
#ifdef EH_DISPLAY_SETTINGS_FLASH
    eh_extra_settings_write(16, &display_settings, sizeof(display_settings));
#else
    eeconfig_update_kb_datablock(&display_settings, KB_SETTINGS_LCD_OFFSET, sizeof(display_settings));
#endif
    display_apply_accent_color(display_settings.red, display_settings.green, display_settings.blue);
    display_apply_background_color(display_settings.background_red, display_settings.background_green, display_settings.background_blue);
    display_apply_button_style(display_settings.style);
    display_apply_brightness();
    display_apply_clock_settings();
}

static bool lcd_timeout_mins_is_valid(uint8_t timeout_mins) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};
    for (uint8_t i = 0; i < sizeof(timeout_variants); ++i) {
        if (timeout_variants[i] == timeout_mins) return true;
    }
    return false;
}

static void set_clock_info_defaults(void) {
    display_settings.clock_info_red   = 255;
    display_settings.clock_info_green = 255;
    display_settings.clock_info_blue  = 255;
}

static void set_clock_background_defaults(void) {
    display_settings.clock_background_dim = 30;
}

static void set_clock_element_defaults(void) {
    display_settings.clock_visible           = 1;
    display_settings.clock_opacity           = 100;
    display_settings.clock_info_visible      = 1;
    display_settings.clock_info_opacity      = 100;
    display_settings.clock_modifiers_visible = 1;
    display_settings.clock_modifiers_red     = display_settings.clock_info_red;
    display_settings.clock_modifiers_green   = display_settings.clock_info_green;
    display_settings.clock_modifiers_blue    = display_settings.clock_info_blue;
    display_settings.clock_modifiers_opacity = 100;
}

static void set_clock_defaults(void) {
    display_settings.clock_text_red       = 255;
    display_settings.clock_text_green     = 255;
    display_settings.clock_text_blue      = 255;
    display_settings.clock_background_red = display_settings.background_red;
    display_settings.clock_background_green = display_settings.background_green;
    display_settings.clock_background_blue = display_settings.background_blue;
    display_settings.clock_style          = 0;
    display_settings.clock_size           = 2;
    display_settings.clock_alignment      = 1;
    display_settings.clock_delay_variant  = MACROPAD_CLOCK_DELAY_DEFAULT;
    display_settings.lcd_timeout_mins      = 10;
    display_settings.clock_colon_blink     = 0;
    set_clock_info_defaults();
    set_clock_background_defaults();
    set_clock_element_defaults();
}

static bool rgb_timeout_mins_is_valid(uint8_t timeout_mins) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};

    for (uint8_t i = 0; i < sizeof(timeout_variants); ++i) {
        if (timeout_variants[i] == timeout_mins) {
            return true;
        }
    }
    return false;
}

static void update_rgb_timeout_mins(uint8_t timeout_mins) {
    if (rgb_timeout_mins == timeout_mins) {
        return;
    }
    rgb_timeout_mins = timeout_mins;
    eeconfig_update_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
}

void kb_settings_led_colors_init(void) {
    eeconfig_read_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
    if (!rgb_timeout_mins_is_valid(rgb_timeout_mins)) {
        rgb_timeout_mins = MACROPAD_RGB_TIMEOUT_DEFAULT_MINS;
        eeconfig_update_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
    }
}

void kb_settings_led_colors_reset(void) {
    rgb_timeout_mins = MACROPAD_RGB_TIMEOUT_DEFAULT_MINS;
    eeconfig_update_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
}

void kb_settings_lcd_init(void) {
#ifdef EH_DISPLAY_SETTINGS_FLASH
    eh_extra_settings_read(16, &display_settings, sizeof(display_settings));
#else
    eeconfig_read_kb_datablock(&display_settings, KB_SETTINGS_LCD_OFFSET, sizeof(display_settings));
#endif
    if (display_settings.magic == MACROPAD_DISPLAY_SETTINGS_MAGIC &&
        display_settings.style < MACROPAD_DISPLAY_BUTTON_STYLE_COUNT && display_settings.brightness <= 100 &&
        display_settings.clock_style < MACROPAD_CLOCK_STYLE_COUNT && display_settings.clock_size < MACROPAD_CLOCK_SIZE_COUNT &&
        display_settings.clock_alignment < MACROPAD_CLOCK_ALIGNMENT_COUNT &&
        display_settings.clock_delay_variant < MACROPAD_CLOCK_DELAY_VARIANT_COUNT &&
        lcd_timeout_mins_is_valid(display_settings.lcd_timeout_mins) && display_settings.clock_colon_blink <= 1 &&
        display_settings.clock_background_dim <= 100 && display_settings.clock_visible <= 1 && display_settings.clock_opacity <= 100 &&
        display_settings.clock_info_visible <= 1 && display_settings.clock_info_opacity <= 100 &&
        display_settings.clock_modifiers_visible <= 1 && display_settings.clock_modifiers_opacity <= 100) {
        return;
    }

    if (display_settings.magic == MACROPAD_DISPLAY_SETTINGS_V026_MAGIC &&
        display_settings.style < MACROPAD_DISPLAY_BUTTON_STYLE_COUNT && display_settings.brightness <= 100 &&
        display_settings.clock_style < MACROPAD_CLOCK_STYLE_COUNT && display_settings.clock_size < MACROPAD_CLOCK_SIZE_COUNT &&
        display_settings.clock_alignment < MACROPAD_CLOCK_ALIGNMENT_COUNT &&
        display_settings.clock_delay_variant < MACROPAD_CLOCK_DELAY_VARIANT_COUNT &&
        lcd_timeout_mins_is_valid(display_settings.lcd_timeout_mins) && display_settings.clock_colon_blink <= 1 &&
        display_settings.clock_background_dim <= 100) {
        set_clock_element_defaults();
        persist_display_settings();
        return;
    }

    if (display_settings.magic == MACROPAD_DISPLAY_SETTINGS_V024_MAGIC &&
        display_settings.style < MACROPAD_DISPLAY_BUTTON_STYLE_COUNT && display_settings.brightness <= 100 &&
        display_settings.clock_style < MACROPAD_CLOCK_STYLE_COUNT && display_settings.clock_size < MACROPAD_CLOCK_SIZE_COUNT &&
        display_settings.clock_alignment < MACROPAD_CLOCK_ALIGNMENT_COUNT &&
        display_settings.clock_delay_variant < MACROPAD_CLOCK_DELAY_VARIANT_COUNT &&
        lcd_timeout_mins_is_valid(display_settings.lcd_timeout_mins) && display_settings.clock_colon_blink <= 1) {
        set_clock_background_defaults();
        set_clock_element_defaults();
        persist_display_settings();
        return;
    }

    if (display_settings.magic == MACROPAD_DISPLAY_SETTINGS_V021_MAGIC &&
        display_settings.style < MACROPAD_DISPLAY_BUTTON_STYLE_COUNT && display_settings.brightness <= 100 &&
        display_settings.clock_style < MACROPAD_CLOCK_STYLE_COUNT && display_settings.clock_size < MACROPAD_CLOCK_SIZE_COUNT &&
        display_settings.clock_alignment < MACROPAD_CLOCK_ALIGNMENT_COUNT &&
        display_settings.clock_delay_variant < MACROPAD_CLOCK_DELAY_VARIANT_COUNT &&
        lcd_timeout_mins_is_valid(display_settings.lcd_timeout_mins) && display_settings.clock_colon_blink <= 1) {
        set_clock_info_defaults();
        set_clock_background_defaults();
        set_clock_element_defaults();
        persist_display_settings();
        return;
    }

    if (display_settings.magic == MACROPAD_DISPLAY_SETTINGS_V018_MAGIC &&
        display_settings.style < MACROPAD_DISPLAY_BUTTON_STYLE_COUNT && display_settings.brightness <= 100 &&
        display_settings.clock_style < MACROPAD_CLOCK_STYLE_COUNT && display_settings.clock_size < MACROPAD_CLOCK_SIZE_COUNT &&
        display_settings.clock_alignment < MACROPAD_CLOCK_ALIGNMENT_COUNT &&
        display_settings.clock_delay_variant < MACROPAD_CLOCK_DELAY_VARIANT_COUNT &&
        lcd_timeout_mins_is_valid(display_settings.lcd_timeout_mins)) {
        display_settings.clock_colon_blink = 0;
        set_clock_info_defaults();
        set_clock_background_defaults();
        set_clock_element_defaults();
        persist_display_settings();
        return;
    }

    if (display_settings.legacy_magic == MACROPAD_DISPLAY_SETTINGS_V012_MAGIC &&
        display_settings.style < MACROPAD_DISPLAY_BUTTON_STYLE_COUNT && display_settings.brightness <= 100) {
        set_clock_defaults();
        persist_display_settings();
        return;
    }

    // In v002-v011 byte 3 combined the format marker and style. Preserve the
    // user's accent and style while filling the newly added fields.
    uint8_t legacy_magic = display_settings.style;
    uint8_t migrated_style;
    if (legacy_magic == MACROPAD_DISPLAY_SETTINGS_LEGACY_MAGIC) {
        migrated_style = 0;
    } else if ((legacy_magic & MACROPAD_DISPLAY_SETTINGS_V010_MAGIC_MASK) == MACROPAD_DISPLAY_SETTINGS_V010_MAGIC &&
               (legacy_magic & 0x0F) < 15) {
        migrated_style = legacy_magic & 0x0F;
    } else if ((legacy_magic & MACROPAD_DISPLAY_SETTINGS_V011_MAGIC_MASK) == MACROPAD_DISPLAY_SETTINGS_V011_MAGIC &&
               (legacy_magic & MACROPAD_DISPLAY_SETTINGS_V011_STYLE_MASK) < 31) {
        migrated_style = legacy_magic & MACROPAD_DISPLAY_SETTINGS_V011_STYLE_MASK;
    } else {
        kb_settings_lcd_reset();
        return;
    }

    display_settings.style            = migrated_style;
    display_settings.brightness       = MACROPAD_DISPLAY_BRIGHTNESS_DEFAULT;
    display_settings.background_red   = 0;
    display_settings.background_green = 0;
    display_settings.background_blue  = 0;
    set_clock_defaults();
    persist_display_settings();
}

void kb_settings_lcd_reset(void) {
    display_settings = (macropad_display_settings_t){
        .red = 200, .green = 178, .blue = 146, .style = 0, .brightness = MACROPAD_DISPLAY_BRIGHTNESS_DEFAULT,
        .background_red = 0, .background_green = 0, .background_blue = 0,
    };
    set_clock_defaults();
    persist_display_settings();
}

uint8_t get_display_accent_red(void) {
    return display_settings.red;
}

uint8_t get_display_accent_green(void) {
    return display_settings.green;
}

uint8_t get_display_accent_blue(void) {
    return display_settings.blue;
}

void set_display_accent_red(uint8_t red) {
    display_settings.red = red;
    persist_display_settings();
}

void set_display_accent_green(uint8_t green) {
    display_settings.green = green;
    persist_display_settings();
}

void set_display_accent_blue(uint8_t blue) {
    display_settings.blue = blue;
    persist_display_settings();
}

uint8_t get_display_background_red(void) {
    return display_settings.background_red;
}

uint8_t get_display_background_green(void) {
    return display_settings.background_green;
}

uint8_t get_display_background_blue(void) {
    return display_settings.background_blue;
}

void set_display_background_red(uint8_t red) {
    display_settings.background_red = red;
    persist_display_settings();
}

void set_display_background_green(uint8_t green) {
    display_settings.background_green = green;
    persist_display_settings();
}

void set_display_background_blue(uint8_t blue) {
    display_settings.background_blue = blue;
    persist_display_settings();
}

uint8_t get_lcd_brightness(void) {
    return display_settings.brightness;
}

void set_lcd_brightness(uint8_t brightness) {
    if (brightness > 100 || brightness == display_settings.brightness) return;
    display_settings.brightness = brightness;
    persist_display_settings();
}

extern bool screen_home_is_active(void);
uint8_t get_split_lcd_brightness(void) {
    uint8_t brightness = screen_home_is_active() ? eh_date_get(10) : display_settings.brightness;
    if (brightness == 0) return 0;
    uint8_t level = ((uint16_t)brightness * BACKLIGHT_LEVELS + 50) / 100;
    return MAX(1, level);
}

uint8_t get_display_button_style(void) {
    return display_settings.style;
}

void set_display_button_style(uint8_t style) {
    if (style >= MACROPAD_DISPLAY_BUTTON_STYLE_COUNT || style == get_display_button_style()) {
        return;
    }
    display_settings.style = style;
    persist_display_settings();
}

uint8_t get_clock_text_red(void) {
    return display_settings.clock_text_red;
}

uint8_t get_clock_text_green(void) {
    return display_settings.clock_text_green;
}

uint8_t get_clock_text_blue(void) {
    return display_settings.clock_text_blue;
}

void set_clock_text_red(uint8_t red) {
    if (red == display_settings.clock_text_red) return;
    display_settings.clock_text_red = red;
    persist_display_settings();
}

void set_clock_text_green(uint8_t green) {
    if (green == display_settings.clock_text_green) return;
    display_settings.clock_text_green = green;
    persist_display_settings();
}

void set_clock_text_blue(uint8_t blue) {
    if (blue == display_settings.clock_text_blue) return;
    display_settings.clock_text_blue = blue;
    persist_display_settings();
}

uint8_t get_clock_background_red(void) {
    return display_settings.clock_background_red;
}

uint8_t get_clock_background_green(void) {
    return display_settings.clock_background_green;
}

uint8_t get_clock_background_blue(void) {
    return display_settings.clock_background_blue;
}

void set_clock_background_red(uint8_t red) {
    if (red == display_settings.clock_background_red) return;
    display_settings.clock_background_red = red;
    persist_display_settings();
}

void set_clock_background_green(uint8_t green) {
    if (green == display_settings.clock_background_green) return;
    display_settings.clock_background_green = green;
    persist_display_settings();
}

void set_clock_background_blue(uint8_t blue) {
    if (blue == display_settings.clock_background_blue) return;
    display_settings.clock_background_blue = blue;
    persist_display_settings();
}

uint8_t get_clock_style(void) {
    return display_settings.clock_style;
}

void set_clock_style(uint8_t style) {
    if (style >= MACROPAD_CLOCK_STYLE_COUNT || style == display_settings.clock_style) return;
    display_settings.clock_style = style;
    persist_display_settings();
}

uint8_t get_clock_size(void) {
    return display_settings.clock_size;
}

void set_clock_size(uint8_t size) {
    if (size >= MACROPAD_CLOCK_SIZE_COUNT || size == display_settings.clock_size) return;
    display_settings.clock_size = size;
    persist_display_settings();
}

uint8_t get_clock_alignment(void) {
    return 1;
}

void set_clock_alignment(uint8_t alignment) {
    (void)alignment;
}

uint8_t get_clock_delay_variant(void) {
    return display_settings.clock_delay_variant;
}

void set_clock_delay_variant(uint8_t variant) {
    if (variant >= MACROPAD_CLOCK_DELAY_VARIANT_COUNT || variant == display_settings.clock_delay_variant) return;
    display_settings.clock_delay_variant = variant;
    persist_display_settings();
}

uint32_t get_clock_delay_ms(void) {
    // Keep the original indices stable for settings already stored in EEPROM.
    static const uint8_t delay_seconds[MACROPAD_CLOCK_DELAY_VARIANT_COUNT] = {0, 5, 10, 15, 30, 60, 120, 20};
    return (uint32_t)delay_seconds[display_settings.clock_delay_variant] * 1000;
}

bool get_clock_colon_blink(void) {
    return display_settings.clock_colon_blink != 0;
}

void set_clock_colon_blink(bool enabled) {
    if (enabled == get_clock_colon_blink()) return;
    display_settings.clock_colon_blink = enabled;
    persist_display_settings();
}

uint8_t get_clock_info_red(void) {
    return display_settings.clock_info_red;
}

uint8_t get_clock_info_green(void) {
    return display_settings.clock_info_green;
}

uint8_t get_clock_info_blue(void) {
    return display_settings.clock_info_blue;
}

void set_clock_info_red(uint8_t red) {
    if (red == display_settings.clock_info_red) return;
    display_settings.clock_info_red = red;
    persist_display_settings();
}

void set_clock_info_green(uint8_t green) {
    if (green == display_settings.clock_info_green) return;
    display_settings.clock_info_green = green;
    persist_display_settings();
}

void set_clock_info_blue(uint8_t blue) {
    if (blue == display_settings.clock_info_blue) return;
    display_settings.clock_info_blue = blue;
    persist_display_settings();
}

bool get_clock_visible(void) {
    return display_settings.clock_visible != 0;
}

void set_clock_visible(bool visible) {
    if (visible == get_clock_visible()) return;
    display_settings.clock_visible = visible;
    persist_display_settings();
}

uint8_t get_clock_opacity(void) {
    return display_settings.clock_opacity;
}

void set_clock_opacity(uint8_t opacity) {
    if (opacity > 100 || opacity == display_settings.clock_opacity) return;
    display_settings.clock_opacity = opacity;
    persist_display_settings();
}

bool get_clock_info_visible(void) {
    return display_settings.clock_info_visible != 0;
}

void set_clock_info_visible(bool visible) {
    if (visible == get_clock_info_visible()) return;
    display_settings.clock_info_visible = visible;
    persist_display_settings();
}

uint8_t get_clock_info_opacity(void) {
    return display_settings.clock_info_opacity;
}

void set_clock_info_opacity(uint8_t opacity) {
    if (opacity > 100 || opacity == display_settings.clock_info_opacity) return;
    display_settings.clock_info_opacity = opacity;
    persist_display_settings();
}

bool get_clock_modifiers_visible(void) {
    return display_settings.clock_modifiers_visible != 0;
}

void set_clock_modifiers_visible(bool visible) {
    if (visible == get_clock_modifiers_visible()) return;
    display_settings.clock_modifiers_visible = visible;
    persist_display_settings();
}

uint8_t get_clock_modifiers_red(void) {
    return display_settings.clock_modifiers_red;
}

uint8_t get_clock_modifiers_green(void) {
    return display_settings.clock_modifiers_green;
}

uint8_t get_clock_modifiers_blue(void) {
    return display_settings.clock_modifiers_blue;
}

void set_clock_modifiers_red(uint8_t red) {
    if (red == display_settings.clock_modifiers_red) return;
    display_settings.clock_modifiers_red = red;
    persist_display_settings();
}

void set_clock_modifiers_green(uint8_t green) {
    if (green == display_settings.clock_modifiers_green) return;
    display_settings.clock_modifiers_green = green;
    persist_display_settings();
}

void set_clock_modifiers_blue(uint8_t blue) {
    if (blue == display_settings.clock_modifiers_blue) return;
    display_settings.clock_modifiers_blue = blue;
    persist_display_settings();
}

uint8_t get_clock_modifiers_opacity(void) {
    return display_settings.clock_modifiers_opacity;
}

void set_clock_modifiers_opacity(uint8_t opacity) {
    if (opacity > 100 || opacity == display_settings.clock_modifiers_opacity) return;
    display_settings.clock_modifiers_opacity = opacity;
    persist_display_settings();
}

uint8_t get_clock_background_dim(void) {
    return display_settings.clock_background_dim;
}

void set_clock_background_dim(uint8_t dim) {
    if (dim > 100 || dim == display_settings.clock_background_dim) return;
    display_settings.clock_background_dim = dim;
    persist_display_settings();
}

uint8_t get_lcd_timeout_mins(void) {
    return display_settings.lcd_timeout_mins;
}

void set_lcd_timeout_mins(uint8_t timeout_mins) {
    if (!lcd_timeout_mins_is_valid(timeout_mins) || timeout_mins == display_settings.lcd_timeout_mins) return;
    display_settings.lcd_timeout_mins = timeout_mins;
    persist_display_settings();
}

uint32_t get_lcd_timeout_ms(void) {
    return (uint32_t)display_settings.lcd_timeout_mins * 60 * 1000;
}

uint8_t get_led_rgb_timeout_mins(void) {
    return rgb_timeout_mins;
}

void set_led_rgb_timeout_mins(uint8_t timeout_mins) {
    if (rgb_timeout_mins_is_valid(timeout_mins)) {
        update_rgb_timeout_mins(timeout_mins);
    }
}

uint32_t get_led_rgb_timeout_ms(void) {
    if (rgb_timeout_mins == 0) {
        return 0;
    }
    return (uint32_t)rgb_timeout_mins * 60 * 1000;
}

void housekeeping_task_user(void) {
    eh_extra_settings_housekeep();
    display_housekeeping_task();
}

void keyboard_post_init_user(void) {
    display_init_kb();
}
