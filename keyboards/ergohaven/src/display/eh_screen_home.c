#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "lvgl_helpers.h"
#include "hid.h"
#include "eh_symbols.h"

LV_FONT_DECLARE(eh_font_montserrat_20);
LV_FONT_DECLARE(eh_font_montserrat_28);

static lv_obj_t *screen_home;
static lv_obj_t *label_product;
static lv_obj_t *label_time;
static lv_obj_t *label_layer;
static lv_obj_t *label_mac;
static lv_obj_t *label_layout;
static lv_obj_t *label_shift;
static lv_obj_t *label_ctrl;
static lv_obj_t *label_alt;
static lv_obj_t *label_gui;
static lv_obj_t *label_num;
static lv_obj_t *label_caps;
static lv_obj_t *label_scroll;
static lv_obj_t *label_hid_media_artist;
static lv_obj_t *label_hid_media_title;
static lv_obj_t *screen_home_mods;
static lv_obj_t *screen_home_media;

void screen_home_init(void) {
    screen_home = lv_obj_create(NULL);
    lv_obj_add_style(screen_home, &style_screen, 0);

    label_product = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_product, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_product, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label_product, 0, 40);
    lv_obj_set_size(label_product, 240, 50);
    lv_label_set_text(label_product, EH_SHORT_PRODUCT_NAME);

    label_time = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label_time, 0, 36);
    lv_obj_set_size(label_time, 240, 50);
    lv_label_set_text(label_time, "00:00");
    lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);

    label_layer = lv_label_create(screen_home);
    lv_label_set_text(label_layer, "");
    lv_obj_set_pos(label_layer, 0, 100);
    lv_obj_set_size(label_layer, 110, 20);
    lv_obj_set_style_text_align(label_layer, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(label_layer, &eh_font_montserrat_20, LV_PART_MAIN);

    label_mac = lv_label_create(screen_home);
    lv_label_set_text(label_mac, EH_SYMBOL_MAC);
    lv_obj_set_pos(label_mac, 110, 100);
    lv_obj_set_size(label_mac, 46, 20);
    lv_obj_set_style_text_align(label_mac, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_mac, &eh_font_montserrat_20, LV_PART_MAIN);
    toggle_hidden(label_mac, false);

    label_layout = lv_label_create(screen_home);
    lv_label_set_text(label_layout, "");
    lv_obj_set_pos(label_layout, 160, 100);
    lv_obj_set_size(label_layout, 80, 20);
    lv_obj_set_style_text_align(label_layout, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layout, &eh_font_montserrat_20, LV_PART_MAIN);

    screen_home_mods = lv_obj_create(screen_home);
    lv_obj_add_style(screen_home_mods, &style_container, 0);
    use_flex_row(screen_home_mods);
    lv_obj_set_pos(screen_home_mods, 0, 130);
    lv_obj_set_size(screen_home_mods, 240, 100);

    label_gui   = create_button(screen_home_mods, "GUI", &style_button, &style_button_active);
    label_alt   = create_button(screen_home_mods, "ALT", &style_button, &style_button_active);
    label_ctrl  = create_button(screen_home_mods, "CTL", &style_button, &style_button_active);
    label_shift = create_button(screen_home_mods, "SFT", &style_button, &style_button_active);

    label_num    = create_button(screen_home_mods, "NUM", &style_button, &style_button_active);
    label_caps   = create_button(screen_home_mods, "CAPS", &style_button, &style_button_active);
    label_scroll = create_button(screen_home_mods, "SCRL", &style_button, &style_button_active);

    screen_home_media = lv_obj_create(screen_home);
    lv_obj_add_style(screen_home_media, &style_container, 0);
    toggle_hidden(screen_home_media, false);
    use_flex_row(screen_home_media);
    lv_obj_set_pos(screen_home_media, 0, 130);
    lv_obj_set_size(screen_home_media, 240, 130);

    label_hid_media_title = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_title, "");
    lv_label_set_long_mode(label_hid_media_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_hid_media_title, lv_pct(95));
    lv_obj_set_style_text_align(label_hid_media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_title, &eh_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label_hid_media_title, 0, 0);
    lv_obj_set_style_pad_bottom(label_hid_media_title, 0, 0);

    label_hid_media_artist = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_artist, "");
    lv_label_set_long_mode(label_hid_media_artist, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_hid_media_artist, lv_pct(95));
    lv_obj_set_style_text_align(label_hid_media_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_artist, &eh_font_montserrat_20, LV_PART_MAIN);

    lv_obj_set_style_pad_top(label_hid_media_artist, 10, 0);
    lv_obj_set_style_text_color(label_hid_media_artist, accent_color_blue, 0);
}

void screen_home_load(void) {
    lv_scr_load(screen_home);
}

void screen_home_housekeep(void) {
    static uint8_t prev_layer      = 255;
    static uint8_t prev_lang       = -1;
    static led_t   prev_led_state  = {.raw = 0};
    static uint8_t prev_mods       = 255;
    static bool    prev_show_mods  = false;
    static bool    prev_hid_active = false;
    static bool    prev_mac        = false;

    uint8_t cur_layer = get_current_layer();
    uint8_t cur_lang  = split_get_lang();
    led_t   led_state = host_keyboard_led_state();
    led_state.caps_lock |= split_get_caps_word();
    uint8_t mods = get_mods() | get_oneshot_mods();
    bool    mac  = split_get_mac();

    static uint32_t mods_timer = 0;
    if (mods != 0 || prev_led_state.raw != led_state.raw) {
        mods_timer = timer_read32();
    }

    bool show_mods  = timer_elapsed32(mods_timer) < EH_DISPLAY_TIMEOUT_ACTIVITY;
    bool hid_active = is_hid_active();

    uint32_t activity_elapsed = last_input_activity_elapsed();
    if (activity_elapsed > __UINT32_MAX__ - 1000) // possible overflow on split
        activity_elapsed = 0;
    bool typing = activity_elapsed < 500; // prevent display updates when typing

    if (prev_hid_active != hid_active || (prev_show_mods != show_mods && hid_active)) {
        if (typing) return;
        toggle_hidden(label_product, !hid_active);
        toggle_hidden(label_time, hid_active);
        toggle_hidden(screen_home_mods, !hid_active || show_mods);
        toggle_hidden(screen_home_media, hid_active && !show_mods);
        prev_show_mods  = show_mods;
        prev_hid_active = hid_active;
        return;
    }

    if (prev_layer != cur_layer) {
        lv_label_set_text(label_layer, get_layer_label(cur_layer));
        prev_layer = cur_layer;
        return;
    }

    if (prev_lang != cur_lang) {
        lv_label_set_text(label_layout, get_layout_label(cur_lang));
        prev_lang = cur_lang;
        return;
    }

    if (mac != prev_mac) {
        toggle_hidden(label_mac, mac);
        if (mac) {
            lv_label_set_text(label_gui, "CMD");
            lv_label_set_text(label_alt, "OPT");
        } else {
            lv_label_set_text(label_gui, "GUI");
            lv_label_set_text(label_alt, "ALT");
        }
        prev_mac = mac;
        return;
    }

    if (led_state.raw != prev_led_state.raw) {
        toggle_state(label_caps, LV_STATE_PRESSED, led_state.caps_lock);
        toggle_state(label_num, LV_STATE_PRESSED, led_state.num_lock);
        toggle_state(label_scroll, LV_STATE_PRESSED, led_state.scroll_lock);
        prev_led_state.raw = led_state.raw;
        return;
    }

    if (mods != prev_mods) {
        toggle_state(label_shift, LV_STATE_PRESSED, mods & MOD_MASK_SHIFT);
        toggle_state(label_ctrl, LV_STATE_PRESSED, mods & MOD_MASK_CTRL);
        toggle_state(label_alt, LV_STATE_PRESSED, mods & MOD_MASK_ALT);
        toggle_state(label_gui, LV_STATE_PRESSED, mods & MOD_MASK_GUI);
        prev_mods = mods;
        return;
    }

    hid_data_t *hid = get_hid_data();
    if (hid->hid_changed) {
        if (typing) return;
        if (hid->time_changed) {
            lv_label_set_text_fmt(label_time, "%02d:%02d", hid->hours, hid->minutes);
            hid->time_changed = false;
            return;
        }
        if (hid->media_title_changed) {
            lv_label_set_text(label_hid_media_title, hid->media_title);
            hid->media_title_changed = false;
            return;
        }
        if (hid->media_artist_changed) {
            lv_label_set_text(label_hid_media_artist, hid->media_artist);
            hid->media_artist_changed = false;
            return;
        }
        hid->hid_changed = false;
        return;
    }
}

const eh_screen_t eh_screen_home = {
    .init      = screen_home_init,
    .load      = screen_home_load,
    .housekeep = screen_home_housekeep,
};
