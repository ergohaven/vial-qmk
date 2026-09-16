#include "src/display/eh_display.h"
#include "src/display/eh_display_bounds.h"
#include "src/display/eh_keycode_str.h"
#include "src/display/eh_pictograms.h"
#include "pictogram_render.h"
#include "src/display/eh_symbols.h"
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
    while (keycode == KC_TRANSPARENT && layer > 0) keycode = dynamic_keymap_get_keycode(--layer, row, col);
    return keycode;
}

uint16_t get_encoder_keycode(int layer, int encoder, bool clockwise) {
    uint16_t keycode = dynamic_keymap_get_encoder(layer, encoder, clockwise);
    while (keycode == KC_TRANSPARENT && layer > 0) keycode = dynamic_keymap_get_encoder(--layer, encoder, clockwise);
    return keycode;
}

/* Screen layout */

static lv_obj_t *screen_layout;

#define NLABELS 15
static lv_obj_t *key_labels[NLABELS];
static lv_obj_t *key_cells[NLABELS];
static lv_obj_t *key_icons[NLABELS];
static lv_img_dsc_t key_icon_dsc[NLABELS];
static lv_color_t key_icon_colors[NLABELS];
static uint8_t key_icon_bits[NLABELS][EH_ICON_RENDER_BYTES];
static uint16_t  label_kc[NLABELS];
static char      label_text[NLABELS][24];
static lv_obj_t *label_layer_icon;
static lv_obj_t *label_layer;

#define LAYER_HEADER_WIDTH 194
#define LAYER_HEADER_HEIGHT 70
#define LAYER_HEADER_LABEL_Y 20
#define LAYER_HEADER_LABEL_HEIGHT 38
#define LAYER_HEADER_GAP 6

#define KEY_PRESS_ANIMATION_MS 120

enum {
    BUTTON_STYLE_ROUNDED,
    BUTTON_STYLE_CIRCLE,
    BUTTON_STYLE_OVAL,
    BUTTON_STYLE_DIAMOND,
    BUTTON_STYLE_CHAMFERED,
    BUTTON_STYLE_HEXAGON,
    BUTTON_STYLE_SQUIRCLE,
    BUTTON_STYLE_TRAPEZOID_VERTICAL_UP,
    BUTTON_STYLE_TRAPEZOID_VERTICAL_DOWN,
    BUTTON_STYLE_TRAPEZOID_UP,
    BUTTON_STYLE_TRAPEZOID_DOWN,
    BUTTON_STYLE_WAVE_IN_PHASE,
    BUTTON_STYLE_WAVE_ANTIPHASE,
    BUTTON_STYLE_TRAPEZOID_HORIZONTAL_UP,
    BUTTON_STYLE_TRAPEZOID_HORIZONTAL_DOWN,
    BUTTON_STYLE_WAVE_HORIZONTAL_IN_PHASE,
    BUTTON_STYLE_WAVE_HORIZONTAL_ANTIPHASE,
    BUTTON_STYLE_WAVE_ALL_IN_PHASE,
    BUTTON_STYLE_WAVE_ALL_ANTIPHASE,
    BUTTON_STYLE_SEMICIRCLE_VERTICAL_UP,
    BUTTON_STYLE_SEMICIRCLE_VERTICAL_DOWN,
    BUTTON_STYLE_SEMICIRCLE_HORIZONTAL_UP,
    BUTTON_STYLE_SEMICIRCLE_HORIZONTAL_DOWN,
    BUTTON_STYLE_SEMICIRCLE_UP,
    BUTTON_STYLE_SEMICIRCLE_DOWN,
    BUTTON_STYLE_OVAL_SIDE_VERTICAL_UP,
    BUTTON_STYLE_OVAL_SIDE_VERTICAL_DOWN,
    BUTTON_STYLE_OVAL_SIDE_HORIZONTAL_UP,
    BUTTON_STYLE_OVAL_SIDE_HORIZONTAL_DOWN,
    BUTTON_STYLE_OVAL_SIDE_UP,
    BUTTON_STYLE_OVAL_SIDE_DOWN,
    BUTTON_STYLE_DIAGONAL_CHAMFER_TL_BR,
    BUTTON_STYLE_DIAGONAL_CHAMFER_TR_BL,
    BUTTON_STYLE_COUNT,
};

static bool     key_pressed[NLABELS];
static bool     key_release_pending[NLABELS];
static uint32_t key_press_timer[NLABELS];
static uint8_t  button_style = BUTTON_STYLE_ROUNDED;

static int8_t key_index_for_cell(lv_obj_t *cell) {
    for (uint8_t index = 0; index < NLABELS; index++) {
        if (key_cells[index] == cell) return index;
    }
    return -1;
}

static lv_area_t centered_shape_area(const lv_area_t *cell, lv_coord_t width, lv_coord_t height) {
    lv_coord_t center_x = (cell->x1 + cell->x2) / 2;
    lv_coord_t center_y = (cell->y1 + cell->y2) / 2;
    lv_area_t  area     = {
         .x1 = center_x - width / 2,
         .y1 = center_y - height / 2,
         .x2 = center_x - width / 2 + width - 1,
         .y2 = center_y - height / 2 + height - 1,
    };
    return area;
}

static void draw_rect_key_shape(lv_draw_ctx_t *draw_ctx, const lv_area_t *area, int8_t index, lv_coord_t radius) {
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color     = key_pressed[index] ? accent_color_blue : display_background_color;
    rect.bg_opa       = LV_OPA_COVER;
    rect.radius       = radius;
    rect.border_color = accent_color_blue;
    rect.border_opa   = index < 12 ? LV_OPA_COVER : LV_OPA_TRANSP;
    rect.border_width = index < 12 ? 1 : 0;
    lv_draw_rect(draw_ctx, &rect, area);
}

static void draw_polygon_key_shape(lv_draw_ctx_t *draw_ctx, lv_point_t *points, uint8_t point_count, int8_t index) {
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = key_pressed[index] ? accent_color_blue : display_background_color;
    fill.bg_opa   = LV_OPA_COVER;
    lv_draw_polygon(draw_ctx, &fill, points, point_count);

    if (index >= 12) return;

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = accent_color_blue;
    line.opa   = LV_OPA_COVER;
    line.width = 1;
    for (uint8_t point = 0; point < point_count; point++) {
        lv_draw_line(draw_ctx, &line, &points[point], &points[(point + 1) % point_count]);
    }
}

static lv_coord_t wave_offset(uint8_t sample, bool phase) {
    static const uint8_t wave_offsets[2][9] = {
        {0, 1, 2, 1, 0, 1, 2, 1, 0},
        {2, 1, 0, 1, 2, 1, 0, 1, 2},
    };
    return wave_offsets[phase ? 1 : 0][sample];
}

static void draw_wavy_key_shape(lv_draw_ctx_t *draw_ctx, const lv_area_t *shape, int8_t index, bool vertical, bool horizontal, bool phase) {
    lv_point_t outline[32];
    uint8_t    point_count = 0;
    lv_coord_t width       = shape->x2 - shape->x1;
    lv_coord_t height      = shape->y2 - shape->y1;

    for (uint8_t sample = 0; sample <= 8; sample++) {
        lv_coord_t x = shape->x1 + width * sample / 8;
        if (vertical && sample == 0) x += wave_offset(0, phase);
        if (vertical && sample == 8) x -= wave_offset(8, phase);
        outline[point_count++] = (lv_point_t){.x = x, .y = shape->y1 + (horizontal ? wave_offset(sample, phase) : 0)};
    }
    for (uint8_t sample = 1; sample <= 8; sample++) {
        lv_coord_t y = shape->y1 + height * sample / 8;
        if (horizontal && sample == 8) y -= wave_offset(8, phase);
        outline[point_count++] = (lv_point_t){.x = shape->x2 - (vertical ? wave_offset(sample, phase) : 0), .y = y};
    }
    for (int8_t sample = 7; sample >= 0; sample--) {
        lv_coord_t x = shape->x1 + width * sample / 8;
        if (vertical && sample == 0) x += wave_offset(0, phase);
        outline[point_count++] = (lv_point_t){.x = x, .y = shape->y2 - (horizontal ? wave_offset(sample, phase) : 0)};
    }
    for (int8_t sample = 7; sample >= 1; sample--) {
        outline[point_count++] = (lv_point_t){
            .x = shape->x1 + (vertical ? wave_offset(sample, phase) : 0),
            .y = shape->y1 + height * sample / 8,
        };
    }

    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = key_pressed[index] ? accent_color_blue : display_background_color;
    fill.bg_opa   = LV_OPA_COVER;
    lv_point_t center = {.x = (shape->x1 + shape->x2) / 2, .y = (shape->y1 + shape->y2) / 2};
    for (uint8_t point = 0; point < point_count; point++) {
        lv_point_t triangle[3] = {center, outline[point], outline[(point + 1) % point_count]};
        lv_draw_polygon(draw_ctx, &fill, triangle, 3);
    }

    if (index >= 12) return;

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = accent_color_blue;
    line.opa   = LV_OPA_COVER;
    line.width = 1;
    for (uint8_t point = 0; point < point_count; point++) {
        lv_draw_line(draw_ctx, &line, &outline[point], &outline[(point + 1) % point_count]);
    }
}

static bool directional_pattern_points_up(uint8_t variant, uint8_t index) {
    uint8_t row = index / 3;
    uint8_t col = index % 3;
    switch (variant) {
        case 0:
            return (row & 1) == 0;
        case 1:
            return (row & 1) != 0;
        case 2:
            return (col & 1) == 0;
        case 3:
            return (col & 1) != 0;
        case 4:
            return true;
        default:
            return false;
    }
}

static void draw_semicircle_key_shape(lv_draw_ctx_t *draw_ctx, const lv_area_t *shape, int8_t index, bool points_up) {
    lv_coord_t radius   = MIN((shape->x2 - shape->x1) / 2, shape->y2 - shape->y1);
    lv_coord_t center_x = (shape->x1 + shape->x2) / 2;
    lv_coord_t baseline = points_up ? shape->y2 - ((shape->y2 - shape->y1) - radius) / 2
                                    : shape->y1 + ((shape->y2 - shape->y1) - radius) / 2;
    lv_area_t circle = {
        .x1 = center_x - radius,
        .y1 = baseline - radius,
        .x2 = center_x + radius,
        .y2 = baseline + radius,
    };

    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color     = key_pressed[index] ? accent_color_blue : display_background_color;
    fill.bg_opa       = LV_OPA_COVER;
    fill.radius       = LV_RADIUS_CIRCLE;
    fill.border_color = accent_color_blue;
    fill.border_opa   = index < 12 ? LV_OPA_COVER : LV_OPA_TRANSP;
    fill.border_width = index < 12 ? 1 : 0;

    const lv_area_t *original_clip = draw_ctx->clip_area;
    lv_area_t        semicircle_clip = *original_clip;
    if (points_up) {
        semicircle_clip.y2 = MIN(semicircle_clip.y2, baseline);
    } else {
        semicircle_clip.y1 = MAX(semicircle_clip.y1, baseline);
    }
    if (semicircle_clip.y1 <= semicircle_clip.y2) {
        draw_ctx->clip_area = &semicircle_clip;
        lv_draw_rect(draw_ctx, &fill, &circle);
        draw_ctx->clip_area = original_clip;
    }

    if (index >= 12) return;

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = accent_color_blue;
    line.opa   = LV_OPA_COVER;
    line.width = 1;
    lv_point_t diameter[2] = {
        {.x = circle.x1, .y = baseline},
        {.x = circle.x2, .y = baseline},
    };
    lv_draw_line(draw_ctx, &line, &diameter[0], &diameter[1]);
}

static void draw_oval_side_key_shape(lv_draw_ctx_t *draw_ctx, const lv_area_t *shape, int8_t index, bool oval_top) {
    lv_coord_t x1 = shape->x1;
    lv_coord_t y1 = shape->y1;
    lv_coord_t x2 = shape->x2;
    lv_coord_t y2 = shape->y2;
    lv_point_t points[12] = {
        {.x = x1, .y = y1 + 12}, {.x = x1 + 3, .y = y1 + 6}, {.x = x1 + 9, .y = y1 + 2},
        {.x = x1 + 18, .y = y1}, {.x = x2 - 18, .y = y1}, {.x = x2 - 9, .y = y1 + 2},
        {.x = x2 - 3, .y = y1 + 6}, {.x = x2, .y = y1 + 12}, {.x = x2, .y = y2 - 6},
        {.x = x2 - 6, .y = y2}, {.x = x1 + 6, .y = y2}, {.x = x1, .y = y2 - 6},
    };
    if (!oval_top) {
        for (uint8_t point = 0; point < 12; point++) {
            points[point].y = y1 + y2 - points[point].y;
        }
    }
    draw_polygon_key_shape(draw_ctx, points, 12, index);
}

static void draw_key_shape_event(lv_event_t *event) {
    lv_obj_t *cell  = lv_event_get_target(event);
    int8_t    index = key_index_for_cell(cell);
    if (index < 0 || (index >= 12 && !key_pressed[index])) return;

    lv_area_t coords;
    lv_obj_get_coords(cell, &coords);
    lv_area_t shape = centered_shape_area(&coords, 73, 42);
    if (button_style == BUTTON_STYLE_CIRCLE) {
        shape = centered_shape_area(&coords, 42, 42);
    }

    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(event);
    if (button_style == BUTTON_STYLE_ROUNDED) {
        draw_rect_key_shape(draw_ctx, &shape, index, 6);
        return;
    }
    if (button_style == BUTTON_STYLE_CIRCLE || button_style == BUTTON_STYLE_OVAL) {
        draw_rect_key_shape(draw_ctx, &shape, index, LV_RADIUS_CIRCLE);
        return;
    }
    if (button_style == BUTTON_STYLE_SQUIRCLE) {
        draw_rect_key_shape(draw_ctx, &shape, index, 14);
        return;
    }

    lv_coord_t x1 = shape.x1;
    lv_coord_t y1 = shape.y1;
    lv_coord_t x2 = shape.x2;
    lv_coord_t y2 = shape.y2;
    lv_coord_t cx = (x1 + x2) / 2;
    lv_coord_t cy = (y1 + y2) / 2;

    if (button_style == BUTTON_STYLE_DIAMOND) {
        lv_point_t points[4] = {{.x = cx, .y = y1}, {.x = x2, .y = cy}, {.x = cx, .y = y2}, {.x = x1, .y = cy}};
        draw_polygon_key_shape(draw_ctx, points, 4, index);
    } else if (button_style == BUTTON_STYLE_CHAMFERED) {
        const lv_coord_t cut = 7;
        lv_point_t points[8] = {
            {.x = x1 + cut, .y = y1}, {.x = x2 - cut, .y = y1}, {.x = x2, .y = y1 + cut}, {.x = x2, .y = y2 - cut},
            {.x = x2 - cut, .y = y2}, {.x = x1 + cut, .y = y2}, {.x = x1, .y = y2 - cut}, {.x = x1, .y = y1 + cut},
        };
        draw_polygon_key_shape(draw_ctx, points, 8, index);
    } else if (button_style == BUTTON_STYLE_HEXAGON) {
        const lv_coord_t inset = 10;
        lv_point_t points[6] = {
            {.x = x1 + inset, .y = y1}, {.x = x2 - inset, .y = y1}, {.x = x2, .y = cy},
            {.x = x2 - inset, .y = y2}, {.x = x1 + inset, .y = y2}, {.x = x1, .y = cy},
        };
        draw_polygon_key_shape(draw_ctx, points, 6, index);
    } else if ((button_style >= BUTTON_STYLE_TRAPEZOID_VERTICAL_UP && button_style <= BUTTON_STYLE_TRAPEZOID_DOWN) ||
               (button_style >= BUTTON_STYLE_TRAPEZOID_HORIZONTAL_UP && button_style <= BUTTON_STYLE_TRAPEZOID_HORIZONTAL_DOWN)) {
        const lv_coord_t inset = 8;
        uint8_t row = index / 3;
        uint8_t col = index % 3;
        bool narrow_top;
        if (button_style == BUTTON_STYLE_TRAPEZOID_VERTICAL_UP) {
            narrow_top = (row & 1) == 0;
        } else if (button_style == BUTTON_STYLE_TRAPEZOID_VERTICAL_DOWN) {
            narrow_top = (row & 1) != 0;
        } else if (button_style == BUTTON_STYLE_TRAPEZOID_HORIZONTAL_UP) {
            narrow_top = (col & 1) == 0;
        } else if (button_style == BUTTON_STYLE_TRAPEZOID_HORIZONTAL_DOWN) {
            narrow_top = (col & 1) != 0;
        } else {
            narrow_top = button_style == BUTTON_STYLE_TRAPEZOID_UP;
        }
        lv_point_t points[4] = {
            {.x = narrow_top ? x1 + inset : x1, .y = y1}, {.x = narrow_top ? x2 - inset : x2, .y = y1},
            {.x = narrow_top ? x2 : x2 - inset, .y = y2}, {.x = narrow_top ? x1 : x1 + inset, .y = y2},
        };
        draw_polygon_key_shape(draw_ctx, points, 4, index);
    } else if (button_style == BUTTON_STYLE_WAVE_IN_PHASE || button_style == BUTTON_STYLE_WAVE_ANTIPHASE ||
               button_style == BUTTON_STYLE_WAVE_HORIZONTAL_IN_PHASE || button_style == BUTTON_STYLE_WAVE_HORIZONTAL_ANTIPHASE ||
               button_style == BUTTON_STYLE_WAVE_ALL_IN_PHASE || button_style == BUTTON_STYLE_WAVE_ALL_ANTIPHASE) {
        uint8_t row = index / 3;
        uint8_t col = index % 3;
        bool antiphase = button_style == BUTTON_STYLE_WAVE_ANTIPHASE || button_style == BUTTON_STYLE_WAVE_HORIZONTAL_ANTIPHASE ||
                         button_style == BUTTON_STYLE_WAVE_ALL_ANTIPHASE;
        bool horizontal = button_style == BUTTON_STYLE_WAVE_HORIZONTAL_IN_PHASE || button_style == BUTTON_STYLE_WAVE_HORIZONTAL_ANTIPHASE ||
                          button_style == BUTTON_STYLE_WAVE_ALL_IN_PHASE || button_style == BUTTON_STYLE_WAVE_ALL_ANTIPHASE;
        bool vertical = button_style == BUTTON_STYLE_WAVE_IN_PHASE || button_style == BUTTON_STYLE_WAVE_ANTIPHASE ||
                        button_style == BUTTON_STYLE_WAVE_ALL_IN_PHASE || button_style == BUTTON_STYLE_WAVE_ALL_ANTIPHASE;
        draw_wavy_key_shape(draw_ctx, &shape, index, vertical, horizontal, antiphase && ((row + col) & 1));
    } else if (button_style >= BUTTON_STYLE_SEMICIRCLE_VERTICAL_UP && button_style <= BUTTON_STYLE_SEMICIRCLE_DOWN) {
        draw_semicircle_key_shape(draw_ctx, &shape, index,
                                  directional_pattern_points_up(button_style - BUTTON_STYLE_SEMICIRCLE_VERTICAL_UP, index));
    } else if (button_style >= BUTTON_STYLE_OVAL_SIDE_VERTICAL_UP && button_style <= BUTTON_STYLE_OVAL_SIDE_DOWN) {
        draw_oval_side_key_shape(draw_ctx, &shape, index,
                                 directional_pattern_points_up(button_style - BUTTON_STYLE_OVAL_SIDE_VERTICAL_UP, index));
    } else if (button_style == BUTTON_STYLE_DIAGONAL_CHAMFER_TL_BR) {
        const lv_coord_t cut = 9;
        lv_point_t points[6] = {
            {.x = x1 + cut, .y = y1}, {.x = x2, .y = y1}, {.x = x2, .y = y2 - cut},
            {.x = x2 - cut, .y = y2}, {.x = x1, .y = y2}, {.x = x1, .y = y1 + cut},
        };
        draw_polygon_key_shape(draw_ctx, points, 6, index);
    } else if (button_style == BUTTON_STYLE_DIAGONAL_CHAMFER_TR_BL) {
        const lv_coord_t cut = 9;
        lv_point_t points[6] = {
            {.x = x1, .y = y1}, {.x = x2 - cut, .y = y1}, {.x = x2, .y = y1 + cut},
            {.x = x2, .y = y2}, {.x = x1 + cut, .y = y2}, {.x = x1, .y = y2 - cut},
        };
        draw_polygon_key_shape(draw_ctx, points, 6, index);
    }
}

static int8_t label_index_for_matrix_pos(uint8_t row, uint8_t col) {
    if (row >= 1 && row <= 4 && col <= 2) {
        return (row - 1) * 3 + col;
    }
    if (row == 0 && col == 2) {
        return 13;
    }
    return -1;
}

static void set_key_pressed(uint8_t index, bool pressed) {
    if (index >= NLABELS || key_cells[index] == NULL || key_labels[index] == NULL || key_pressed[index] == pressed) {
        return;
    }

    key_pressed[index] = pressed;
    lv_obj_invalidate(key_cells[index]);
    lv_obj_set_style_text_color(key_labels[index], pressed ? display_background_color : accent_color_blue, 0);
    if (key_icons[index] != NULL) {
        lv_obj_set_style_img_recolor(key_icons[index], pressed ? display_background_color : key_icon_colors[index], 0);
    }
}

void screen_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
    int8_t index = label_index_for_matrix_pos(row, col);
    if (index < 0 || key_cells[index] == NULL) {
        return;
    }

    if (pressed) {
        key_release_pending[index] = false;
        key_press_timer[index]     = timer_read32();
        set_key_pressed(index, true);
    } else if (timer_elapsed32(key_press_timer[index]) >= KEY_PRESS_ANIMATION_MS) {
        key_release_pending[index] = false;
        set_key_pressed(index, false);
    } else {
        key_release_pending[index] = true;
    }
}

void screen_layout_process_encoder_event(uint8_t index, bool clockwise) {
    if (index != 0) return;

    uint8_t label_index              = clockwise ? 14 : 12;
    uint8_t opposite_label_index     = clockwise ? 12 : 14;
    key_release_pending[opposite_label_index] = false;
    set_key_pressed(opposite_label_index, false);
    key_release_pending[label_index] = true;
    key_press_timer[label_index]     = timer_read32();
    set_key_pressed(label_index, true);
}

void screen_layout_apply_accent_color(void) {
    if (label_layer_icon != NULL) lv_obj_set_style_text_color(label_layer_icon, accent_color_blue, 0);
    if (label_layer != NULL) lv_obj_set_style_text_color(label_layer, accent_color_blue, 0);
    for (uint8_t index = 0; index < NLABELS; index++) {
        if (key_cells[index] == NULL || key_labels[index] == NULL) continue;
        lv_obj_invalidate(key_cells[index]);
        lv_obj_set_style_text_color(key_labels[index], key_pressed[index] ? display_background_color : accent_color_blue, 0);
        if (key_icons[index] != NULL) {
            key_icon_colors[index] = accent_color_blue;
            lv_obj_set_style_img_recolor(key_icons[index], key_pressed[index] ? display_background_color : key_icon_colors[index], 0);
        }
    }
}

void screen_layout_apply_background_color(void) {
    for (uint8_t index = 0; index < NLABELS; index++) {
        if (key_cells[index] == NULL || key_labels[index] == NULL) continue;
        lv_obj_invalidate(key_cells[index]);
        lv_obj_set_style_text_color(key_labels[index], key_pressed[index] ? display_background_color : accent_color_blue, 0);
        if (key_icons[index] != NULL) {
            lv_obj_set_style_img_recolor(key_icons[index], key_pressed[index] ? display_background_color : key_icon_colors[index], 0);
        }
    }
}

void screen_layout_apply_button_style(uint8_t style) {
    if (style >= BUTTON_STYLE_COUNT) style = BUTTON_STYLE_ROUNDED;
    button_style = style;

    for (uint8_t index = 0; index < NLABELS; index++) {
        lv_obj_t *cell = key_cells[index];
        if (cell == NULL) continue;

        lv_obj_invalidate(cell);
    }
}

bool screen_layout_has_active_key_animation(void) {
    for (uint8_t index = 0; index < NLABELS; index++) {
        if (key_pressed[index] || key_release_pending[index]) {
            return true;
        }
    }
    return false;
}

static void finish_key_press_animations(void) {
    for (uint8_t index = 0; index < NLABELS; index++) {
        if (key_release_pending[index] && timer_elapsed32(key_press_timer[index]) >= KEY_PRESS_ANIMATION_MS) {
            key_release_pending[index] = false;
            set_key_pressed(index, false);
        }
    }
}

static void screen_layout_set_layer_name(const char *name) {
    lv_coord_t letter_space   = lv_obj_get_style_text_letter_space(label_layer, LV_PART_MAIN);
    lv_coord_t icon_width     = lv_txt_get_width(EH_SYMBOL_LAYER, strlen(EH_SYMBOL_LAYER), &eh_font_montserrat_28, letter_space, LV_TEXT_FLAG_NONE);
    lv_coord_t name_width     = lv_txt_get_width(name, strlen(name), &eh_font_montserrat_28, letter_space, LV_TEXT_FLAG_NONE);
    lv_coord_t max_name_width = LAYER_HEADER_WIDTH - icon_width - LAYER_HEADER_GAP;

    lv_label_set_text(label_layer, name);
    if (name_width <= max_name_width) {
        lv_coord_t group_width = icon_width + LAYER_HEADER_GAP + name_width;
        lv_coord_t group_x     = (LAYER_HEADER_WIDTH - group_width) / 2;
        lv_obj_set_pos(label_layer_icon, group_x, LAYER_HEADER_LABEL_Y);
        lv_obj_set_size(label_layer_icon, icon_width, LAYER_HEADER_LABEL_HEIGHT);
        lv_label_set_long_mode(label_layer, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(label_layer, group_x + icon_width + LAYER_HEADER_GAP, LAYER_HEADER_LABEL_Y);
        lv_obj_set_size(label_layer, MAX(name_width, 1), LAYER_HEADER_LABEL_HEIGHT);
    } else {
        lv_obj_set_pos(label_layer_icon, 0, LAYER_HEADER_LABEL_Y);
        lv_obj_set_size(label_layer_icon, icon_width, LAYER_HEADER_LABEL_HEIGHT);
        lv_label_set_long_mode(label_layer, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_pos(label_layer, icon_width + LAYER_HEADER_GAP, LAYER_HEADER_LABEL_Y);
        lv_obj_set_size(label_layer, max_name_width, LAYER_HEADER_LABEL_HEIGHT);
    }
}

void screen_layout_init(void) {
    eh_pictograms_init();
    screen_layout = lv_obj_create(NULL);
    lv_obj_add_style(screen_layout, &style_screen, 0);
    lv_obj_t *content = eh_display_safe_content(screen_layout);
    lv_obj_set_style_pad_row(content, lv_obj_get_style_pad_row(screen_layout, 0), 0);
    lv_obj_set_style_pad_column(content, lv_obj_get_style_pad_column(screen_layout, 0), 0);
    use_flex_column(content);
    lv_obj_set_scrollbar_mode(screen_layout, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *layer_header = lv_obj_create(content);
    lv_obj_add_style(layer_header, &style_container, 0);
    lv_obj_set_size(layer_header, LAYER_HEADER_WIDTH, LAYER_HEADER_HEIGHT);
    lv_obj_set_scrollbar_mode(layer_header, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(layer_header, LV_OBJ_FLAG_SCROLLABLE);

    label_layer_icon = lv_label_create(layer_header);
    lv_label_set_text(label_layer_icon, EH_SYMBOL_LAYER);
    lv_obj_set_style_text_align(label_layer_icon, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layer_icon, &eh_font_montserrat_28, LV_PART_MAIN);

    label_layer = lv_label_create(layer_header);
    lv_label_set_text(label_layer, "");
    lv_obj_set_style_text_align(label_layer, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layer, &eh_font_montserrat_28, LV_PART_MAIN);
    screen_layout_set_layer_name(layer_name(0));

    lv_obj_t *cont = lv_obj_create(content);
    lv_obj_set_size(cont, 222, 240);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    int32_t v = 0;
    lv_obj_set_style_pad_row(cont, v, 0);
    lv_obj_set_style_pad_column(cont, v, 0);
    lv_obj_add_style(cont, &style_container, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    for (uint32_t i = 0; i < NLABELS; i++) {
        if (i == 12) {
            lv_obj_t *obj = lv_obj_create(cont);
            lv_obj_set_size(obj, 222, 5);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
            lv_obj_add_style(obj, &style_screen, 0);
            lv_obj_set_style_border_opa(obj, 0, 0);
        }
        lv_obj_t *obj = lv_obj_create(cont);
        key_cells[i] = obj;
        lv_obj_set_size(obj, 74, 43);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(obj, &style_screen, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
        lv_obj_add_event_cb(obj, draw_key_shape_event, LV_EVENT_DRAW_MAIN, NULL);

        key_labels[i] = lv_label_create(obj);
        lv_obj_center(key_labels[i]);
        lv_obj_set_style_text_font(key_labels[i], &eh_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_align(key_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(key_labels[i], accent_color_blue, 0);
        lv_label_set_text_static(key_labels[i], "");

        key_icons[i] = lv_img_create(obj);
        lv_obj_center(key_icons[i]);
        lv_obj_set_style_img_recolor_opa(key_icons[i], LV_OPA_COVER, 0);
        key_icon_colors[i] = accent_color_blue;
        lv_obj_set_style_img_recolor(key_icons[i], key_icon_colors[i], 0);
        lv_obj_add_flag(key_icons[i], LV_OBJ_FLAG_HIDDEN);
        label_kc[i] = 0;
        key_pressed[i] = false;
        key_release_pending[i] = false;

        if (i >= 12) {
            lv_obj_set_style_border_opa(obj, 0, 0);
            lv_obj_set_style_text_color(key_labels[i], accent_color_blue, 0);
        }
    }
    screen_layout_apply_button_style(get_display_button_style());
    screen_layout_apply_accent_color();
}

static uint8_t prev_layer = 255;
static int     lbl_idx    = 0;
static uint32_t pictogram_generation;

static void screen_layout_set_key_content(uint8_t index, uint16_t keycode) {
    const uint8_t *bitmap = eh_pictogram_for_keycode(keycode);
    if (bitmap == NULL) {
        lv_obj_add_flag(key_icons[index], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(key_labels[index], LV_OBJ_FLAG_HIDDEN);
        get_keycode_str(label_text[index], keycode);
        lv_label_set_text_static(key_labels[index], label_text[index]);
        return;
    }

    eh_render_pictogram(bitmap, eh_pictogram_stored_width(), key_icon_bits[index]);
    key_icon_dsc[index] = (lv_img_dsc_t){
        .header.always_zero = 0,
        .header.w = EH_ICON_RENDER_SIZE,
        .header.h = EH_ICON_RENDER_SIZE,
        .header.cf = LV_IMG_CF_ALPHA_1BIT,
        .data_size = EH_ICON_RENDER_BYTES,
        .data = key_icon_bits[index],
    };
    key_icon_colors[index] = accent_color_blue;
    lv_img_cache_invalidate_src(&key_icon_dsc[index]);
    lv_img_set_src(key_icons[index], &key_icon_dsc[index]);
    lv_obj_invalidate(key_icons[index]);
    lv_obj_set_style_img_recolor(key_icons[index], key_pressed[index] ? display_background_color : key_icon_colors[index], 0);
    lv_obj_add_flag(key_labels[index], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(key_icons[index], LV_OBJ_FLAG_HIDDEN);
}

/* A hidden screen retains its rendered keycodes, so unchanged keycodes do
 * not imply unchanged icon pixels. Refresh before exposing it on wake. */
static void screen_layout_refresh_key_content(void) {
    uint8_t layer = get_current_layer();
    for (uint8_t index = 0; index < NLABELS; index++) {
        uint16_t keycode = index < 12 ? get_keycode(layer, 1 + index / 3, index % 3)
            : index == 13 ? get_keycode(layer, 0, 2) : get_encoder_keycode(layer, 0, index == 14);
        screen_layout_set_key_content(index, keycode);
        label_kc[index] = keycode;
    }
    // Only acknowledge a generation after its images have actually been drawn.
    pictogram_generation = eh_pictograms_generation();
    lbl_idx = 0;
}

void screen_layout_load(void) {
    prev_layer = get_current_layer();
    screen_layout_set_layer_name(layer_name(prev_layer));
    layer_name_updated = false;
    screen_layout_refresh_key_content();
    lv_scr_load(screen_layout);
    display_apply_brightness();
}

void screen_layout_housekeep(void) {
    static uint32_t update_timer = 0;
    finish_key_press_animations();
    if (pictogram_generation != eh_pictograms_generation()) {
        screen_layout_refresh_key_content();
    }
    if (timer_elapsed32(update_timer) < 5) // prevent long display updates
        return;

    uint8_t layer = get_current_layer();
    if (layer != prev_layer || layer_name_updated) {
        prev_layer = layer;
        screen_layout_set_layer_name(layer_name(layer));
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
        screen_layout_set_key_content(lbl_idx, keycode);
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
