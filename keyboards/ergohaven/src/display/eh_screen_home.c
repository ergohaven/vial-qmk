#include "eh_display.h"
#include "eh_display_bounds.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "src/eh_ruen.h"
#include "../eh_settings.h"
#include "lvgl_helpers.h"
#include "hid.h"
#include "eh_symbols.h"
#ifdef EH_STANDBY_BACKGROUND_ENABLE
#    include "eh_background.h"
#endif

LV_FONT_DECLARE(eh_font_montserrat_20);
LV_FONT_DECLARE(eh_font_montserrat_28);
#ifdef EH_CLOCK_FONT_CHOICES_ENABLE
LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_28);
LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_40);
LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_48);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_28);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_40);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_48);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_28);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_40);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_48);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_28);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_40);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_48);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_28);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_40);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_48);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_28);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_40);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_48);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_28);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_40);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_48);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_28);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_40);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_48);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_28);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_40);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_48);
#endif

#ifdef EH_DATE_SETTINGS_ENABLE
#include "eh_date_settings.h"
LV_FONT_DECLARE(eh_font_clock_montserrat_64);
LV_FONT_DECLARE(eh_font_clock_montserrat_28);
LV_FONT_DECLARE(eh_font_clock_montserrat_40);
LV_FONT_DECLARE(eh_font_clock_montserrat_48);

LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_64);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_64);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_64);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_64);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_64);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_64);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_64);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_64);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_64);
LV_FONT_DECLARE(eh_font_date_montserrat_20);
LV_FONT_DECLARE(eh_font_date_ubuntu_sans_20);
LV_FONT_DECLARE(eh_font_date_ubuntu_mono_20);
LV_FONT_DECLARE(eh_font_date_liberation_mono_20);
LV_FONT_DECLARE(eh_font_date_dejavu_sans_20);
LV_FONT_DECLARE(eh_font_date_dejavu_serif_20);
LV_FONT_DECLARE(eh_font_date_dejavu_mono_20);
LV_FONT_DECLARE(eh_font_date_liberation_sans_20);
LV_FONT_DECLARE(eh_font_date_liberation_serif_20);
LV_FONT_DECLARE(eh_font_date_liberation_narrow_20);
static lv_obj_t *label_date;
#endif
static lv_obj_t *screen_home;
#ifdef EH_STANDBY_BACKGROUND_ENABLE
static lv_obj_t *standby_background;
static lv_obj_t *standby_background_dim;
#endif
static lv_obj_t *label_product;
static lv_obj_t *label_time;
static lv_obj_t *label_time_colon;
static lv_obj_t *label_time_minutes;
static lv_obj_t *standby_header;
static lv_obj_t *label_layer_icon;
static lv_obj_t *label_layer;
static lv_obj_t *label_mac;
static lv_obj_t *label_layout;
static lv_obj_t *label_hid_media_artist;
static lv_obj_t *label_hid_media_title;
static lv_obj_t *screen_home_media;
#ifndef EH_HOME_HIDE_MODIFIERS
static lv_obj_t *label_shift;
static lv_obj_t *label_ctrl;
static lv_obj_t *label_alt;
static lv_obj_t *label_gui;
static lv_obj_t *label_num;
static lv_obj_t *label_caps;
static lv_obj_t *label_scroll;
static lv_obj_t *screen_home_mods;
#endif
static lv_obj_t *clock_custom;
static uint8_t   clock_hours;
static uint8_t   clock_minutes;
static uint8_t   flip_previous_digits[4];
static uint8_t   flip_changed_mask;
static int32_t   flip_animation_progress = 100;
static bool      clock_time_initialized;
static bool      clock_colon_visible = true;
#ifdef EH_STANDBY_BACKGROUND_ENABLE
static lv_img_dsc_t standby_background_frames[EH_BACKGROUND_MAX_FRAMES];
static uint32_t standby_background_generation = UINT32_MAX;
static uint32_t standby_background_frame_time;
static uint64_t standby_background_frame_elapsed;
static uint16_t standby_background_speed_remainder;
static uint8_t standby_background_frame;
#endif

#define FLIP_ANIMATION_MS 360

#ifdef EH_STANDBY_BACKGROUND_ENABLE
static void refresh_standby_background(void) {
    uint32_t current_generation = eh_background_generation();
    if (standby_background_generation == current_generation) return;
    standby_background_generation = current_generation;
    standby_background_frame = 0;
    standby_background_frame_time = timer_read32();
    standby_background_frame_elapsed = 0;
    standby_background_speed_remainder = 0;

    uint8_t frame_count = eh_background_frame_count();
    bool animation = eh_background_kind() == EH_BACKGROUND_KIND_ANIMATION;
    uint32_t frame_size = animation ? EH_BACKGROUND_ANIMATION_FRAME_SIZE : EH_BACKGROUND_IMAGE_FRAME_SIZE;
    for (uint8_t frame = 0; frame < frame_count; frame++) {
        standby_background_frames[frame] = (lv_img_dsc_t){
            .header.always_zero = 0,
            .header.w = EH_BACKGROUND_WIDTH,
            .header.h = EH_BACKGROUND_HEIGHT,
            .data_size = frame_size,
            .header.cf = animation ? LV_IMG_CF_RAW_ALPHA : LV_IMG_CF_INDEXED_8BIT,
            .data = eh_background_frame_data(frame),
        };
    }
    if (frame_count > 0) lv_img_set_src(standby_background, &standby_background_frames[0]);
    toggle_hidden(standby_background, frame_count > 0);
    toggle_hidden(standby_background_dim, frame_count > 0 && get_clock_background_dim() > 0);
}

static void animate_standby_background(void) {
    uint32_t now = timer_read32();
    uint32_t real_elapsed = TIMER_DIFF_32(now, standby_background_frame_time);
    standby_background_frame_time = now;
    if (eh_background_animation_paused()) return;
    uint8_t frame_count = eh_background_frame_count();
    if (frame_count < 2) return;

    uint64_t scaled = (uint64_t)real_elapsed * eh_background_speed_percent() + standby_background_speed_remainder;
    standby_background_frame_elapsed += scaled / 100u;
    standby_background_speed_remainder = (uint16_t)(scaled % 100u);

    uint32_t cycle_duration = 0;
    for (uint8_t frame = 0; frame < frame_count; frame++) {
        cycle_duration += MAX(1u, (uint32_t)eh_background_frame_delay(frame));
    }
    if (cycle_duration > 0 && standby_background_frame_elapsed >= cycle_duration) {
        standby_background_frame_elapsed %= cycle_duration;
    }

    bool advanced = false;
    for (uint8_t skipped = 0; skipped < frame_count; skipped++) {
        uint32_t delay = MAX(1u, (uint32_t)eh_background_frame_delay(standby_background_frame));
        if (standby_background_frame_elapsed < delay) break;
        standby_background_frame_elapsed -= delay;
        standby_background_frame = (standby_background_frame + 1) % frame_count;
        advanced = true;
    }
    if (advanced) lv_img_set_src(standby_background, &standby_background_frames[standby_background_frame]);
}
#endif

enum {
    CLOCK_STYLE_MODERN,
    CLOCK_STYLE_SEGMENT_STRAIGHT,
    CLOCK_STYLE_SEGMENT_ITALIC,
    CLOCK_STYLE_TUBE,
    CLOCK_STYLE_FLIP,
    CLOCK_STYLE_DOT_MATRIX,
    CLOCK_STYLE_NEON,
};

static void apply_clock_element_visibility(void) {
    // Encoder rotation must not hide an already visible standby clock/date.
    // Only physical matrix activity starts a new appearance-delay interval.
    bool clock_visible = get_clock_visible() && is_hid_time_active();
    toggle_hidden(label_time, clock_visible);
    toggle_hidden(label_time_colon, clock_visible && clock_colon_visible);
    toggle_hidden(label_time_minutes, clock_visible);
    toggle_hidden(clock_custom, false);

    bool info_visible = get_clock_info_visible();
    toggle_hidden(label_layer_icon, info_visible);
    toggle_hidden(label_layer, info_visible);
    toggle_hidden(label_mac, info_visible && split_get_mac());
    toggle_hidden(label_layout, info_visible);
    lv_coord_t name_width = split_get_mac() ? 80 : 110;
    if (lv_obj_get_style_width(label_layer,0) != name_width) lv_obj_set_width(label_layer,name_width);


}

static const lv_font_t *clock_font(void) {
#ifdef EH_CLOCK_FONT_CHOICES_ENABLE
    static const lv_font_t *const fonts[10][4] = {
        {&eh_font_clock_montserrat_28, &eh_font_clock_montserrat_40, &eh_font_clock_montserrat_48, &eh_font_clock_montserrat_64},
        {&eh_font_clock_ubuntu_sans_28, &eh_font_clock_ubuntu_sans_40, &eh_font_clock_ubuntu_sans_48, &eh_font_clock_ubuntu_sans_64},
        {&eh_font_clock_ubuntu_mono_28, &eh_font_clock_ubuntu_mono_40, &eh_font_clock_ubuntu_mono_48, &eh_font_clock_ubuntu_mono_64},
        {&eh_font_clock_liberation_mono_28, &eh_font_clock_liberation_mono_40, &eh_font_clock_liberation_mono_48, &eh_font_clock_liberation_mono_64},
        {&eh_font_clock_dejavu_sans_28, &eh_font_clock_dejavu_sans_40, &eh_font_clock_dejavu_sans_48, &eh_font_clock_dejavu_sans_64},
        {&eh_font_clock_dejavu_serif_28, &eh_font_clock_dejavu_serif_40, &eh_font_clock_dejavu_serif_48, &eh_font_clock_dejavu_serif_64},
        {&eh_font_clock_dejavu_mono_28, &eh_font_clock_dejavu_mono_40, &eh_font_clock_dejavu_mono_48, &eh_font_clock_dejavu_mono_64},
        {&eh_font_clock_liberation_sans_28, &eh_font_clock_liberation_sans_40, &eh_font_clock_liberation_sans_48, &eh_font_clock_liberation_sans_64},
        {&eh_font_clock_liberation_serif_28, &eh_font_clock_liberation_serif_40, &eh_font_clock_liberation_serif_48, &eh_font_clock_liberation_serif_64},
        {&eh_font_clock_liberation_narrow_28, &eh_font_clock_liberation_narrow_40, &eh_font_clock_liberation_narrow_48, &eh_font_clock_liberation_narrow_64},
    };
    return fonts[MIN(get_clock_style(), 9)][MIN(get_clock_size(), 3)];
#else
    static const lv_font_t *const fonts[] = {&lv_font_montserrat_28, &lv_font_montserrat_40, &lv_font_montserrat_48};
    return fonts[MIN(get_clock_size(), 2)];
#endif
}

static lv_coord_t clock_height(void) {
    static const lv_coord_t heights[] = {32, 46, 60, 78};
    return heights[MIN(get_clock_size(), 3)];
}

static lv_coord_t clock_aligned_x(const lv_area_t *area, lv_coord_t width) {
    switch (get_clock_alignment()) {
        case 0:
            return area->x1 + 8;
        case 2:
            return area->x2 + 1 - width - 8;
        default:
            return area->x1 + (lv_area_get_width(area) - width) / 2;
    }
}

#ifdef EH_DATE_SETTINGS_ENABLE
/* Vertical ink bounds relative to the label, including its font padding. */
static bool label_ink_y(lv_obj_t *label, lv_coord_t *top, lv_coord_t *bottom) {
    if (!label) return false;
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const char *text = lv_label_get_text(label);
    uint32_t index = 0, letter;
    *top = 32767; *bottom = -32767;
    while ((letter = _lv_txt_encoded_next(text, &index))) {
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, letter, 0) || !glyph.box_w || !glyph.box_h) continue;
        lv_coord_t y = lv_obj_get_style_pad_top(label, 0) + font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
        *top = MIN(*top, y); *bottom = MAX(*bottom, y + glyph.box_h);
    }
    return *bottom > *top;
}
#endif

/* Use the same slots even while an element is disabled/delayed, so toggles do
 * not make the clock jump. Include the colon on its blink-off frame as well. */
static lv_coord_t clock_vertical_center_twice(void) {
    lv_coord_t header_bottom = 39, date_top = 115;
#ifdef EH_DATE_SETTINGS_ENABLE
    lv_coord_t top, bottom, visible_bottom = -32767;
    lv_obj_t *header_labels[] = {label_layer_icon, label_layer, label_layout, label_mac};
    for (unsigned i = 0; i < ARRAY_SIZE(header_labels); i++) {
        if (header_labels[i] == label_mac && !split_get_mac()) continue;
        if (label_ink_y(header_labels[i], &top, &bottom)) visible_bottom = MAX(visible_bottom, 11 + bottom);
    }
    if (visible_bottom != -32767) header_bottom = visible_bottom;
    if (label_ink_y(label_date, &top, &bottom)) date_top = 115 + top;
#endif
    return header_bottom + date_top;
}

/* Center the visible glyphs, not the font's advance/baseline padding. The
 * colon participates even while hidden, keeping the blinking clock stationary. */
static void position_clock_labels(void) {
    const lv_font_t *font = clock_font();
    lv_obj_t *labels[] = {label_time, label_time_colon, label_time_minutes};
    lv_coord_t starts[3], widths[3], pen = 0;
    lv_coord_t left = 32767, top = 32767, right = -32767, bottom = -32767;
    for (uint8_t part = 0; part < 3; part++) {
        const char *text = lv_label_get_text(labels[part]);
        starts[part] = pen;
        lv_coord_t local = 0, local_right = 0;
        for (uint8_t n = 0; text[n]; n++) {
            lv_font_glyph_dsc_t glyph;
            if (!lv_font_get_glyph_dsc(font, &glyph, text[n], text[n + 1])) continue;
            if (glyph.box_w && glyph.box_h) {
                lv_coord_t gx = pen + local + glyph.ofs_x;
                lv_coord_t gy = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
                left = MIN(left, gx);
                right = MAX(right, gx + glyph.box_w);
                top = MIN(top, gy);
                bottom = MAX(bottom, gy + glyph.box_h);
                local_right = MAX(local_right, local + glyph.ofs_x + glyph.box_w);
            }
            local += glyph.adv_w;
        }
        widths[part] = MAX(local, local_right) + 2;
        pen += local;
    }
    if (right <= left || bottom <= top) return;
    const lv_area_t area = {.x1 = 10, .y1 = 28, .x2 = 219, .y2 = 107};
    lv_coord_t x = clock_aligned_x(&area, right - left) - left;
    lv_coord_t y = (clock_vertical_center_twice() - (bottom - top)) / 2 - top;
    for (uint8_t part = 0; part < 3; part++) {
        lv_obj_set_pos(labels[part], x + starts[part], y);
        lv_obj_set_size(labels[part], widths[part], font->line_height + 4);
    }
}

static void update_clock_label(void) {
    if (label_time == NULL || label_time_colon == NULL || label_time_minutes == NULL) return;
    lv_label_set_text_fmt(label_time, "%02d", clock_hours);
    lv_label_set_text(label_time_colon, ":");
    lv_label_set_text_fmt(label_time_minutes, "%02d", clock_minutes);
    position_clock_labels();
    toggle_hidden(label_time_colon, get_clock_visible() && is_hid_time_active() && clock_colon_visible);
}

static void update_clock_colon(lv_timer_t *timer) {
    (void)timer;
    bool visible = get_clock_colon_blink() ? !clock_colon_visible : true;
    if (visible == clock_colon_visible) return;
    clock_colon_visible = visible;
    update_clock_label();
    if (clock_custom != NULL) lv_obj_invalidate(clock_custom);
}

static void draw_clock_dot(lv_draw_ctx_t *draw_ctx, lv_coord_t x, lv_coord_t y, lv_coord_t radius, lv_color_t color, lv_opa_t opa) {
    lv_area_t area = {.x1 = x - radius, .y1 = y - radius, .x2 = x + radius, .y2 = y + radius};
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = color;
    dot.bg_opa   = opa;
    dot.radius   = LV_RADIUS_CIRCLE;
    dot.border_width = 0;
    lv_draw_rect(draw_ctx, &dot, &area);
}

static lv_coord_t segment_skew(bool italic, lv_coord_t height, lv_coord_t y) {
    return italic ? (height - y) / 7 : 0;
}

static void draw_segment_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                               lv_coord_t line_width, bool italic, bool rounded, lv_color_t color, lv_opa_t opa, bool draw_all) {
    static const uint8_t masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t mask = draw_all ? 0x7F : masks[digit % 10];
    lv_coord_t pad = MAX(1, line_width / 2);
    lv_coord_t mid = height / 2;
    lv_point_t segments[7][2] = {
        {{x + pad + segment_skew(italic, height, 0), y}, {x + width - pad + segment_skew(italic, height, 0), y}},
        {{x + width + segment_skew(italic, height, pad), y + pad}, {x + width + segment_skew(italic, height, mid - pad), y + mid - pad}},
        {{x + width + segment_skew(italic, height, mid + pad), y + mid + pad}, {x + width + segment_skew(italic, height, height - pad), y + height - pad}},
        {{x + pad + segment_skew(italic, height, height), y + height}, {x + width - pad + segment_skew(italic, height, height), y + height}},
        {{x + segment_skew(italic, height, mid + pad), y + mid + pad}, {x + segment_skew(italic, height, height - pad), y + height - pad}},
        {{x + segment_skew(italic, height, pad), y + pad}, {x + segment_skew(italic, height, mid - pad), y + mid - pad}},
        {{x + pad + segment_skew(italic, height, mid), y + mid}, {x + width - pad + segment_skew(italic, height, mid), y + mid}},
    };
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color       = color;
    line.opa         = opa;
    line.width       = line_width;
    line.round_start = rounded;
    line.round_end   = rounded;
    for (uint8_t segment = 0; segment < 7; segment++) {
        if (mask & (1U << segment)) lv_draw_line(draw_ctx, &line, &segments[segment][0], &segments[segment][1]);
    }
}

static void draw_hex_segment(lv_draw_ctx_t *draw_ctx, lv_point_t start, lv_point_t end, lv_coord_t thickness, bool horizontal,
                             lv_color_t color) {
    lv_coord_t half  = MAX(1, thickness / 2);
    lv_coord_t bevel = half;
    lv_point_t points[6];

    if (horizontal) {
        points[0] = (lv_point_t){.x = start.x + bevel, .y = start.y - half};
        points[1] = (lv_point_t){.x = end.x - bevel, .y = end.y - half};
        points[2] = end;
        points[3] = (lv_point_t){.x = end.x - bevel, .y = end.y + half};
        points[4] = (lv_point_t){.x = start.x + bevel, .y = start.y + half};
        points[5] = start;
    } else {
        points[0] = start;
        points[1] = (lv_point_t){.x = start.x + half, .y = start.y + bevel};
        points[2] = (lv_point_t){.x = end.x + half, .y = end.y - bevel};
        points[3] = end;
        points[4] = (lv_point_t){.x = end.x - half, .y = end.y - bevel};
        points[5] = (lv_point_t){.x = start.x - half, .y = start.y + bevel};
    }

    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color     = color;
    fill.bg_opa       = LV_OPA_COVER;
    fill.border_width = 0;
    lv_draw_polygon(draw_ctx, &fill, points, ARRAY_SIZE(points));
}

static void draw_hex_segment_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                   lv_coord_t height, bool italic, lv_color_t color) {
    static const uint8_t masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t mask = masks[digit % 10];
    lv_coord_t thickness = MAX(3, height / 9);
    if ((thickness & 1) == 0) thickness++;

    lv_coord_t half     = thickness / 2;
    lv_coord_t joint    = half + 1;
    lv_coord_t mid      = height / 2;
    lv_coord_t left     = x + half;
    lv_coord_t right    = x + width - half;
    lv_point_t segments[7][2] = {
        {{left + joint + segment_skew(italic, height, 0), y},
         {right - joint + segment_skew(italic, height, 0), y}},
        {{right + segment_skew(italic, height, joint), y + joint},
         {right + segment_skew(italic, height, mid - joint), y + mid - joint}},
        {{right + segment_skew(italic, height, mid + joint), y + mid + joint},
         {right + segment_skew(italic, height, height - joint), y + height - joint}},
        {{left + joint + segment_skew(italic, height, height), y + height},
         {right - joint + segment_skew(italic, height, height), y + height}},
        {{left + segment_skew(italic, height, mid + joint), y + mid + joint},
         {left + segment_skew(italic, height, height - joint), y + height - joint}},
        {{left + segment_skew(italic, height, joint), y + joint},
         {left + segment_skew(italic, height, mid - joint), y + mid - joint}},
        {{left + joint + segment_skew(italic, height, mid), y + mid},
         {right - joint + segment_skew(italic, height, mid), y + mid}},
    };

    for (uint8_t segment = 0; segment < 7; segment++) {
        if (mask & (1U << segment)) {
            bool horizontal = segment == 0 || segment == 3 || segment == 6;
            draw_hex_segment(draw_ctx, segments[segment][0], segments[segment][1], thickness, horizontal, color);
        }
    }
}

static void draw_dot_matrix_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, lv_coord_t x, lv_coord_t y, lv_coord_t dot, lv_color_t color) {
    static const uint8_t rows[10][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
        {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
    };
    lv_coord_t step = dot * 2 + 1;
    for (uint8_t row = 0; row < 7; row++) {
        for (uint8_t col = 0; col < 5; col++) {
            if (rows[digit % 10][row] & (1U << (4 - col))) {
                draw_clock_dot(draw_ctx, x + col * step + dot, y + row * step + dot, dot, color, LV_OPA_COVER);
            }
        }
    }
}

static void draw_flip_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, const lv_area_t *area, const lv_font_t *font, lv_color_t text_color,
                            lv_color_t background_color) {
    lv_draw_rect_dsc_t card;
    lv_draw_rect_dsc_init(&card);
    card.bg_color     = lv_color_mix(text_color, background_color, 22);
    card.bg_opa       = LV_OPA_COVER;
    card.radius       = 5;
    card.border_color = lv_color_mix(text_color, background_color, 90);
    card.border_opa   = LV_OPA_50;
    card.border_width = 1;
    lv_draw_rect(draw_ctx, &card, area);

    lv_draw_line_dsc_t split;
    lv_draw_line_dsc_init(&split);
    split.color = background_color;
    split.opa   = LV_OPA_70;
    split.width = 1;
    lv_point_t split_points[2] = {
        {.x = area->x1 + 2, .y = (area->y1 + area->y2) / 2},
        {.x = area->x2 - 2, .y = (area->y1 + area->y2) / 2},
    };
    lv_draw_line(draw_ctx, &split, &split_points[0], &split_points[1]);

    char text[2] = {(char)('0' + digit), '\0'};
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.color = text_color;
    label.font  = font;
    label.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t text_area = *area;
    text_area.y1 += (lv_area_get_height(area) - font->line_height) / 2;
    lv_draw_label(draw_ctx, &label, &text_area, text, NULL);
}

static void draw_flip_digit_clipped(lv_draw_ctx_t *draw_ctx, uint8_t digit, const lv_area_t *area, const lv_area_t *clip,
                                    const lv_font_t *font, lv_color_t text_color, lv_color_t background_color) {
    lv_area_t clipped;
    if (!_lv_area_intersect(&clipped, draw_ctx->clip_area, clip)) return;

    const lv_area_t *saved_clip = draw_ctx->clip_area;
    draw_ctx->clip_area         = &clipped;
    draw_flip_digit(draw_ctx, digit, area, font, text_color, background_color);
    draw_ctx->clip_area = saved_clip;
}

static void draw_animated_flip_digit(lv_draw_ctx_t *draw_ctx, uint8_t previous_digit, uint8_t digit, const lv_area_t *area,
                                     const lv_font_t *font, lv_color_t text_color, lv_color_t background_color) {
    int32_t progress = MIN(flip_animation_progress, 100);
    lv_coord_t middle = (area->y1 + area->y2) / 2;
    lv_coord_t half_height = middle - area->y1;
    lv_area_t top = {.x1 = area->x1, .y1 = area->y1, .x2 = area->x2, .y2 = middle};

    draw_flip_digit(draw_ctx, previous_digit, area, font, text_color, background_color);
    draw_flip_digit_clipped(draw_ctx, digit, area, &top, font, text_color, background_color);

    if (progress < 50) {
        lv_area_t folding_top = top;
        folding_top.y1 += half_height * progress / 50;
        draw_flip_digit_clipped(draw_ctx, previous_digit, area, &folding_top, font, text_color, background_color);
    } else {
        lv_area_t folding_bottom = {.x1 = area->x1,
                                    .y1 = middle,
                                    .x2 = area->x2,
                                    .y2 = middle + half_height * (progress - 50) / 50};
        draw_flip_digit_clipped(draw_ctx, digit, area, &folding_bottom, font, text_color, background_color);
    }

    lv_draw_rect_dsc_t hinge;
    lv_draw_rect_dsc_init(&hinge);
    hinge.bg_color = background_color;
    hinge.bg_opa   = (lv_opa_t)(LV_OPA_20 + (50 - LV_ABS(50 - progress)) * 2);
    hinge.border_width = 0;
    lv_area_t hinge_area = {.x1 = area->x1 + 2, .y1 = middle - 1, .x2 = area->x2 - 2, .y2 = middle + 1};
    lv_draw_rect(draw_ctx, &hinge, &hinge_area);
}

static void set_flip_animation_progress(void *object, int32_t value) {
    flip_animation_progress = value;
    lv_obj_invalidate(object);
}

static void finish_flip_animation(lv_anim_t *animation) {
    (void)animation;
    flip_animation_progress = 100;
    flip_changed_mask       = 0;
    lv_obj_invalidate(clock_custom);
    lv_obj_invalidate(label_time);
    lv_obj_invalidate(label_time_colon);
    lv_obj_invalidate(label_time_minutes);
}

static void start_flip_animation(uint8_t previous_hours, uint8_t previous_minutes, uint8_t hours, uint8_t minutes) {
    uint8_t previous[4] = {previous_hours / 10, previous_hours % 10, previous_minutes / 10, previous_minutes % 10};
    uint8_t current[4]  = {hours / 10, hours % 10, minutes / 10, minutes % 10};
    flip_changed_mask   = 0;
    for (uint8_t index = 0; index < ARRAY_SIZE(current); index++) {
        flip_previous_digits[index] = previous[index];
        if (previous[index] != current[index]) flip_changed_mask |= (1U << index);
    }
    if (flip_changed_mask == 0) return;

    lv_anim_del(clock_custom, set_flip_animation_progress);
    flip_animation_progress = 0;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, clock_custom);
    lv_anim_set_exec_cb(&animation, set_flip_animation_progress);
    lv_anim_set_values(&animation, 0, 100);
    lv_anim_set_time(&animation, FLIP_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&animation, finish_flip_animation);
    lv_anim_start(&animation);
}

static void draw_tube_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, const lv_area_t *area, const lv_font_t *font, lv_color_t text_color,
                            lv_color_t background_color) {
    lv_draw_rect_dsc_t tube;
    lv_draw_rect_dsc_init(&tube);
    tube.bg_color     = lv_color_mix(text_color, background_color, 12);
    tube.bg_opa       = LV_OPA_30;
    tube.radius       = LV_RADIUS_CIRCLE;
    tube.border_color = text_color;
    tube.border_opa   = LV_OPA_40;
    tube.border_width = 1;
    lv_draw_rect(draw_ctx, &tube, area);

    char text[2] = {(char)('0' + digit), '\0'};
    lv_draw_label_dsc_t glow;
    lv_draw_label_dsc_init(&glow);
    glow.color = text_color;
    glow.opa   = LV_OPA_30;
    glow.font  = font;
    glow.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t glow_area = *area;
    glow_area.x1 -= 1;
    glow_area.x2 += 1;
    glow_area.y1 += (lv_area_get_height(area) - font->line_height) / 2;
    lv_draw_label(draw_ctx, &glow, &glow_area, text, NULL);

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.color = text_color;
    label.font  = font;
    label.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t text_area = *area;
    text_area.y1 += (lv_area_get_height(area) - font->line_height) / 2;
    lv_draw_label(draw_ctx, &label, &text_area, text, NULL);
}

static void draw_clock_event(lv_event_t *event) {
    uint8_t style = get_clock_style();
    if (style == CLOCK_STYLE_MODERN) return;

    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(event);
    lv_area_t area;
    lv_obj_get_coords(clock_custom, &area);
    lv_color_t color = lv_color_make(get_clock_text_red(), get_clock_text_green(), get_clock_text_blue());
    lv_color_t background = lv_color_make(get_clock_background_red(), get_clock_background_green(), get_clock_background_blue());
    uint8_t digits[4] = {clock_hours / 10, clock_hours % 10, clock_minutes / 10, clock_minutes % 10};
    lv_coord_t height = clock_height();
    lv_coord_t y = area.y1 + (lv_area_get_height(&area) - height) / 2;
    lv_coord_t gap = MAX(4, height / 9);

    if (style == CLOCK_STYLE_DOT_MATRIX) {
        lv_coord_t dot = MAX(1, (height - 6) / 15);
        lv_coord_t digit_width = dot * 10 + 4;
        lv_coord_t colon_width = dot * 4;
        lv_coord_t total_width = digit_width * 4 + gap * 4 + colon_width;
        lv_coord_t x = clock_aligned_x(&area, total_width);
        for (uint8_t index = 0; index < 4; index++) {
            draw_dot_matrix_digit(draw_ctx, digits[index], x, y, dot, color);
            x += digit_width + gap;
            if (index == 1) {
                if (clock_colon_visible) {
                    draw_clock_dot(draw_ctx, x + colon_width / 2, y + height / 3, dot, color, LV_OPA_COVER);
                    draw_clock_dot(draw_ctx, x + colon_width / 2, y + height * 2 / 3, dot, color, LV_OPA_COVER);
                }
                x += colon_width + gap;
            }
        }
        return;
    }

    lv_coord_t digit_width = height * 5 / 9;
    lv_coord_t digit_advance = digit_width;
    if (style == CLOCK_STYLE_SEGMENT_ITALIC) digit_advance += segment_skew(true, height, 0);
    if (style == CLOCK_STYLE_NEON) gap += MAX(4, height / 10);
    lv_coord_t colon_width = MAX(5, height / 5);
    lv_coord_t colon_advance = colon_width;
    if (style == CLOCK_STYLE_SEGMENT_ITALIC) colon_advance += segment_skew(true, height, 0);
    lv_coord_t total_width = digit_advance * 4 + gap * 4 + colon_advance;
    lv_coord_t x = clock_aligned_x(&area, total_width);
    const lv_font_t *font = clock_font();
    for (uint8_t index = 0; index < 4; index++) {
        if (style == CLOCK_STYLE_FLIP) {
            lv_area_t card = {.x1 = x, .y1 = y, .x2 = x + digit_width, .y2 = y + height};
            if ((flip_changed_mask & (1U << index)) && flip_animation_progress < 100) {
                draw_animated_flip_digit(draw_ctx, flip_previous_digits[index], digits[index], &card, font, color, background);
            } else {
                draw_flip_digit(draw_ctx, digits[index], &card, font, color, background);
            }
        } else if (style == CLOCK_STYLE_TUBE) {
            lv_area_t tube = {.x1 = x, .y1 = y, .x2 = x + digit_width, .y2 = y + height};
            draw_tube_digit(draw_ctx, digits[index], &tube, font, color, background);
        } else if (style == CLOCK_STYLE_NEON) {
            draw_segment_digit(draw_ctx, digits[index], x, y, digit_width, height, MAX(5, height / 7), false, true, color, LV_OPA_20, false);
            draw_segment_digit(draw_ctx, digits[index], x, y, digit_width, height, MAX(2, height / 18), false, true, color, LV_OPA_COVER, false);
        } else {
            draw_hex_segment_digit(draw_ctx, digits[index], x, y, digit_width, height, style == CLOCK_STYLE_SEGMENT_ITALIC, color);
        }
        x += digit_advance + gap;
        if (index == 1) {
            if (clock_colon_visible) {
                lv_coord_t radius = MAX(1, height / 18);
                bool italic = style == CLOCK_STYLE_SEGMENT_ITALIC;
                lv_coord_t upper_y = height / 3;
                lv_coord_t lower_y = height * 2 / 3;
                draw_clock_dot(draw_ctx, x + colon_width / 2 + segment_skew(italic, height, upper_y), y + upper_y, radius, color,
                               LV_OPA_COVER);
                draw_clock_dot(draw_ctx, x + colon_width / 2 + segment_skew(italic, height, lower_y), y + lower_y, radius, color,
                               LV_OPA_COVER);
            }
            x += colon_advance + gap;
        }
    }
}

#ifdef EH_DATE_SETTINGS_ENABLE
static const lv_font_t *standby_text_font(void) {
    static const lv_font_t *const fonts[10] = {
        &eh_font_date_montserrat_20,
        &eh_font_date_ubuntu_sans_20,
        &eh_font_date_ubuntu_mono_20,
        &eh_font_date_liberation_mono_20,
        &eh_font_date_dejavu_sans_20,
        &eh_font_date_dejavu_serif_20,
        &eh_font_date_dejavu_mono_20,
        &eh_font_date_liberation_sans_20,
        &eh_font_date_liberation_serif_20,
        &eh_font_date_liberation_narrow_20,
    };
    return fonts[MIN(get_clock_style(),9)];
}
#endif

#ifdef EH_DATE_SETTINGS_ENABLE
static void update_date(void) {
    if (!label_date) return;
    hid_data_t *hid = get_hid_data();
    toggle_hidden(label_date, eh_date_get(0) && hid->date_valid && is_hid_time_active());
    static uint8_t previous[10];
    static uint16_t previous_year;
    static uint8_t previous_month,previous_day;
    uint8_t current[10]; for(unsigned i=0;i<10;i++) current[i]=eh_date_get(i);
    // Date follows the clock family; its text size matches the layer line.
    current[5] = get_clock_style(); current[6] = 20;
    if(!memcmp(previous,current,10) && previous_year==hid->year && previous_month==hid->month && previous_day==hid->day) return;
    memcpy(previous,current,10); previous_year=hid->year; previous_month=hid->month; previous_day=hid->day;
    uint8_t format = eh_date_get(8);
    char text[16];
    if (format == 1) snprintf(text,sizeof(text),"%02u.%02u.%04u",hid->month,hid->day,hid->year);
    else if (format == 2) snprintf(text,sizeof(text),"%04u.%02u.%02u",hid->year,hid->month,hid->day);
    else snprintf(text,sizeof(text),"%02u.%02u.%04u",hid->day,hid->month,hid->year);
    if(strcmp(lv_label_get_text(label_date),text)) lv_label_set_text(label_date,text);
    const lv_font_t *date_font = standby_text_font();
    lv_obj_set_style_text_font(label_date,date_font,0);
    lv_obj_set_style_pad_top(label_date,MAX(0,(24-date_font->line_height)/2),0);
    lv_obj_set_style_text_color(label_date,lv_color_make(get_clock_text_red(),get_clock_text_green(),get_clock_text_blue()),0);
    lv_obj_set_style_text_opa(label_date,LV_OPA_COVER,0);
    lv_obj_set_style_text_align(label_date,LV_TEXT_ALIGN_CENTER,0);
    position_clock_labels();

}
#endif

void screen_home_apply_clock_settings(void) {
    if (screen_home == NULL) return;
#ifdef EH_DATE_SETTINGS_ENABLE
    update_date();
    lv_obj_set_style_text_color(label_date,lv_color_make(get_clock_text_red(),get_clock_text_green(),get_clock_text_blue()),0);
#endif
    lv_color_t text_color = lv_color_make(get_clock_text_red(), get_clock_text_green(), get_clock_text_blue());
    lv_color_t background = lv_color_make(get_clock_background_red(), get_clock_background_green(), get_clock_background_blue());
    lv_color_t info_color = text_color;

    lv_obj_set_style_bg_color(screen_home, background, 0);
#ifdef EH_STANDBY_BACKGROUND_ENABLE
    lv_obj_set_style_bg_color(standby_background_dim, background, 0);
    lv_obj_set_style_bg_opa(standby_background_dim, (lv_opa_t)((uint16_t)get_clock_background_dim() * LV_OPA_COVER / 100), 0);
    toggle_hidden(standby_background_dim, eh_background_is_valid() && get_clock_background_dim() > 0);
#endif
    lv_obj_t *time_labels[] = {label_time, label_time_colon, label_time_minutes};
    for (uint8_t index = 0; index < ARRAY_SIZE(time_labels); index++) {
        lv_obj_set_style_text_color(time_labels[index], text_color, 0);
        lv_obj_set_style_text_font(time_labels[index], clock_font(), LV_PART_MAIN);
        lv_obj_set_style_opa(time_labels[index], LV_OPA_COVER, 0);
    }
    lv_obj_set_style_opa(clock_custom, LV_OPA_COVER, 0);
    lv_obj_t *info_labels[] = {label_layer_icon, label_layer, label_mac, label_layout};
    for (uint8_t index = 0; index < ARRAY_SIZE(info_labels); index++) {
        lv_obj_set_style_text_color(info_labels[index], info_color, 0);
        lv_obj_set_style_text_opa(info_labels[index], LV_OPA_COVER, 0);
    }
    lv_obj_set_style_text_color(label_product, info_color, 0);
    lv_obj_set_style_text_color(label_hid_media_title, info_color, 0);
    lv_obj_set_style_text_color(label_hid_media_artist, info_color, 0);
#ifdef EH_DATE_SETTINGS_ENABLE
    lv_obj_set_style_text_font(label_layer,standby_text_font(),0);
    lv_obj_set_style_text_font(label_layout,standby_text_font(),0);
    lv_obj_set_style_pad_top(label_layer,MAX(0,(28-standby_text_font()->line_height)/2),0);
    lv_obj_set_style_pad_top(label_layout,MAX(0,(28-standby_text_font()->line_height)/2),0);
    lv_obj_set_style_pad_top(label_layer_icon,MAX(0,(28-eh_font_montserrat_20.line_height)/2),0);
    lv_obj_set_style_pad_top(label_mac,MAX(0,(28-eh_font_montserrat_20.line_height)/2),0);
    lv_obj_set_style_base_dir(label_layer,LV_BASE_DIR_LTR,0);
    lv_obj_set_style_anim_speed(label_layer,25,0);
    lv_color_t media_color=text_color;
    lv_obj_t *media_labels[]={label_hid_media_title,label_hid_media_artist};
    for(unsigned i=0;i<2;i++) {
        lv_obj_set_style_text_font(media_labels[i],standby_text_font(),0);
        lv_obj_set_style_text_color(media_labels[i],media_color,0);
        lv_obj_set_style_pad_top(media_labels[i],MAX(0,((i?30:36)-standby_text_font()->line_height)/2),0);
        lv_obj_set_style_base_dir(media_labels[i],LV_BASE_DIR_LTR,0);
        lv_obj_set_style_anim_speed(media_labels[i],25,0);
    }
    toggle_hidden(screen_home_media,is_hid_active() && eh_date_get(11));
#endif
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(label_time_colon, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(label_time_minutes, LV_TEXT_ALIGN_LEFT, 0);
    position_clock_labels();
    clock_colon_visible = true;
    update_clock_label();
    toggle_hidden(label_product, false);
    apply_clock_element_visibility();
    lv_obj_invalidate(clock_custom);
}

void screen_home_init(void) {
    screen_home = lv_obj_create(NULL);
    lv_obj_add_style(screen_home, &style_screen, 0);

#ifdef EH_STANDBY_BACKGROUND_ENABLE
    eh_background_init();
    standby_background = lv_img_create(screen_home);
    lv_obj_set_pos(standby_background, 0, 0);
    lv_obj_add_flag(standby_background, LV_OBJ_FLAG_HIDDEN);
    standby_background_dim = lv_obj_create(screen_home);
    lv_obj_set_pos(standby_background_dim, 0, 0);
    lv_obj_set_size(standby_background_dim, EH_BACKGROUND_WIDTH, EH_BACKGROUND_HEIGHT);
    lv_obj_set_style_bg_color(standby_background_dim, lv_color_black(), 0);
    lv_obj_set_style_border_width(standby_background_dim, 0, 0);
    lv_obj_set_style_pad_all(standby_background_dim, 0, 0);
    lv_obj_clear_flag(standby_background_dim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(standby_background_dim, LV_OBJ_FLAG_HIDDEN);
    refresh_standby_background();
#endif

#ifdef EH_DATE_SETTINGS_ENABLE
    lv_obj_t *content = eh_display_safe_content(screen_home);
#else
    lv_obj_t *content = screen_home;
#endif
    label_product = lv_label_create(content);
    lv_obj_set_style_text_font(label_product, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_product, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label_product, 0, 40);
    lv_obj_set_size(label_product, 230, 50);
    lv_label_set_text(label_product, EH_SHORT_PRODUCT_NAME);

    label_time = lv_label_create(content);
    lv_obj_set_style_text_font(label_time, clock_font(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label_time, "00");
    lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);

    label_time_colon = lv_label_create(content);
    lv_obj_set_style_text_font(label_time_colon, clock_font(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time_colon, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label_time_colon, ":");
    lv_obj_add_flag(label_time_colon, LV_OBJ_FLAG_HIDDEN);

    label_time_minutes = lv_label_create(content);
    lv_obj_set_style_text_font(label_time_minutes, clock_font(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time_minutes, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label_time_minutes, "00");
    lv_obj_add_flag(label_time_minutes, LV_OBJ_FLAG_HIDDEN);

#ifdef EH_DATE_SETTINGS_ENABLE
    label_date = lv_label_create(content);
    lv_obj_set_pos(label_date,10,115);
    lv_obj_set_size(label_date,210,24);
    lv_label_set_long_mode(label_date,LV_LABEL_LONG_CLIP);
    lv_label_set_text(label_date,"");
    lv_obj_add_flag(label_date,LV_OBJ_FLAG_HIDDEN);
#endif
    clock_custom = lv_obj_create(content);
    lv_obj_set_pos(clock_custom, 0, 39);
    lv_obj_set_size(clock_custom, 230, 76);
    lv_obj_set_style_bg_opa(clock_custom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_custom, 0, 0);
    lv_obj_set_style_pad_all(clock_custom, 0, 0);
    lv_obj_clear_flag(clock_custom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(clock_custom, draw_clock_event, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(clock_custom, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(update_clock_colon, 500, NULL);

    standby_header = lv_obj_create(content);
    lv_obj_add_style(standby_header, &style_container, 0);
    lv_obj_set_pos(standby_header, 26, 11);
    lv_obj_set_size(standby_header, 178, 28);
    lv_obj_set_style_pad_all(standby_header, 0, 0);
    lv_obj_set_style_border_width(standby_header, 0, 0);
    lv_obj_set_style_radius(standby_header, 0, 0);
    lv_obj_set_style_bg_opa(standby_header, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(standby_header, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(standby_header, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    label_layer_icon = lv_label_create(standby_header);
    lv_label_set_text(label_layer_icon, EH_SYMBOL_LAYER);
    lv_obj_set_pos(label_layer_icon, 0, 0);
    lv_obj_set_size(label_layer_icon, 20, 28);
    lv_obj_set_style_text_align(label_layer_icon, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layer_icon, &eh_font_montserrat_20, LV_PART_MAIN);

    label_layer = lv_label_create(standby_header);
    lv_label_set_text(label_layer, "");
    lv_obj_set_pos(label_layer, 26, 0);
    lv_obj_set_size(label_layer, 110, 28);
    lv_label_set_long_mode(label_layer, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(label_layer, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layer, &eh_font_montserrat_20, LV_PART_MAIN);

    label_mac = lv_label_create(standby_header);
    lv_label_set_text(label_mac, EH_SYMBOL_MAC);
    lv_obj_set_pos(label_mac, 112, 0);
    lv_obj_set_size(label_mac, 24, 28);
    lv_obj_set_style_text_align(label_mac, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_mac, &eh_font_montserrat_20, LV_PART_MAIN);
    toggle_hidden(label_mac, false);

    label_layout = lv_label_create(standby_header);
    lv_label_set_text(label_layout, "");
    lv_obj_set_pos(label_layout, 142, 0);
    lv_obj_set_size(label_layout, 36, 28);
    lv_obj_set_style_text_align(label_layout, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(label_layout, &eh_font_montserrat_20, LV_PART_MAIN);

#ifndef EH_HOME_HIDE_MODIFIERS
    screen_home_mods = lv_obj_create(content);
    lv_obj_add_style(screen_home_mods, &style_container, 0);
    use_flex_row(screen_home_mods);
    lv_obj_set_pos(screen_home_mods, 0, 130);
    lv_obj_set_size(screen_home_mods, 230, 100);

    label_gui   = create_button(screen_home_mods, "GUI", &style_button, &style_button_active);
    label_alt   = create_button(screen_home_mods, "ALT", &style_button, &style_button_active);
    label_ctrl  = create_button(screen_home_mods, "CTL", &style_button, &style_button_active);
    label_shift = create_button(screen_home_mods, "SFT", &style_button, &style_button_active);

    label_num    = create_button(screen_home_mods, "NUM", &style_button, &style_button_active);
    label_caps   = create_button(screen_home_mods, "CAPS", &style_button, &style_button_active);
    label_scroll = create_button(screen_home_mods, "SCRL", &style_button, &style_button_active);

#endif
    screen_home_media = lv_obj_create(content);
    lv_obj_add_style(screen_home_media, &style_container, 0);
    toggle_hidden(screen_home_media, false);
    lv_obj_set_style_pad_all(screen_home_media, 0, 0);
    lv_obj_set_pos(screen_home_media, 0, 130);
    lv_obj_set_size(screen_home_media, 230, 90);

    label_hid_media_title = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_title, "");
    lv_label_set_long_mode(label_hid_media_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(label_hid_media_title,10,7);
    lv_obj_set_size(label_hid_media_title,210,36);
    lv_obj_set_style_text_align(label_hid_media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_title, &eh_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label_hid_media_title, MAX(0,(36-eh_font_montserrat_28.line_height)/2), 0);
    lv_obj_set_style_pad_bottom(label_hid_media_title, 0, 0);

    label_hid_media_artist = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_artist, "");
    lv_label_set_long_mode(label_hid_media_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(label_hid_media_artist,10,48);
    lv_obj_set_size(label_hid_media_artist,210,30);
    lv_obj_set_style_text_align(label_hid_media_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_artist, &eh_font_montserrat_20, LV_PART_MAIN);

    lv_obj_set_style_pad_top(label_hid_media_artist, MAX(0,(30-eh_font_montserrat_20.line_height)/2), 0);
    lv_obj_set_style_text_color(label_hid_media_artist, accent_color_blue, 0);

    screen_home_apply_clock_settings();
}

bool screen_home_is_active(void) { return screen_home && lv_scr_act()==screen_home; }

void screen_home_load(void) {
    lv_label_set_text(label_layer, layer_name(get_current_layer()));
    lv_scr_load(screen_home);
    display_apply_brightness();
}

#ifndef EH_HOME_HIDE_MODIFIERS
static void screen_home_update_modifiers(void) {
    static led_t previous_leds;
    static uint8_t previous_mods = 255;
    static bool previous_mac;
    static uint32_t mods_timer;
    led_t leds = host_keyboard_led_state();
    leds.caps_lock |= split_get_caps_word();
    uint8_t mods = get_mods() | get_oneshot_mods();
    if (mods || previous_leds.raw != leds.raw) mods_timer = timer_read32();
    bool show = !is_hid_active() || timer_elapsed32(mods_timer) < EH_DISPLAY_TIMEOUT_ACTIVITY;
    toggle_hidden(screen_home_mods, show);
    toggle_hidden(screen_home_media, !show);
    bool mac = split_get_mac();
    if (mac != previous_mac) {
        lv_label_set_text(label_gui, mac ? "CMD" : "GUI");
        lv_label_set_text(label_alt, mac ? "OPT" : "ALT");
        previous_mac = mac;
    }
    if (leds.raw != previous_leds.raw) {
        toggle_state(label_caps, LV_STATE_PRESSED, leds.caps_lock);
        toggle_state(label_num, LV_STATE_PRESSED, leds.num_lock);
        toggle_state(label_scroll, LV_STATE_PRESSED, leds.scroll_lock);
        previous_leds = leds;
    }
    if (mods != previous_mods) {
        toggle_state(label_shift, LV_STATE_PRESSED, mods & MOD_MASK_SHIFT);
        toggle_state(label_ctrl, LV_STATE_PRESSED, mods & MOD_MASK_CTRL);
        toggle_state(label_alt, LV_STATE_PRESSED, mods & MOD_MASK_ALT);
        toggle_state(label_gui, LV_STATE_PRESSED, mods & MOD_MASK_GUI);
        previous_mods = mods;
    }
}
#endif

void screen_home_housekeep(void) {
#ifndef EH_HOME_HIDE_MODIFIERS
    screen_home_update_modifiers();
#endif
    static uint8_t prev_layer      = 255;
    static uint8_t prev_lang       = -1;
    static bool    prev_hid_active = false;
    static bool    prev_mac        = false;

#ifdef EH_STANDBY_BACKGROUND_ENABLE
    refresh_standby_background();
    animate_standby_background();
#endif

    uint8_t cur_layer = get_current_layer();
    uint8_t cur_lang  = split_get_lang();
    bool hid_active = is_hid_active();
    bool mac = split_get_mac();
#ifdef EH_DATE_SETTINGS_ENABLE
    update_date();
    apply_clock_element_visibility();
#endif
    uint32_t activity_elapsed = last_input_activity_elapsed();
    if (activity_elapsed > __UINT32_MAX__ - 1000) // possible overflow on split
        activity_elapsed = 0;
    bool typing = activity_elapsed < 500; // prevent display updates when typing

    if (prev_hid_active != hid_active) {
        if (typing) return;
        if (!hid_active) {
            clock_hours            = 0;
            clock_minutes          = 0;
            clock_time_initialized = false;
            update_clock_label();
            lv_obj_invalidate(clock_custom);
        }
        toggle_hidden(label_product, false);
        apply_clock_element_visibility();
#ifdef EH_DATE_SETTINGS_ENABLE
        toggle_hidden(screen_home_media, hid_active && eh_date_get(11));
#elif defined(EH_HOME_HIDE_MODIFIERS)
        toggle_hidden(screen_home_media,hid_active);
#endif
        prev_hid_active = hid_active;
        return;
    }

    if (prev_layer != cur_layer || layer_name_updated) {
        lv_label_set_text(label_layer, layer_name(cur_layer));
        position_clock_labels();
        prev_layer         = cur_layer;
        layer_name_updated = false;
        return;
    }

    if (prev_lang != cur_lang) {
        lv_label_set_text(label_layout, cur_lang == LANG_RU ? "RU" : "EN");
        position_clock_labels();
        prev_lang = cur_lang;
        return;
    }

    if (mac != prev_mac) {
        toggle_hidden(label_mac, get_clock_info_visible() && mac);
        position_clock_labels();
        prev_mac = mac;
        return;
    }

    hid_data_t *hid = get_hid_data();
    if (hid->hid_changed) {
        if (typing) return;
        if (hid->time_changed) {
            uint8_t previous_hours   = clock_hours;
            uint8_t previous_minutes = clock_minutes;
            clock_hours   = hid->hours;
            clock_minutes = hid->minutes;
            update_clock_label();
            if (clock_time_initialized && get_clock_style() == CLOCK_STYLE_FLIP) {
                start_flip_animation(previous_hours, previous_minutes, hid->hours, hid->minutes);
            }
            clock_time_initialized = true;
            lv_obj_invalidate(clock_custom);
            hid->time_changed = false;
            return;
        }
        if (hid->media_title_changed) {
            if(strcmp(lv_label_get_text(label_hid_media_title),hid->media_title)) lv_label_set_text(label_hid_media_title, hid->media_title);
            hid->media_title_changed = false;
            return;
        }
        if (hid->media_artist_changed) {
            if(strcmp(lv_label_get_text(label_hid_media_artist),hid->media_artist)) lv_label_set_text(label_hid_media_artist, hid->media_artist);
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
