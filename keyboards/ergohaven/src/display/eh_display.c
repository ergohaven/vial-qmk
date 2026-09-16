#include "eh_display.h"
#include "gpio.h"
#include "hid.h"
#include "lvgl_helpers.h"
#include "qp.h"
#include "src/eh_ruen.h"
#include "eh_symbols.h"
#include "src/eh_settings.h"
#include "ergohaven.h"

painter_device_t display;

/* shared styles */
lv_color_t accent_color_red;
lv_color_t accent_color_blue;
lv_color_t display_background_color;

lv_style_t style_screen;
lv_style_t style_container;
lv_style_t style_button;
lv_style_t style_button_active;

static bool display_enabled = false;
static bool is_display_on   = false;

bool is_display_enabled(void) {
    return display_enabled;
}

__attribute__((weak)) bool display_should_wake_on_usb_resume(void) {
    return true;
}

__attribute__((weak)) void display_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
    (void)row;
    (void)col;
    (void)pressed;
}

__attribute__((weak)) void display_process_matrix_press(void) {}

__attribute__((weak)) void display_process_encoder_event(uint8_t index, bool clockwise, uint16_t keycode) {
    (void)index;
    (void)clockwise;
    (void)keycode;
}

__attribute__((weak)) void display_accent_color_changed_kb(void) {}

__attribute__((weak)) void display_background_color_changed_kb(void) {}

__attribute__((weak)) void display_button_style_changed_kb(uint8_t style) {
    (void)style;
}

__attribute__((weak)) void display_clock_settings_changed_kb(void) {}

void display_apply_accent_color(uint8_t red, uint8_t green, uint8_t blue) {
    accent_color_blue = lv_color_make(red, green, blue);
    if (!display_enabled) return;

    lv_style_set_text_color(&style_button, accent_color_blue);
    lv_style_set_bg_color(&style_button_active, accent_color_blue);
    display_accent_color_changed_kb();
}

void display_apply_background_color(uint8_t red, uint8_t green, uint8_t blue) {
    display_background_color = lv_color_make(red, green, blue);
    if (!display_enabled) return;

    lv_style_set_bg_color(&style_screen, display_background_color);
    lv_style_set_text_color(&style_button_active, display_background_color);
    // Mutating a shared LVGL style does not automatically repaint every
    // object which already references it. Report the change before updating
    // screen-specific custom drawing so the entire active background changes
    // in the same frame.
    lv_obj_report_style_change(&style_screen);
    lv_obj_report_style_change(&style_button_active);
    display_background_color_changed_kb();
}

void display_apply_brightness(void) {
    if (display_enabled && is_display_on) {
        backlight_level_noeeprom(get_split_lcd_brightness());
    }
}

void display_apply_button_style(uint8_t style) {
    if (!display_enabled) return;
    display_button_style_changed_kb(style);
}

void display_apply_clock_settings(void) {
    if (!display_enabled) return;
    display_clock_settings_changed_kb();
}

__attribute__((weak)) void display_init_styles_kb(void) {
    accent_color_red       = lv_color_make(248, 83, 107);
    accent_color_blue      = lv_color_make(get_display_accent_red(), get_display_accent_green(), get_display_accent_blue());
    display_background_color = lv_color_make(get_display_background_red(), get_display_background_green(), get_display_background_blue());
    lv_disp_t  *lv_display = lv_disp_get_default();
    lv_theme_t *lv_theme   = lv_theme_default_init(lv_display, accent_color_blue, accent_color_red, true, LV_FONT_DEFAULT);
    lv_disp_set_theme(lv_display, lv_theme);

    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, display_background_color);

    lv_style_init(&style_container);
    lv_style_set_pad_top(&style_container, 0);
    lv_style_set_pad_bottom(&style_container, 0);
    lv_style_set_pad_left(&style_container, 0);
    lv_style_set_pad_right(&style_container, 0);
    lv_style_set_bg_opa(&style_container, 0);
    lv_style_set_border_width(&style_container, 0);
    lv_style_set_width(&style_container, lv_pct(100));
    lv_style_set_height(&style_container, LV_SIZE_CONTENT);

    lv_style_init(&style_button);
    lv_style_set_pad_top(&style_button, 4);
    lv_style_set_pad_bottom(&style_button, 4);
    lv_style_set_pad_left(&style_button, 4);
    lv_style_set_pad_right(&style_button, 4);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_text_color(&style_button, accent_color_blue);

    lv_style_init(&style_button_active);
    lv_style_set_bg_color(&style_button_active, accent_color_blue);
    lv_style_set_bg_opa(&style_button_active, LV_OPA_100);
    lv_style_set_text_color(&style_button_active, display_background_color);
}

__attribute__((weak)) void display_init_screens_kb(void);

#ifndef QP_ROTATION
#    define QP_ROTATION QP_ROTATION_180
#endif

#ifndef EH_DISPLAY_SPI_DIVISOR
#    define EH_DISPLAY_SPI_DIVISOR 4
#endif

bool display_init_kb(void) {
    display_enabled = false;
    dprint("display_init_kb - start\n");

    backlight_init();
    backlight_level_noeeprom(get_split_lcd_brightness());

    display = qp_st7789_make_spi_device(240, 280, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, EH_DISPLAY_SPI_DIVISOR, 3);
    qp_set_viewport_offsets(display, 0, 20);

    if (!qp_init(display, QP_ROTATION) || !qp_lvgl_attach(display)) return display_enabled;

    display_enabled = true;
    dprint("display_init_kb - initialised\n");

    display_init_styles_kb();
    display_init_screens_kb();

    return display_enabled;
}

void display_turn_on(void) {
    if (!is_display_on) {
#if !defined(K03_DISPLAY_LEFT) && !defined(K03_DISPLAY_RIGHT)
        qp_power(display, true);
#endif
        backlight_level_noeeprom(get_split_lcd_brightness());
        is_display_on = true;
    }
}

void display_turn_off(void) {
    if (is_display_on) {
        is_display_on = false;
#if !defined(K03_DISPLAY_LEFT) && !defined(K03_DISPLAY_RIGHT)
        qp_power(display, false);
#endif
        backlight_level_noeeprom(0);
    }
}

/* Common helpers */

const char *get_layer_label(uint8_t layer) {
    static char buf[32];
    sprintf(buf, EH_SYMBOL_LAYER " %s", layer_name(layer));
    return buf;
}

const char *get_layout_label(uint8_t layout) {
    switch (layout) {
        default:
        case LANG_EN:
            return EH_SYMBOL_GLOBE " EN";
            break;

        case LANG_RU:
            return EH_SYMBOL_GLOBE " RU";
            break;
    }
}
