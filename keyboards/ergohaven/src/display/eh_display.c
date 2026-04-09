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

lv_style_t style_screen;
lv_style_t style_container;
lv_style_t style_button;
lv_style_t style_button_active;

static bool display_enabled = false;

bool is_display_enabled(void) {
    return display_enabled;
}

__attribute__((weak)) void display_init_styles_kb(void) {
    accent_color_red       = lv_color_make(248, 83, 107);
    accent_color_blue      = lv_color_make(84, 189, 191);
    lv_disp_t  *lv_display = lv_disp_get_default();
    lv_theme_t *lv_theme   = lv_theme_default_init(lv_display, accent_color_blue, accent_color_red, true, LV_FONT_DEFAULT);
    lv_disp_set_theme(lv_display, lv_theme);

    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_black());

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
    lv_style_set_text_color(&style_button_active, lv_color_black());
}

__attribute__((weak)) void display_init_screens_kb(void);

#ifndef QP_ROTATION
#    define QP_ROTATION QP_ROTATION_180
#endif

bool display_init_kb(void) {
    display_enabled = false;
    dprint("display_init_kb - start\n");

    backlight_init();

    display = qp_st7789_make_spi_device(240, 280, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, 4, 3);
    qp_set_viewport_offsets(display, 0, 20);

    if (!qp_init(display, QP_ROTATION) || !qp_lvgl_attach(display)) return display_enabled;

    display_enabled = true;
    dprint("display_init_kb - initialised\n");

    display_init_styles_kb();
    display_init_screens_kb();

    return display_enabled;
}

static bool is_display_on = false;

void display_turn_on(void) {
    if (!is_display_on) {
        backlight_init();
        qp_power(display, true);
        is_display_on = true;
    }
}

void display_turn_off(void) {
    if (is_display_on) {
        is_display_on = false;
        qp_power(display, false);
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
