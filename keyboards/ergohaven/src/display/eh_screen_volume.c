#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "lvgl_helpers.h"
#include "hid.h"

static lv_obj_t *screen_volume;
static lv_obj_t *arc_volume;
static lv_obj_t *label_volume_arc;

void screen_volume_init(void) {
    screen_volume = lv_obj_create(NULL);
    lv_obj_add_style(screen_volume, &style_screen, 0);

    arc_volume = lv_arc_create(screen_volume);
    lv_obj_set_size(arc_volume, 200, 200);
    lv_obj_center(arc_volume);

    label_volume_arc = lv_label_create(screen_volume);
    lv_label_set_text(label_volume_arc, "00");
    lv_obj_set_style_text_font(label_volume_arc, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(label_volume_arc);

    lv_obj_t *volume_text_label = lv_label_create(screen_volume);
    lv_label_set_text(volume_text_label, "Volume");
    lv_obj_align(volume_text_label, LV_ALIGN_BOTTOM_MID, 0, -50);
}

void screen_volume_load(void) {
    lv_scr_load(screen_volume);
}

void screen_volume_housekeep(void) {
    static uint8_t prev_volume = -1;
    hid_data_t    *hid         = get_hid_data();
    if (hid->volume != prev_volume) {
        prev_volume = hid->volume;
        lv_arc_set_value(arc_volume, hid->volume);
        lv_label_set_text_fmt(label_volume_arc, "%02d", hid->volume);
    }
    hid->volume_changed = false;
}

const eh_screen_t eh_screen_volume = {
    .init      = screen_volume_init,
    .load      = screen_volume_load,
    .housekeep = screen_volume_housekeep,
};
