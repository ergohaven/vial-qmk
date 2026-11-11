#include "keyboards/ergohaven/sm62/render_alias.h"
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
#if defined(KEYBOARD_ergohaven_sm62)
static bool painter_ready   = false;
#endif
static lv_obj_t *display_root;
static lv_obj_t *display_active_view;

__attribute__((weak)) void display_diag_mark(uint32_t stage) {
    (void)stage;
}

bool is_display_enabled(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) return sm62_render_enabled();
#endif
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
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_style(1,red,green,blue); return; }
#endif
    accent_color_blue = lv_color_make(red, green, blue);
    if (!display_enabled) return;

    lv_style_set_text_color(&style_button, accent_color_blue);
    lv_style_set_bg_color(&style_button_active, accent_color_blue);
    display_accent_color_changed_kb();
}

void display_apply_background_color(uint8_t red, uint8_t green, uint8_t blue) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_style(2,red,green,blue); return; }
#endif
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
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_style(16,0,0,0); return; }
#endif
    if (display_enabled && is_display_on) {
        backlight_level_noeeprom(get_split_lcd_brightness());
    }
}

void display_apply_button_style(uint8_t style) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_style(4,style,0,0); return; }
#endif
    if (!display_enabled) return;
    display_button_style_changed_kb(style);
}

void display_apply_clock_settings(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_style(8,0,0,0); return; }
#endif
    if (!display_enabled) return;
    display_clock_settings_changed_kb();
}

lv_obj_t *display_view_create(void) {
    if (!display_root) {
        display_root = lv_obj_create(NULL);
        lv_obj_set_size(display_root, 240, 280);
        lv_obj_set_style_pad_all(display_root, 0, 0);
        lv_obj_set_style_border_width(display_root, 0, 0);
        lv_obj_set_style_bg_color(display_root, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(display_root, LV_OPA_COVER, 0);
        lv_obj_clear_flag(display_root, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t *view = lv_obj_create(display_root);
    lv_obj_set_pos(view, 0, 0);
    lv_obj_set_size(view, 240, 280);
    lv_obj_set_style_pad_all(view, 0, 0);
    lv_obj_set_style_border_width(view, 0, 0);
    lv_obj_clear_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    return view;
}

void display_show_view(lv_obj_t *view) {
    if (!view || view == display_active_view) {
        return;
    }

    display_diag_mark(0x3A0);
    /* All views are full-screen and opaque. Their fixed child order is
     * irrelevant while only one is visible, so do not reorder the LVGL child
     * list during a transition. Reordering could collide with the renderer
     * traversing that same list and was the last operation shared by the S302
     * failures. */
    lv_obj_clear_flag(view, LV_OBJ_FLAG_HIDDEN);
    display_diag_mark(0x3A1);
    if (display_active_view) {
        lv_obj_add_flag(display_active_view, LV_OBJ_FLAG_HIDDEN);
    }
    display_diag_mark(0x3A2);
    display_active_view = view;
    display_diag_mark(0x3A3);
}

void display_refresh(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_style(32,0,0,0); return; }
#endif
    if (display_root) lv_obj_invalidate(display_root);
}

void display_load_root(void) {
    if (display_root && lv_scr_act() != display_root) {
        lv_scr_load(display_root);
    }
}

__attribute__((weak)) const char *display_boot_status_kb(void) {
    return "v" EH_VERSION_STR;
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

#if defined(KEYBOARD_ergohaven_sm62)
extern void sm62_diag_stage(uint8_t stage);
#endif

#if defined(KEYBOARD_ergohaven_sm62)
bool display_init_kb(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) return sm62_render_enabled();
#endif
    /* Five total attempts, one per housekeeping deadline; never reset an
     * already initialized painter or allocate another registered device. */
    static uint8_t attempts;
    static uint32_t attempted_at;
    if (display_enabled) return true;
    if (attempts >= 5 || (attempts && timer_elapsed32(attempted_at) < 250U)) return false;
    ++attempts;
    attempted_at = timer_read32();
    display_enabled = false;
    dprint("display_init_kb - start\n");

    backlight_init();
#ifdef SM62_DISPLAY_SYNC_DELAY_MS
    /* Keep both panels dark while their independent startup paths run. */
    backlight_level_noeeprom(0);
#else
    backlight_level_noeeprom(get_split_lcd_brightness());
#endif

    /* Keep the registered painter device across bounded retries.  The ST7789
     * factory has a fixed device pool, so allocating a fresh handle after a
     * transient cold-start failure would exhaust that pool immediately. */
    if (!display) {
        display = qp_st7789_make_spi_device(240, 280, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, EH_DISPLAY_SPI_DIVISOR, 3);
        if (!display) {
            dprint("display_init_kb - no painter device\n");
            return false;
        }
        qp_set_viewport_offsets(display, 0, 20);
    }

    if (!painter_ready) {
        painter_ready = qp_init(display, QP_ROTATION);
        if (!painter_ready) {
            dprint("display_init_kb - qp_init failed\n");
            return false;
        }
    }
    sm62_diag_stage(1);
    if (!qp_lvgl_attach(display)) {
        dprint("display_init_kb - qp_lvgl_attach failed\n");
        return false;
    }

    sm62_diag_stage(2);
    display_enabled = true;
    dprint("display_init_kb - initialised\n");

    display_init_styles_kb();
    display_init_screens_kb();

    return display_enabled;
}
#else
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
#endif

__attribute__((weak)) bool display_user_enabled_kb(void) { return true; }

#if defined(KEYBOARD_ergohaven_sm62)
static bool display_power_off;
static bool display_power_known;
static bool display_power_requested;
static bool display_power_request_seen;
static uint8_t display_power_attempts;
static uint32_t display_power_attempted_at;

/* Budget is per desired-state transition, not per loop. Failed commands never
 * publish success or illuminate the backlight. No blind panel reset is used. */
static void sm62_request_display_power(bool on) {
    if (!display_power_request_seen || display_power_requested != on) {
        display_power_request_seen = true;
        display_power_requested = on;
        display_power_attempts = 0;
    }
    if (!on) backlight_level_noeeprom(0);
    if (display_power_known && ((on && is_display_on) || (!on && display_power_off))) {
        if (on) backlight_level_noeeprom(get_split_lcd_brightness());
        return;
    }
    if (!display || !painter_ready || display_power_attempts >= 5 ||
        (display_power_attempts && timer_elapsed32(display_power_attempted_at) < 250U)) return;
    ++display_power_attempts;
    display_power_attempted_at = timer_read32();
    display_power_known = false;
    if (!qp_power(display, on)) return;
    display_power_known = true;
    is_display_on = on;
    display_power_off = !on;
    backlight_level_noeeprom(on ? get_split_lcd_brightness() : 0);
}
#endif

#if defined(KEYBOARD_ergohaven_sm62)
void display_turn_on(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_suspend(false); return; }
#endif
#if defined(SM62_DUALCORE_RENDER)
    if (sm62_view_off()) { display_turn_off(); return; }
#else
    if (!display_user_enabled_kb()) { display_turn_off(); return; }
#endif
    sm62_request_display_power(true);
}
#else
void display_turn_on(void) {
    if (!is_display_on) {
#if !defined(K03_DISPLAY_LEFT) && !defined(K03_DISPLAY_RIGHT)
        qp_power(display, true);
#endif
        backlight_level_noeeprom(get_split_lcd_brightness());
        is_display_on = true;
    }
}
#endif

#if defined(KEYBOARD_ergohaven_sm62)
void display_turn_off(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) { sm62_render_suspend(true); return; }
#endif
    sm62_request_display_power(false);
}
#else
void display_turn_off(void) {
    if (is_display_on) {
        is_display_on = false;
#if !defined(K03_DISPLAY_LEFT) && !defined(K03_DISPLAY_RIGHT)
        qp_power(display, false);
#endif
        backlight_level_noeeprom(0);
    }
}
#endif

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
