#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "lvgl_helpers.h"
#include "hid.h"

static lv_obj_t *screen_volume;
static lv_obj_t *arc_volume;
static lv_obj_t *label_volume_arc;
static lv_obj_t *volume_text_label;

// Overlap frequent host updates without stopping between each sample.
// Keep fractional percentages so a small encoder step has intermediate angles.
#define VOLUME_ANIMATION_MS 100
#define VOLUME_SCALE 100

static int16_t  displayed_volume = -1;
static int16_t  displayed_label  = -1;
static uint8_t  previous_volume  = UINT8_MAX;

void screen_volume_apply_accent_color(void) {
    if (label_volume_arc) lv_obj_set_style_text_color(label_volume_arc, accent_color_blue, LV_PART_MAIN);
    if (volume_text_label) lv_obj_set_style_text_color(volume_text_label, accent_color_blue, LV_PART_MAIN);
    if (arc_volume == NULL) return;

    lv_obj_set_style_arc_color(arc_volume, accent_color_blue, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc_volume, accent_color_blue, LV_PART_KNOB);
}

static void set_displayed_volume(void *obj, int32_t value) {
    displayed_volume = value;
    lv_arc_set_value(obj, value);
    int16_t percent = (value + VOLUME_SCALE / 2) / VOLUME_SCALE;
    if (percent != displayed_label) {
        displayed_label = percent;
        lv_label_set_text_fmt(label_volume_arc, "%02d", (int)percent);
    }
}

static void animate_volume_to(uint8_t target) {
    int32_t end = MIN(target, 100) * VOLUME_SCALE;
    int32_t start = displayed_volume < 0 ? end : displayed_volume;

    if (start == end) {
        lv_anim_del(arc_volume, set_displayed_volume);
        set_displayed_volume(arc_volume, end);
        return;
    }

    // Retarget the running animation without deleting/recreating it. When it
    // is the last animation, recreation resets LVGL's elapsed-time baseline;
    // frequent host packets can then discard time and make the arc lag behind.
    lv_anim_t *running = lv_anim_get(arc_volume, set_displayed_volume);
    if (running) {
        lv_anim_set_values(running, start, end);
        running->current_value = start;
        running->act_time = 0;
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, arc_volume);
    lv_anim_set_exec_cb(&animation, set_displayed_volume);
    lv_anim_set_time(&animation, VOLUME_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_values(&animation, start, end);
    lv_anim_start(&animation);
}

void screen_volume_init(void) {
    screen_volume = lv_obj_create(NULL);
    lv_obj_add_style(screen_volume, &style_screen, 0);

    arc_volume = lv_arc_create(screen_volume);
    lv_obj_set_size(arc_volume, 200, 200);
    lv_arc_set_range(arc_volume, 0, 100 * VOLUME_SCALE);
    lv_obj_center(arc_volume);
    screen_volume_apply_accent_color();

    label_volume_arc = lv_label_create(screen_volume);
    lv_label_set_text(label_volume_arc, "00");
    lv_obj_set_style_text_font(label_volume_arc, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(label_volume_arc);

    volume_text_label = lv_label_create(screen_volume);
    lv_label_set_text(volume_text_label, "Volume");
    lv_obj_align(volume_text_label, LV_ALIGN_BOTTOM_MID, 0, -50);
    screen_volume_apply_accent_color();
}

void screen_volume_load(void) {
    if (!is_hid_volume_active()) return;
    uint8_t volume = MIN(get_hid_data()->volume, 100);
    previous_volume = get_hid_data()->volume;
    lv_anim_del(arc_volume, set_displayed_volume);
    set_displayed_volume(arc_volume, volume * VOLUME_SCALE);
    lv_scr_load(screen_volume);
    display_apply_brightness();
}

void screen_volume_housekeep(void) {
    hid_data_t *hid = get_hid_data();
    // Only animate authoritative host values, never a local estimate.
    if (is_hid_volume_active() && (hid->volume_changed || hid->volume != previous_volume)) {
        previous_volume = hid->volume;
        animate_volume_to(hid->volume);
    }
    hid->volume_changed = false;
}

const eh_screen_t eh_screen_volume = {
    .init      = screen_volume_init,
    .load      = screen_volume_load,
    .housekeep = screen_volume_housekeep,
};
