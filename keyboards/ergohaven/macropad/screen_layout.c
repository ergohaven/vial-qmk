#include "ergohaven_display.h"
#include "ergohaven_keycode_str.h"
#include "lvgl_helpers.h"
#include "ergohaven.h"

LV_FONT_DECLARE(ergohaven_symbols_20);
LV_FONT_DECLARE(ergohaven_symbols_28);
LV_FONT_DECLARE(custom_icons_20);

static const char *PROGMEM LAYER_NAME[] = {
    "MAIN", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
};

const char *layer_upper_name(uint8_t layer) {
    if (layer < 16) {
        return LAYER_NAME[layer];
    } else {
        return "UNDEFINED";
    }
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

static lv_obj_t *key_labels[15];
static lv_obj_t *label_layer_small;

void screen_layout_init(void) {
    screen_layout = lv_obj_create(NULL);
    lv_obj_add_style(screen_layout, &style_screen, 0);
    use_flex_column(screen_layout);
    lv_obj_set_scrollbar_mode(screen_layout, LV_SCROLLBAR_MODE_OFF);

    label_layer_small = lv_label_create(screen_layout);
    lv_label_set_text(label_layer_small, "");
    lv_obj_set_style_pad_top(label_layer_small, 25, 0);
    lv_obj_set_style_pad_bottom(label_layer_small, 25, 0);
    lv_obj_set_style_text_color(label_layer_small, accent_color_blue, 0);
    lv_obj_set_style_text_font(label_layer_small, &ergohaven_symbols_28, LV_PART_MAIN);

    lv_obj_t *cont = lv_obj_create(screen_layout);
    lv_obj_set_size(cont, 232, 250);
    // lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    int32_t v = 0;
    lv_obj_set_style_pad_row(cont, v, 0);
    lv_obj_set_style_pad_column(cont, v, 0);
    lv_obj_add_style(cont, &style_container, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    uint32_t i;
    for (i = 0; i < 15; i++) {
        if (i == 12) {
            lv_obj_t *obj = lv_obj_create(cont);
            lv_obj_set_size(obj, 231, 10);
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
        lv_obj_set_style_text_font(key_labels[i], &custom_icons_20, LV_PART_MAIN);
        lv_obj_set_style_text_align(key_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(key_labels[i], "");

        if (i >= 12) {
            lv_obj_set_style_border_opa(obj, 0, 0);
            lv_obj_set_style_text_color(key_labels[i], accent_color_blue, 0);
        }
    }
}

static uint8_t prev_layer         = 255;
static int     update_label_index = 0;

void screen_layout_load(void) {
    prev_layer         = 255;
    update_label_index = 0;
    lv_scr_load(screen_layout);
}

void screen_layout_housekeep(void) {
    uint8_t layer = get_current_layer();
    if (layer != prev_layer) {
        prev_layer = layer;
        lv_label_set_text(label_layer_small, get_layer_label(layer));
        update_label_index = 0;
        return;
    }

    if (update_label_index >= 15) return;

    uint16_t keycode = KC_TRANSPARENT;
    if (update_label_index < 12)
        keycode = get_keycode(layer, update_label_index / 3 + 1, update_label_index % 3);
    else if (update_label_index == 13)
        keycode = get_keycode(layer, 0, 2);
    else if (update_label_index == 12)
        keycode = get_encoder_keycode(layer, 0, false);
    else if (update_label_index == 14)
        keycode = get_encoder_keycode(layer, 0, true);
    lv_label_set_text(key_labels[update_label_index], keycode_to_str(keycode));
    update_label_index += 1;
}

const eh_screen_t eh_screen_layout = {
    .init      = screen_layout_init,
    .load      = screen_layout_load,
    .housekeep = screen_layout_housekeep,
};
