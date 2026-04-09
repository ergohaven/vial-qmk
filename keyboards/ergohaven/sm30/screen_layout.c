#include "ergohaven.h"
#include "src/eh_settings.h"
#include "src/display/eh_display.h"
#include "src/display/eh_keycode_str.h"
#include "src/display/lvgl_helpers.h"

LV_FONT_DECLARE(eh_font_iosevka_16);
LV_FONT_DECLARE(eh_font_iosevka_22);
LV_FONT_DECLARE(eh_font_iosevka_28);

const char *default_layer_label(uint8_t layer) {
    static const char *PROGMEM default_layer_labels[] = {
        "MAIN", "EXTRA", "RAISE", "ADJST", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVN", "TWLVE", "THRTN", "FRTN", "FIFTN",
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

#define NLABELS 29
static lv_obj_t *key_labels[NLABELS];
static bool      label_big[NLABELS];
static uint16_t  label_kc[NLABELS];
static char      label_text[NLABELS][24];
static lv_obj_t *label_layer_small;

void screen_layout_init(void) {
    screen_layout = lv_obj_create(NULL);
    lv_obj_add_style(screen_layout, &style_screen, 0);
    use_flex_column(screen_layout);
    lv_obj_set_scrollbar_mode(screen_layout, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *cont = lv_obj_create(screen_layout);
    lv_obj_set_style_pad_top(screen_layout, 30, 0);
    lv_obj_set_style_pad_bottom(screen_layout, 10, 0);
    lv_obj_set_style_pad_right(screen_layout, 5, 0);

    const int CELL_W = 38;
    const int CELL_H = 44;
    lv_obj_set_size(cont, CELL_W * 6 + 2, CELL_H * 5 + 2);
    lv_obj_set_style_pad_top(cont, 0, 0);
    lv_obj_set_style_pad_bottom(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    int32_t v = 0;
    lv_obj_set_style_pad_row(cont, v, 0);
    lv_obj_set_style_pad_column(cont, v, 0);
    lv_obj_add_style(cont, &style_container, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    for (uint32_t i = 0; i < NLABELS; i++) {
        if (i == 0) {
            lv_obj_t *obj = lv_obj_create(cont);
            lv_obj_set_size(obj, CELL_W / 2, CELL_H);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
            lv_obj_add_style(obj, &style_screen, 0);
            lv_obj_set_style_border_opa(obj, 0, 0);
        }
        lv_obj_t *obj = lv_obj_create(cont);
        lv_obj_set_size(obj, CELL_W, CELL_H);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(obj, &style_screen, 0);
        lv_obj_set_style_border_width(obj, 1, 0);

        key_labels[i] = lv_label_create(obj);
        lv_obj_center(key_labels[i]);
        lv_obj_set_style_text_font(key_labels[i], &eh_font_iosevka_22, LV_PART_MAIN);
        lv_obj_set_style_text_align(key_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text_static(key_labels[i], "");
        label_big[i] = true;
        label_kc[i]  = 0;
    }

    label_layer_small = lv_label_create(screen_layout);
    lv_label_set_text(label_layer_small, "");
    lv_obj_set_style_pad_top(label_layer_small, 10, 0);
    lv_obj_set_style_pad_bottom(label_layer_small, 0, 0);
    // lv_obj_set_style_text_color(label_layer_small, accent_color_blue, 0);
    lv_obj_set_style_text_font(label_layer_small, &eh_font_iosevka_28, LV_PART_MAIN);
}

static uint8_t prev_layer = 255;
static int     lbl_idx    = 0;

void screen_layout_load(void) {
    prev_layer = 255;
    lbl_idx    = 0;
    lv_scr_load(screen_layout);
}

size_t visible_len(const char *s) {
    size_t len = 0;
    while (*s != '\0') {
        unsigned char p = (unsigned char)(*s);
        if (p < 0x80) { // 1-byte ASCII
            len += 1;
            s++;
        } else if (p < 0xE0) { // 2-byte
            len += 1;
            s += 2;
        } else if (p < 0xF0) { // 3-byte
            len += 2;
            s += 3;
        } else { // 4-byte (simplified, assumes valid input)
            len += 2;
            s += 4;
        }
    }
    return len;
}

void screen_layout_housekeep(void) {
    static uint32_t update_timer = 0;
    if (timer_elapsed32(update_timer) < 5) // prevent long display updates
        return;

    uint8_t layer = get_current_layer();
    if (layer != prev_layer || layer_name_updated) {
        prev_layer = layer;
        lv_label_set_text(label_layer_small, get_layer_label(layer));
        update_timer       = timer_read32();
        lbl_idx            = 0;
        layer_name_updated = false;
        return;
    }

    if (lbl_idx >= NLABELS) {
        lbl_idx = 0;
    }

    const uint8_t TABLE[NLABELS][2] = {
        {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0},         //
        {0, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1}, {1, 0}, //
        {1, 5}, {2, 4}, {2, 3}, {2, 2}, {2, 1}, {2, 0}, //
        {2, 5}, {3, 4}, {3, 3}, {3, 2}, {3, 1}, {3, 0}, //
        {3, 5}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, //
    };

    uint16_t keycode = get_keycode(layer, TABLE[lbl_idx][0], TABLE[lbl_idx][1]);
    if (keycode != label_kc[lbl_idx]) {
        get_keycode_str(label_text[lbl_idx], keycode);
        int len = visible_len(label_text[lbl_idx]);

        if (len <= 3 && !label_big[lbl_idx]) {
            label_big[lbl_idx] = true;
            lv_obj_set_style_text_font(key_labels[lbl_idx], &eh_font_iosevka_22, LV_PART_MAIN);
        } else if (len > 3 && label_big[lbl_idx]) {
            label_big[lbl_idx] = false;
            lv_obj_set_style_text_font(key_labels[lbl_idx], &eh_font_iosevka_16, LV_PART_MAIN);
        }
        lv_label_set_text_static(key_labels[lbl_idx], label_text[lbl_idx]);
        label_kc[lbl_idx] = keycode;
        update_timer      = timer_read32();
        set_activity_timestamps(update_timer, update_timer, update_timer);
    }
    lbl_idx += 1;
}

const eh_screen_t eh_screen_layout = {
    .init      = screen_layout_init,
    .load      = screen_layout_load,
    .housekeep = screen_layout_housekeep,
};
