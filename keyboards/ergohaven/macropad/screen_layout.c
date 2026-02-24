#include "src/display/eh_display.h"
#include "src/display/eh_keycode_str.h"
#include "src/display/lvgl_helpers.h"
#include "ergohaven.h"
#include "src/eh_settings.h"

LV_FONT_DECLARE(eh_font_montserrat_20);
LV_FONT_DECLARE(eh_font_montserrat_28);

const char *default_layer_label(uint8_t layer) {
    static const char *PROGMEM default_layer_labels[] = {
        "Numbers", "Navigation", "Mouse", "Media", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Eleven", "Twelve", "Thirteen", "Fourteen", "Fifteen",
    };
    return default_layer_labels[layer];
}

uint16_t get_keycode(int layer, int row, int col) {
    uint16_t keycode = dynamic_keymap_get_keycode(layer, row, col);
    if (keycode == KC_TRANSPARENT) keycode = dynamic_keymap_get_keycode(0, row, col);
    return keycode;
}

uint16_t get_encoder_keycode(int layer, int encoder, bool clockwise) {
    uint16_t keycode = dynamic_keymap_get_encoder(layer, encoder, clockwise);
    if (keycode == KC_TRANSPARENT) keycode = dynamic_keymap_get_encoder(0, encoder, clockwise);
    return keycode;
}

/* Screen layout */

static lv_obj_t *screen_layout;

#define NLABELS 15
static lv_obj_t *key_labels[NLABELS];
static uint16_t  label_kc[NLABELS];
static char      label_text[NLABELS][24];
static lv_obj_t *label_layer;

void screen_layout_init(void) {
    screen_layout = lv_obj_create(NULL);
    lv_obj_add_style(screen_layout, &style_screen, 0);
    use_flex_column(screen_layout);
    lv_obj_set_scrollbar_mode(screen_layout, LV_SCROLLBAR_MODE_OFF);

    label_layer = lv_label_create(screen_layout);
    lv_label_set_text(label_layer, "");
    lv_obj_set_style_pad_top(label_layer, 25, 0);
    lv_obj_set_style_pad_bottom(label_layer, 25, 0);
    lv_obj_set_style_text_color(label_layer, accent_color_blue, 0);
    lv_obj_set_style_text_font(label_layer, &eh_font_montserrat_28, LV_PART_MAIN);

    lv_obj_t *cont = lv_obj_create(screen_layout);
    lv_obj_set_size(cont, 232, 250);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    int32_t v = 0;
    lv_obj_set_style_pad_row(cont, v, 0);
    lv_obj_set_style_pad_column(cont, v, 0);
    lv_obj_add_style(cont, &style_container, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    for (uint32_t i = 0; i < NLABELS; i++) {
        if (i == 12) {
            lv_obj_t *obj = lv_obj_create(cont);
            lv_obj_set_size(obj, 231, 5);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
            lv_obj_add_style(obj, &style_screen, 0);
            lv_obj_set_style_border_opa(obj, 0, 0);
        }
        lv_obj_t *obj = lv_obj_create(cont);
        lv_obj_set_size(obj, 77, 45);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(obj, &style_screen, 0);
        lv_obj_set_style_border_width(obj, 1, 0);

        key_labels[i] = lv_label_create(obj);
        lv_obj_center(key_labels[i]);
        lv_obj_set_style_text_font(key_labels[i], &eh_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_align(key_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text_static(key_labels[i], "");
        label_kc[i] = 0;

        if (i >= 12) {
            lv_obj_set_style_border_opa(obj, 0, 0);
            lv_obj_set_style_text_color(key_labels[i], accent_color_blue, 0);
        }
    }
}

static uint8_t prev_layer = 255;
static int     lbl_idx    = 0;

void screen_layout_load(void) {
    prev_layer = 255;
    lbl_idx    = 0;
    lv_scr_load(screen_layout);
}

void screen_layout_housekeep(void) {
    static uint32_t update_timer = 0;
    if (timer_elapsed32(update_timer) < 5) // prevent long display updates
        return;

    uint8_t layer = get_current_layer();
    if (layer != prev_layer || layer_name_updated) {
        prev_layer = layer;
        lv_label_set_text(label_layer, get_layer_label(layer));
        update_timer       = timer_read32();
        lbl_idx            = 0;
        layer_name_updated = false;
        return;
    }

    if (lbl_idx >= NLABELS) {
        lbl_idx = 0;
    }

    const uint8_t TABLE[NLABELS - 3][2] = {
        {1, 0}, {1, 1}, {1, 2}, //
        {2, 0}, {2, 1}, {2, 2}, //
        {3, 0}, {3, 1}, {3, 2}, //
        {4, 0}, {4, 1}, {4, 2}, //
    };

    uint16_t keycode = KC_TRANSPARENT;
    if (lbl_idx < 12)
        keycode = get_keycode(layer, TABLE[lbl_idx][0], TABLE[lbl_idx][1]);
    else if (lbl_idx == 13)
        keycode = get_keycode(layer, 0, 2);
    else if (lbl_idx == 12)
        keycode = get_encoder_keycode(layer, 0, false);
    else if (lbl_idx == 14)
        keycode = get_encoder_keycode(layer, 0, true);
    if (keycode != label_kc[lbl_idx]) {
        get_keycode_str(label_text[lbl_idx], keycode);
        lv_label_set_text_static(key_labels[lbl_idx], label_text[lbl_idx]);
        label_kc[lbl_idx] = keycode;
        update_timer      = timer_read32();
    }
    lbl_idx += 1;
}

const eh_screen_t eh_screen_layout = {
    .init      = screen_layout_init,
    .load      = screen_layout_load,
    .housekeep = screen_layout_housekeep,
};
