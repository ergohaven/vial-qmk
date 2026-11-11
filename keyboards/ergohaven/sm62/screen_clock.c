// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboards/ergohaven/sm62/render_alias.h"
#include "src/display/eh_display.h"
#include "sm62_effect_config.h"
#include "sm62_date.h"
#include <lvgl.h>
#include "application_display.h"
#include "hid.h"
#include "ergohaven.h"
#include "src/eh_settings.h"
#include <string.h>
#include "timer.h"
#include "src/eh_ruen.h"
#include "keyboards/ergohaven/macropad/sm62_starfield.h"
#include "sm62_starfield_control.h"

LV_FONT_DECLARE(sm62_font_unbounded_48);
LV_FONT_DECLARE(sm62_font_unbounded_16);
LV_FONT_DECLARE(sm62_font_zen_regular_20);
LV_FONT_DECLARE(sm62_font_zen_medium_20);

#ifndef SM62_EFFECT
#    define SM62_EFFECT 0
#endif
#if SM62_FAST_RETURN
#    define SM62_BUILD_ID "SM62-1.1-BOOT-FPS"
#elif SM62_EFFECT == 1
#    define SM62_BUILD_ID "SM62-1-LIQUID-DROPLET"
#elif SM62_EFFECT == 2
#    define SM62_BUILD_ID "SM62-1-SUBTLE-RIPPLE"
#elif SM62_EFFECT == 3
#    define SM62_BUILD_ID "SM62-1-GOLD-SHOCKWAVE"
#elif SM62_EFFECT == 4
#    define SM62_BUILD_ID "SM62-1-PRESSURE-RING"
#elif SM62_EFFECT == 5
#    define SM62_BUILD_ID "SM62-1-DIRECTIONAL-TRAIL"
#elif SM62_EFFECT == 6
#    define SM62_BUILD_ID "SM62-1-SOFT-BLOOM"
#elif SM62_EFFECT == 7
#    define SM62_BUILD_ID "SM62-1-GRID-PULSE"
#elif SM62_EFFECT == 8
#    define SM62_BUILD_ID "SM62-1-ELASTIC-TAP"
#elif SM62_EFFECT == 9
#    define SM62_BUILD_ID "SM62-1-RIPPLE-GRID-PULSE"
#else
#    define SM62_BUILD_ID "SM62-1-BASE"
#endif
static const char sm62_build_id[] __attribute__((used)) = SM62_BUILD_ID "-COMMON";

static lv_obj_t *screen_main;
static lv_obj_t *label_hours;
static lv_obj_t *label_minutes;
static lv_obj_t *label_colon;
static lv_obj_t *label_date;
static bool clock_live;
static lv_obj_t *label_language;
static lv_obj_t *label_layer_number;
static lv_obj_t *label_layer_name;
static uint8_t displayed_layer = UINT8_MAX;
static char displayed_layer_name[32];
static sm62_starfield_t stars;
static uint8_t displayed_language = UINT8_MAX;
static lv_obj_t *make_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, const lv_font_t *font, lv_text_align_t align, uint32_t color, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(label, text);
    return label;
}

static void clock_label_positions(bool live) {
    lv_coord_t shift = 0;
    if (!live) {
        /* Center the placeholder's visible glyph bounds, not the unequal
         * digit-pair label boxes. Keep all three labels' spacing unchanged. */
        lv_point_t sm_size;
        lv_font_glyph_dsc_t first, six, last;
        lv_txt_get_size(&sm_size, "SM", &sm62_font_unbounded_48, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_font_get_glyph_dsc(&sm62_font_unbounded_48, &first, 'S', 'M');
        lv_font_get_glyph_dsc(&sm62_font_unbounded_48, &six, '6', '2');
        lv_font_get_glyph_dsc(&sm62_font_unbounded_48, &last, '2', 0);
        lv_coord_t left = 7 + 98 - sm_size.x + first.ofs_x;
        lv_coord_t right = 132 + six.adv_w + last.ofs_x + last.box_w;
        lv_obj_update_layout(screen_main);
        shift = (lv_obj_get_width(screen_main) - (right - left)) / 2 - left;
    } else {
        /* Measure current text at the original anchors; never accumulate shifts. */
        const char *hours = lv_label_get_text(label_hours);
        const char *minutes = lv_label_get_text(label_minutes);
        lv_point_t hours_size;
        lv_font_glyph_dsc_t first, minute_first, last;
        lv_txt_get_size(&hours_size, hours, &sm62_font_unbounded_48, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_font_get_glyph_dsc(&sm62_font_unbounded_48, &first, hours[0], hours[1]);
        lv_font_get_glyph_dsc(&sm62_font_unbounded_48, &minute_first, minutes[0], minutes[1]);
        lv_font_get_glyph_dsc(&sm62_font_unbounded_48, &last, minutes[1], 0);
        lv_coord_t left = 7 + 98 - hours_size.x + first.ofs_x;
        lv_coord_t right = 132 + minute_first.adv_w + last.ofs_x + last.box_w;
        lv_obj_update_layout(screen_main);
        shift = (lv_obj_get_width(screen_main) - (right - left)) / 2 - left;
    }
    lv_obj_set_x(label_hours, 7 + shift);
    lv_obj_set_x(label_colon, 110 + shift);
    lv_obj_set_x(label_minutes, 132 + shift);
}

static void screen_main_init(void) {
    __asm__ volatile("" : : "r"(sm62_build_id));
    screen_main = display_view_create();
    lv_obj_set_style_pad_all(screen_main, 0, 0);
    lv_obj_set_style_border_width(screen_main, 0, 0);
    lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x070705), 0);
    lv_obj_set_style_bg_opa(screen_main, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_SCROLLABLE);

    sm62_starfield_init(&stars, screen_main, 0x0139A62U);
    sm62_stars_update(&stars);

    label_layer_number = make_label(screen_main, 40, 17, 160, 21, &sm62_font_zen_regular_20, LV_TEXT_ALIGN_CENTER, 0xC1C1C0, "LAYER / 0");
    lv_obj_t *header_rule = lv_obj_create(screen_main);
    lv_obj_set_pos(header_rule, 72, 41);
    lv_obj_set_size(header_rule, 96, 3);
    lv_obj_set_style_radius(header_rule, 0, 0);
    lv_obj_set_style_pad_all(header_rule, 0, 0);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_set_style_bg_color(header_rule, lv_color_hex(0xF2DB95), 0);
    lv_obj_set_style_bg_opa(header_rule, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header_rule, LV_OBJ_FLAG_SCROLLABLE);

    label_layer_name = make_label(screen_main, 40, 49, 160, 21, &sm62_font_zen_medium_20, LV_TEXT_ALIGN_CENTER, 0xFFF8E7, "MAIN");

    label_hours = make_label(screen_main, 7, 124, 98, 44, &sm62_font_unbounded_48, LV_TEXT_ALIGN_RIGHT, 0xFFF8E7, "SM");
    label_colon = make_label(screen_main, 110, 124, 20, 44, &sm62_font_unbounded_48, LV_TEXT_ALIGN_CENTER, 0xF2DB95, ":");
    label_minutes = make_label(screen_main, 132, 124, 84, 44, &sm62_font_unbounded_48, LV_TEXT_ALIGN_LEFT, 0xFFF8E7, "62");
    clock_label_positions(false);

    label_date = make_label(screen_main, 50, 170, 140, 18, &sm62_font_unbounded_16, LV_TEXT_ALIGN_CENTER, 0xC1C1C0, "");
    lv_obj_add_flag(label_date, LV_OBJ_FLAG_HIDDEN);
    label_language = make_label(screen_main, 180, 243, 40, 21, &sm62_font_zen_medium_20, LV_TEXT_ALIGN_RIGHT, 0xFFF8E7, "EN");
    lv_obj_add_flag(label_language, LV_OBJ_FLAG_HIDDEN);
}

static void update_time(bool force) {
    hid_data_t *hid = get_hid_data();
    bool live = is_hid_active() && hid->hours < 24U && hid->minutes < 60U;
    if (!live) {
        if (force || clock_live) {
            lv_label_set_text_static(label_hours, "SM");
            lv_label_set_text_static(label_minutes, "62");
            clock_label_positions(false);
        }
        clock_live = false;
        return;
    }
    if (force || !clock_live || hid->time_changed) {
        lv_label_set_text_fmt(label_hours, "%02u", hid->hours);
        lv_label_set_text_fmt(label_minutes, "%02u", hid->minutes);
        clock_label_positions(true);
    }
    clock_live = true;
    hid->time_changed = false;
}

static void update_language(bool force) {
    /* Check validity before the language cache: expiry/re-sync may leave the
     * same language value and must still change visibility. */
    if (!sm62_local_language_enabled() || !is_hid_layout_valid()) {
        lv_obj_add_flag(label_language, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(label_language, LV_OBJ_FLAG_HIDDEN);
    uint8_t language = get_cur_lang();
    if (!force && language == displayed_language) {
        return;
    }
    displayed_language = language;
    lv_label_set_text_static(label_language, language == LANG_EN ? "EN" : "RU");
}

static void update_layer(bool force) {
    uint8_t layer = get_current_layer();
    const char *name = sm62_effective_layer_name(layer);
    if (!force && layer == displayed_layer && strcmp(name, displayed_layer_name) == 0) return;
    displayed_layer = layer;
    lv_label_set_text_fmt(label_layer_number, "LAYER / %u", (unsigned)layer);
    lv_label_set_text(label_layer_name, name);
    strncpy(displayed_layer_name, name, sizeof(displayed_layer_name) - 1U);
    displayed_layer_name[sizeof(displayed_layer_name) - 1U] = '\0';
}

static void screen_main_load(void) {
    stars.timer = timer_read32();
    sm62_starfield_wait_for_frame(&stars);
    sm62_stars_update(&stars);
    display_show_view(screen_main);
    update_layer(true);
    update_language(true);
    update_time(true);
    sm62_date_update(label_date, true);
}

void sm62_main_frame_completed(void) {
    sm62_starfield_frame_completed(&stars);
}

static void screen_main_housekeep(void) {
    /* Do not enqueue another set of star invalidations while the previous
     * frame is still flushing, especially during cold-start USB activity. */
    sm62_stars_update(&stars);
    update_layer(false);
    update_language(false);
    update_time(false);
    sm62_date_update(label_date, false);
}

const eh_screen_t eh_screen_clock = {
    .init = screen_main_init,
    .load = screen_main_load,
    .housekeep = screen_main_housekeep,
};
