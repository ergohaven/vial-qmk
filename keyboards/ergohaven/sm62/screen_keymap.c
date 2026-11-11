// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboards/ergohaven/sm62/render_alias.h"
#include <lvgl.h>
#include "application_display.h"
#include "application_geometry.h"
#include "src/display/application_icons.h"
#include "hid.h"
#include "sm62_effect_config.h"
#include "sm62_settings.h"
#include "timer.h"
#include "ergohaven.h"
#include "dynamic_keymap.h"
#include "src/display/eh_display.h"
#include "src/display/eh_keycode_str.h"
#include "src/display/eh_symbols.h"

LV_FONT_DECLARE(eh_font_iosevka_28);
LV_FONT_DECLARE(sm62_font_zen_medium_16);
LV_FONT_DECLARE(sm62_font_zen_medium_17);
LV_FONT_DECLARE(eh_font_iosevka_18);
LV_FONT_DECLARE(sm62_font_zen_medium_20);
LV_FONT_DECLARE(sm62_font_zen_medium_28);

#define KEYMAP_BG          0x070705
#define KEYMAP_KEY_BG      0x070705
#define KEYMAP_BORDER      KEYMAP_TEXT
#define KEYMAP_TEXT        0x837F76
#define KEYMAP_ACTIVE_BG   0xF2DB95
#define KEYMAP_ACTIVE_TEXT 0x070705
#define KEYMAP_RELEASE_BG  KEYMAP_ACTIVE_BG

#define KEYMAP_RELEASE_FADE_MS  96U
#if SM62_FAST_RETURN
#    define KEYMAP_POST_FADE_HOLD_MS 70U
#else
#    define KEYMAP_POST_FADE_HOLD_MS 0U
#endif
#define KEYMAP_FADE_FRAME_MS    12U
#define KEYMAP_GRID_LINE_SIZE    2
#define KEYMAP_GRID_NODE_SIZE    20
#define KEYMAP_GRID_FILLET_RADIUS 9
#define KEYMAP_ACTIVE_SIZE       69
#define KEYMAP_ACTIVE_RADIUS      7
#ifndef SM62_EFFECT
#    define SM62_EFFECT 0
#endif
#define SM62_EFFECT_SLOTS 6U
#define SM62_EFFECT_MS 144U

typedef enum {
    KEY_VISUAL_REST,
    KEY_VISUAL_PRESSED,
    KEY_VISUAL_RELEASED,
} key_visual_state_t;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *primary;
    lv_obj_t *secondary;
    lv_obj_t *shifted;
    char primary_text[64];
    lv_obj_t *icon;
    lv_img_dsc_t icon_dsc;
    uint8_t icon_bits[EH_ICON_RENDER_BYTES];
    bool icon_running;
} key_widgets_t;

static lv_obj_t *screen_keymap;
static key_widgets_t keys[9];
static lv_obj_t *vertical_grid_lines[12];
static lv_obj_t *horizontal_grid_lines[12];
static lv_obj_t *grid_junctions[4];
static bool old_pressed[5][7];
static bool released_fading[5][7];
static uint32_t released_at[5][7];
static uint8_t rendered_row[9];
static uint8_t rendered_col[9];
static uint8_t rendered_state[9];
static uint8_t rendered_mix[9];
static uint8_t first_row;
static uint8_t first_visual_col;
static uint8_t previous_layer = 0xFF;
static bool previous_side_left;
static bool force_refresh;
static uint8_t previous_cyrillic_enabled = 0xFF;
static uint32_t fade_frame_timer;
static bool pending_matrix_change;
static bool focus_next_press_after_layer_change;
static bool recent_press_valid;
static uint8_t recent_press_row, recent_press_col;
static uint8_t visual_col_from_matrix(bool left, uint8_t matrix_col);
static uint8_t visual_row_from_matrix(bool left, uint8_t matrix_row, uint8_t matrix_col);
#if SM62_EFFECT > 0
typedef struct {
    uint32_t started;
    uint8_t row;
    uint8_t col;
    uint8_t prior_row;
    uint8_t prior_col;
    bool has_prior;
    bool active;
} effect_event_t;
static effect_event_t effect_events[SM62_EFFECT_SLOTS];
static uint8_t effect_rendered[9];
static uint8_t grid_rendered[28];
static uint8_t last_press_row, last_press_col;
static bool has_last_press;
static uint32_t effect_frame_timer;

#if SM62_EFFECT != 8
static uint8_t effect_envelope(uint32_t age, uint16_t delay, uint16_t duration, uint8_t peak) {
    if (age < delay || age >= (uint32_t)delay + duration) return 0;
    uint16_t phase = age - delay;
    if (phase < 24U) return peak;
    return (uint8_t)(((uint32_t)(duration - phase) * peak) / (duration - 24U));
}

#endif

static void effect_start(uint8_t row, uint8_t col) {
    uint8_t slot = 0;
    uint32_t oldest = 0;
    for (uint8_t i = 0; i < SM62_EFFECT_SLOTS; ++i) {
        if (!effect_events[i].active) { slot = i; break; }
        uint32_t age = timer_elapsed32(effect_events[i].started);
        if (age >= oldest) { oldest = age; slot = i; }
    }
    effect_events[slot] = (effect_event_t){
        .started = timer_read32(), .row = row, .col = col,
        .prior_row = last_press_row, .prior_col = last_press_col,
        .has_prior = has_last_press, .active = true,
    };
    last_press_row = row;
    last_press_col = col;
    has_last_press = true;
    effect_frame_timer = effect_events[slot].started;
}

static bool effect_advance(void) {
    bool active = false;
    for (uint8_t i = 0; i < SM62_EFFECT_SLOTS; ++i) {
        if (!effect_events[i].active) continue;
        if (timer_elapsed32(effect_events[i].started) >= SM62_EFFECT_MS) effect_events[i].active = false;
        else active = true;
    }
    return active;
}

static uint8_t effect_at(uint8_t row, uint8_t col) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < SM62_EFFECT_SLOTS; ++i) {
        const effect_event_t *event = &effect_events[i];
        if (!event->active) continue;
        uint8_t dr = row > event->row ? row - event->row : event->row - row;
        uint8_t dc = col > event->col ? col - event->col : event->col - col;
        uint8_t distance = dr + dc;
        uint32_t age = timer_elapsed32(event->started);
        (void)age; (void)distance;
        uint8_t value = 0;
#if SM62_EFFECT == 1 /* Liquid: modest dip, then a short rebound. */
        if (distance == 1U) value = age < 44U ? effect_envelope(age, 0, 48, 32) : effect_envelope(age, 44, 76, 24);
#elif SM62_EFFECT == 2 || SM62_EFFECT == 9 /* Ripple and hybrid. */
        if (distance == 1U) value = effect_envelope(age, 16, 88, 54);
        if (distance == 2U) value = effect_envelope(age, 44, 84, 27);
#elif SM62_EFFECT == 6 /* Soft Bloom. */
        if (distance == 1U) value = effect_envelope(age, 8, 104, 30);
#elif SM62_EFFECT == 8 /* Elastic Tap: centre only, single soft rebound. */
        if (distance == 0U) value = age < 24U ? 0 : (age < 52U ? 72 : (age < 80U ? 30 : 0));
#endif
        if (value > result) result = value;
    }
    return result;
}

#if SM62_EFFECT == 3 || SM62_EFFECT == 5 || SM62_EFFECT == 7 || SM62_EFFECT == 9
static uint8_t grid_effect_level(bool left, uint8_t row, uint8_t boundary, bool vertical) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < SM62_EFFECT_SLOTS; ++i) {
        const effect_event_t *event = &effect_events[i];
        if (!event->active) continue;
        int8_t er = (int8_t)visual_row_from_matrix(left, event->row, event->col) - (int8_t)first_row;
        int8_t ec = (int8_t)visual_col_from_matrix(left, event->col) - (int8_t)first_visual_col;
        uint32_t age = timer_elapsed32(event->started);
        uint8_t value = 0;
#if SM62_EFFECT == 3 || SM62_EFFECT == 7 || SM62_EFFECT == 9
        bool adjacent = vertical ? ((int8_t)row == er && ((int8_t)boundary == ec || (int8_t)boundary == ec + 1)) :
                                   ((int8_t)boundary == er || (int8_t)boundary == er + 1) && (int8_t)row == ec;
        if (adjacent) value = effect_envelope(age, SM62_EFFECT == 3 ? 12 : 0, 92, SM62_EFFECT == 3 ? 185 : 130);
#if SM62_EFFECT == 3
        bool outer = vertical ? ((er == (int8_t)row - 1 || er == (int8_t)row + 1) &&
                                  ((int8_t)boundary == ec || (int8_t)boundary == ec + 1)) :
                                 ((ec == (int8_t)row - 1 || ec == (int8_t)row + 1) &&
                                  ((int8_t)boundary == er || (int8_t)boundary == er + 1));
        if (outer) {
            uint8_t far = effect_envelope(age, 40, 76, 90);
            if (far > value) value = far;
        }
#endif
#endif
#if SM62_EFFECT == 5 /* Directional Trail follows the previous key. */
        if (event->has_prior) {
            int8_t pr = (int8_t)visual_row_from_matrix(left, event->prior_row, event->prior_col) - (int8_t)first_row;
            int8_t pc = (int8_t)visual_col_from_matrix(left, event->prior_col) - (int8_t)first_visual_col;
            bool path = false;
            if (vertical && er == pr && (int8_t)row == er) {
                path = ((int8_t)boundary > (ec < pc ? ec : pc) && (int8_t)boundary <= (ec > pc ? ec : pc));
            } else if (!vertical && ec == pc && (int8_t)row == ec) {
                path = ((int8_t)boundary > (er < pr ? er : pr) && (int8_t)boundary <= (er > pr ? er : pr));
            } else if (vertical && (int8_t)row == pr && pc != ec) {
                path = ((int8_t)boundary > (ec < pc ? ec : pc) && (int8_t)boundary <= (ec > pc ? ec : pc));
            } else if (!vertical && (int8_t)row == ec && pr != er) {
                path = ((int8_t)boundary > (er < pr ? er : pr) && (int8_t)boundary <= (er > pr ? er : pr));
            }
            if (path) value = effect_envelope(age, 0, 110, 150);
        }
#endif
        if (value > result) result = value;
    }
    return result;
}

#endif

static void render_grid_effect(bool left) {
#if SM62_EFFECT == 3 || SM62_EFFECT == 5 || SM62_EFFECT == 7 || SM62_EFFECT == 9
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t boundary = 0; boundary < 4; ++boundary) {
            uint8_t index = row * 4U + boundary;
            uint8_t level = grid_effect_level(left, row, boundary, true);
            if (grid_rendered[index] != level) {
                grid_rendered[index] = level;
                lv_obj_set_style_bg_color(vertical_grid_lines[index], lv_color_mix(lv_color_hex(KEYMAP_ACTIVE_BG), lv_color_hex(KEYMAP_BORDER), level), 0);
            }
        }
    }
    for (uint8_t boundary = 0; boundary < 4; ++boundary) {
        for (uint8_t col = 0; col < 3; ++col) {
            uint8_t index = boundary * 3U + col;
            uint8_t level = grid_effect_level(left, col, boundary, false);
            if (grid_rendered[12U + index] != level) {
                grid_rendered[12U + index] = level;
                lv_obj_set_style_bg_color(horizontal_grid_lines[index], lv_color_mix(lv_color_hex(KEYMAP_ACTIVE_BG), lv_color_hex(KEYMAP_BORDER), level), 0);
            }
        }
    }
    for (uint8_t row = 1; row < 3; ++row) {
        for (uint8_t col = 1; col < 3; ++col) {
            uint8_t index = (row - 1U) * 2U + col - 1U;
            uint8_t value = 0;
            for (uint8_t i = 0; i < SM62_EFFECT_SLOTS; ++i) {
                const effect_event_t *event = &effect_events[i];
                if (!event->active) continue;
                int8_t er = (int8_t)visual_row_from_matrix(left, event->row, event->col) - (int8_t)first_row;
                int8_t ec = (int8_t)visual_col_from_matrix(left, event->col) - (int8_t)first_visual_col;
                if ((er == (int8_t)row || er == (int8_t)row - 1) &&
                    (ec == (int8_t)col || ec == (int8_t)col - 1)) {
                    uint8_t pulse = effect_envelope(timer_elapsed32(event->started), 12, 92, 90);
                    if (pulse > value) value = pulse;
                }
            }
            if (grid_rendered[24U + index] != value) {
                grid_rendered[24U + index] = value;
                lv_obj_set_style_img_recolor(grid_junctions[index], lv_color_mix(lv_color_hex(KEYMAP_ACTIVE_BG), lv_color_hex(KEYMAP_BORDER), value), 0);
            }
        }
    }
#else
    (void)left;
#endif
}
#endif


static lv_obj_t *make_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, const lv_font_t *font, lv_text_align_t align) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(KEYMAP_TEXT), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(label, "");
    return label;
}

static key_widgets_t make_key(lv_obj_t *parent, lv_coord_t x, lv_coord_t y) {
    key_widgets_t widget = {0};
    widget.box = lv_obj_create(parent);
    lv_obj_set_pos(widget.box, x, y);
    /* The reference leaves a small gap between the reactive fill and grid. */
    lv_obj_set_size(widget.box, KEYMAP_ACTIVE_SIZE, KEYMAP_ACTIVE_SIZE);
    lv_obj_set_style_radius(widget.box, KEYMAP_ACTIVE_RADIUS, 0);
    lv_obj_set_style_pad_all(widget.box, 0, 0);
    lv_obj_set_style_border_width(widget.box, 0, 0);
    lv_obj_set_style_bg_color(widget.box, lv_color_hex(KEYMAP_KEY_BG), 0);
    lv_obj_set_style_bg_opa(widget.box, LV_OPA_COVER, 0);
    lv_obj_clear_flag(widget.box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(widget.box, 0, 0);
    lv_obj_set_style_shadow_spread(widget.box, 0, 0);
    lv_obj_set_style_shadow_color(widget.box, lv_color_hex(KEYMAP_ACTIVE_BG), 0);
    lv_obj_set_style_shadow_opa(widget.box, LV_OPA_TRANSP, 0);
    widget.primary = make_label(widget.box, 9, 4, 56, 34, &sm62_font_zen_medium_28, LV_TEXT_ALIGN_LEFT);
    widget.secondary = make_label(widget.box, 38, 40, 29, 25, &sm62_font_zen_medium_20, LV_TEXT_ALIGN_LEFT);
    widget.shifted = make_label(widget.box, 39, 5, 25, 25, &sm62_font_zen_medium_20, LV_TEXT_ALIGN_RIGHT);
    return widget;
}

static lv_obj_t *make_grid_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, width, height);
    lv_obj_set_style_radius(line, 1, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(KEYMAP_BORDER), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

/*
 * A binary A4 mask avoids the isolated dim pixels produced by v14's
 * anti-aliased 0x5 nibbles. The 9 px grid fillet is concentric with the
 * active key's 7 px corner, preserving the same 2 px clearance around the
 * entire curve as along the flat edges.
 */
/*
 * A8 coverage is intentional here. The binary A4 mask used by v15 turned
 * the 9 px circles into visible diagonal chamfers on the ST7789. Four
 * sub-pixel samples per axis keep the edge circular, while every partial
 * pixel belongs to one continuous anti-aliased band (no isolated dots).
 */
static LV_ATTRIBUTE_MEM_ALIGN uint8_t grid_junction_alpha[KEYMAP_GRID_NODE_SIZE * KEYMAP_GRID_NODE_SIZE];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t grid_junction_top_alpha[KEYMAP_GRID_NODE_SIZE * KEYMAP_GRID_NODE_SIZE];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t grid_junction_no_bottom_left_alpha[KEYMAP_GRID_NODE_SIZE * KEYMAP_GRID_NODE_SIZE];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t grid_junction_no_top_left_alpha[KEYMAP_GRID_NODE_SIZE * KEYMAP_GRID_NODE_SIZE];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t grid_junction_top_right_alpha[KEYMAP_GRID_NODE_SIZE * KEYMAP_GRID_NODE_SIZE];
static bool grid_junction_alpha_ready;

static lv_img_dsc_t grid_junction_image;
static lv_img_dsc_t grid_junction_top_image;
static lv_img_dsc_t grid_junction_no_bottom_left_image;
static lv_img_dsc_t grid_junction_no_top_left_image;
static lv_img_dsc_t grid_junction_top_right_image;

static void init_grid_junction_alpha(void) {
    if (grid_junction_alpha_ready) return;

    memset(grid_junction_alpha, 0, sizeof(grid_junction_alpha));
    memset(grid_junction_top_alpha, 0, sizeof(grid_junction_top_alpha));
    memset(grid_junction_no_bottom_left_alpha, 0, sizeof(grid_junction_no_bottom_left_alpha));
    memset(grid_junction_no_top_left_alpha, 0, sizeof(grid_junction_no_top_left_alpha));
    memset(grid_junction_top_right_alpha, 0, sizeof(grid_junction_top_right_alpha));
    memset(&grid_junction_image, 0, sizeof(grid_junction_image));
    memset(&grid_junction_top_image, 0, sizeof(grid_junction_top_image));
    const uint8_t center_lo = KEYMAP_GRID_NODE_SIZE / 2U - 1U;
    const uint8_t center_hi = KEYMAP_GRID_NODE_SIZE / 2U;
    enum {
        GRID_AA_SAMPLES = 4,
        GRID_AA_SAMPLE_COUNT = GRID_AA_SAMPLES * GRID_AA_SAMPLES,
        GRID_AA_UNITS_PER_PIXEL = GRID_AA_SAMPLES * 2,
    };
    const int16_t radius_scaled = KEYMAP_GRID_FILLET_RADIUS * GRID_AA_UNITS_PER_PIXEL;
    const int32_t radius_sq = (int32_t)radius_scaled * radius_scaled;

    for (uint8_t y = 0; y < KEYMAP_GRID_NODE_SIZE; ++y) {
        for (uint8_t x = 0; x < KEYMAP_GRID_NODE_SIZE; ++x) {
            uint8_t coverage = 0;

            if (x == center_lo || x == center_hi || y == center_lo || y == center_hi) {
                coverage = GRID_AA_SAMPLE_COUNT;
            } else {
                const int16_t corner_x = x < center_lo ? GRID_AA_UNITS_PER_PIXEL / 2 :
                    KEYMAP_GRID_NODE_SIZE * GRID_AA_UNITS_PER_PIXEL - GRID_AA_UNITS_PER_PIXEL / 2;
                const int16_t corner_y = y < center_lo ? GRID_AA_UNITS_PER_PIXEL / 2 :
                    KEYMAP_GRID_NODE_SIZE * GRID_AA_UNITS_PER_PIXEL - GRID_AA_UNITS_PER_PIXEL / 2;

                for (uint8_t sy = 0; sy < GRID_AA_SAMPLES; ++sy) {
                    for (uint8_t sx = 0; sx < GRID_AA_SAMPLES; ++sx) {
                        const int16_t sample_x = (int16_t)x * GRID_AA_UNITS_PER_PIXEL + sx * 2U + 1U;
                        const int16_t sample_y = (int16_t)y * GRID_AA_UNITS_PER_PIXEL + sy * 2U + 1U;
                        const int16_t dx = sample_x - corner_x;
                        const int16_t dy = sample_y - corner_y;
                        if ((int32_t)dx * dx + (int32_t)dy * dy > radius_sq) {
                            ++coverage;
                        }
                    }
                }
            }

            /* Preserve the complete contour at T-junctions beside empty cells. */
            uint8_t alpha = (uint8_t)(((uint16_t)coverage * 255U + GRID_AA_SAMPLE_COUNT / 2U) /
                                      GRID_AA_SAMPLE_COUNT);
            uint16_t pixel = (uint16_t)y * KEYMAP_GRID_NODE_SIZE + x;
            grid_junction_alpha[pixel] = alpha;
            /* At a bottom T-junction only the two upper key corners exist.
             * Keep their fillets and the horizontal separator, but do not
             * draw a vertical tail into the empty row below. */
            grid_junction_top_alpha[pixel] = y <= center_hi ? alpha : 0;
            /* No arc in the empty lower-left cell. */
            grid_junction_no_bottom_left_alpha[pixel] =
                (x < center_lo && y > center_hi) ? 0 : alpha;
            /* Shifted PRINT leaves the upper-left cell empty. */
            grid_junction_no_top_left_alpha[pixel] =
                (x < center_lo && y < center_lo) ? 0 : alpha;
            /* The upper-right cell needs only its inner fillet. */
            grid_junction_top_right_alpha[pixel] =
                (x >= center_lo && y <= center_hi) ? alpha : 0;
        }
    }
    grid_junction_image.header.w = KEYMAP_GRID_NODE_SIZE;
    grid_junction_image.header.h = KEYMAP_GRID_NODE_SIZE;
    grid_junction_image.header.cf = LV_IMG_CF_ALPHA_8BIT;
    grid_junction_image.data_size = sizeof(grid_junction_alpha);
    grid_junction_image.data = grid_junction_alpha;
    grid_junction_top_image = grid_junction_image;
    grid_junction_top_image.data_size = sizeof(grid_junction_top_alpha);
    grid_junction_top_image.data = grid_junction_top_alpha;
    grid_junction_no_bottom_left_image = grid_junction_image;
    grid_junction_no_bottom_left_image.data = grid_junction_no_bottom_left_alpha;
    grid_junction_no_top_left_image = grid_junction_image;
    grid_junction_no_top_left_image.data = grid_junction_no_top_left_alpha;
    grid_junction_top_right_image = grid_junction_image;
    grid_junction_top_right_image.data = grid_junction_top_right_alpha;
    grid_junction_alpha_ready = true;
}

static lv_obj_t *make_grid_junction(lv_obj_t *parent, lv_coord_t x, lv_coord_t y) {
    init_grid_junction_alpha();
    lv_obj_t *junction = lv_img_create(parent);
    lv_obj_set_pos(junction, x, y);
    lv_img_set_src(junction, &grid_junction_image);
    lv_obj_set_style_img_recolor(junction, lv_color_hex(KEYMAP_BORDER), 0);
    lv_obj_set_style_img_recolor_opa(junction, LV_OPA_COVER, 0);
    lv_obj_clear_flag(junction, LV_OBJ_FLAG_SCROLLABLE);
    return junction;
}

static uint8_t matrix_col_from_visual(bool left, uint8_t visual_col) {
    return left ? 6U - visual_col : visual_col;
}

static uint8_t visual_col_from_matrix(bool left, uint8_t matrix_col) {
    return left ? 6U - matrix_col : matrix_col;
}

/* The three outer switches on each half are drawn one grid row below
 * their physical matrix row. Keep Vial and display coordinates aligned. */
static uint8_t visual_row_from_matrix(bool left, uint8_t matrix_row, uint8_t matrix_col) {
    return matrix_col == 6U && matrix_row < 3U ? matrix_row + 1U : matrix_row;
}

static uint8_t matrix_row_from_visual(bool left, uint8_t visual_row, uint8_t matrix_col) {
    if (matrix_col == 6U)
        return visual_row >= 1U && visual_row <= 3U ? visual_row - 1U : 5U;
    return visual_row;
}

static uint8_t global_row(bool left, uint8_t local_row) {
    return left ? local_row : local_row + MATRIX_ROWS / 2U;
}

/* Matrix holes are not the same as real switches assigned to KC_NO. */
static bool physical_key_exists(bool left, uint8_t row, uint8_t col) {
    if (row >= 5 || col >= 7) return false;
    if (row == 4) return col <= 3;
    if (left) {
        return row <= 2 || col <= 5; /* left C7: R1-R3 */
    }
    return row <= 2 || col <= 5; /* right C7: physical R1-R3, drawn R2-R4 */
}

static bool visual_key_exists(bool left, uint8_t visual_row, uint8_t matrix_col) {
    return physical_key_exists(left, matrix_row_from_visual(left, visual_row, matrix_col), matrix_col);
}

/* Cyrillic punctuation follows the assigned base keycode, not its old
 * physical position. Other Russian letters retain the physical layout. */
static const char *secondary_legend(bool left, uint8_t row, uint8_t col, uint16_t keycode) {
    switch (keycode) {
        case KC_GRV:  return "Ё";
        case KC_LBRC: return "Х";
        case KC_QUOT: return "Э";
        case KC_RBRC: return "Ъ";
        case KC_SCLN: return "Ж";
        case KC_COMM: return "Б";
        case KC_DOT:  return "Ю";
    }
    if (keycode < KC_A || keycode > KC_Z) return "";
    if (left) {
        if (row == 1) {
            static const char *const letters[] = {"Е", "К", "У", "Ц", "Й"};
            if (col <= 4) return letters[col];
        }
        if (row == 2) {
            static const char *const letters[] = {"П", "А", "В", "Ы", "Ф"};
            if (col <= 4) return letters[col];
        }
        if (row == 3) {
            static const char *const letters[] = {"И", "М", "С", "Ч", "Я"};
            if (col <= 4) return letters[col];
        }
    } else {
        if (row == 1) {
            static const char *const letters[] = {"Н", "Г", "Ш", "Щ", "З"};
            if (col <= 4) return letters[col];
        }
        if (row == 2) {
            static const char *const letters[] = {"Р", "О", "Л", "Д", "Ж"};
            if (col <= 4) return letters[col];
        }
        if (row == 3) {
            static const char *const letters[] = {"Т", "Ь", "Б", "Ю", "."};
            if (col <= 4) return letters[col];
        }
    }
    return "";
}

/* US HID Shift legends are independent of the host's language selection. */
static const char *shifted_legend(uint16_t keycode) {
    switch (keycode) {
        case KC_1: return "!";
        case KC_2: return "@";
        case KC_3: return "#";
        case KC_4: return "$";
        case KC_5: return "%";
        case KC_6: return "^";
        case KC_7: return "&";
        case KC_8: return "*";
        case KC_9: return "(";
        case KC_0: return ")";
        case KC_GRV: return "~";
        case KC_MINS: return "_";
        case KC_EQL: return "+";
        case KC_LBRC: return "{";
        case KC_RBRC: return "}";
        case KC_BSLS: return "|";
        case KC_SCLN: return ":";
        case KC_QUOT: return "\"";
        case KC_COMM: return "<";
        case KC_DOT: return ">";
        case KC_SLSH: return "?";
        default: return "";
    }
}

static uint8_t text_codepoints(const char *text) {
    uint8_t count = 0;
    while (*text) {
        if (((uint8_t)*text & 0xC0U) != 0x80U) ++count;
        ++text;
    }
    return count;
}

/* Entropy can assign many QMK families, including compound icon labels. A
 * private-use symbol needs Iosevka; Zen has only ordinary Latin/Cyrillic. */
static bool text_has_icon(const char *text) {
    while (*text) {
        if ((uint8_t)text[0] >= 0xEEU ||
            ((uint8_t)text[0] == 0xEFU && (uint8_t)text[1] >= 0x80U)) return true;
        ++text;
    }
    return false;
}

static bool keycode_is_layer_switch(uint16_t keycode) {
    return (keycode >= QK_LAYER_TAP && keycode <= QK_ONE_SHOT_LAYER_MAX) ||
           (keycode >= QK_LAYER_TAP_TOGGLE && keycode <= QK_PERSISTENT_DEF_LAYER_MAX);
}

static const char *display_keycode_text(uint16_t keycode, char *fallback) {
    switch (keycode) {
        case KC_QUOT: return "'";
        case KC_PSCR: return "PRINT";
        case KC_CAPS: return "CAPS";
        case KC_LSFT:
        case KC_RSFT: return "SHIFT";
        case KC_LCTL:
        case KC_RCTL: return "CTRL";
        case KC_LALT:
        case KC_RALT: return "ALT";
        case KC_LGUI:
        case KC_RGUI: return "WIN";
        case KC_SPC:  return "SPACE";
        case KC_ENT:  return "ENTER";
        case KC_TAB:  return "TAB";
        case KC_BSPC: return "BACK";
        case KC_VOLU: return "VOL+";
        case KC_VOLD: return "VOL-";
        case KC_MUTE: return "MUTE";
        case KC_MPRV: return "PREV";
        case KC_MNXT: return "NEXT";
        case KC_MPLY: return "PLAY";
        case KC_UP:   return "UP";
        case KC_DOWN: return "DOWN";
        case KC_LEFT: return "LEFT";
        case KC_RGHT: return "RIGHT";
        case CW_TOGG: return "CAPS WD";
    }
    if (keycode_is_layer_switch(keycode)) return EH_SYMBOL_LAYER;
    get_keycode_str(fallback, keycode);
    return fallback;
}

static void style_key(key_widgets_t *widget, key_visual_state_t state, uint8_t release_mix, uint8_t effect_mix) {
    lv_color_t bg = lv_color_hex(KEYMAP_KEY_BG);
    lv_color_t fg = lv_color_hex(KEYMAP_TEXT);

    if (state == KEY_VISUAL_PRESSED) {
        bg = lv_color_hex(KEYMAP_ACTIVE_BG);
        fg = lv_color_hex(KEYMAP_ACTIVE_TEXT);
        lv_obj_set_style_shadow_width(widget->box, 0, 0);
        lv_obj_set_style_shadow_spread(widget->box, 0, 0);
        lv_obj_set_style_shadow_opa(widget->box, LV_OPA_TRANSP, 0);
    } else {
        lv_obj_set_style_shadow_width(widget->box, 0, 0);
        lv_obj_set_style_shadow_spread(widget->box, 0, 0);
        lv_obj_set_style_shadow_opa(widget->box, LV_OPA_TRANSP, 0);
        if (state == KEY_VISUAL_RELEASED) {
            bg = lv_color_mix(lv_color_hex(KEYMAP_RELEASE_BG), lv_color_hex(KEYMAP_KEY_BG), release_mix);
            fg = lv_color_mix(lv_color_hex(KEYMAP_ACTIVE_TEXT), lv_color_hex(KEYMAP_TEXT), release_mix);
        }
    }

#if SM62_EFFECT == 1
    if (state == KEY_VISUAL_REST && effect_mix) {
        fg = lv_color_mix(lv_color_hex(effect_mix > 25U ? 0x10100E : 0xC1B18F), fg, effect_mix > 25U ? 54U : 50U);
    }
#elif SM62_EFFECT == 2 || SM62_EFFECT == 9
    if (state == KEY_VISUAL_REST && effect_mix) {
        fg = lv_color_mix(lv_color_hex(0xC7B695), fg, effect_mix);
    }
    /* A restrained contour travels between cells; no full-cell bloom. */
    lv_obj_set_style_border_width(widget->box, state == KEY_VISUAL_REST && effect_mix ? 1 : 0, 0);
    lv_obj_set_style_border_color(widget->box, lv_color_hex(KEYMAP_ACTIVE_BG), 0);
    lv_obj_set_style_border_opa(widget->box, state == KEY_VISUAL_REST ? (uint8_t)(effect_mix * 3U) : LV_OPA_TRANSP, 0);
#elif SM62_EFFECT == 6
    if (state == KEY_VISUAL_REST && effect_mix) {
        bg = lv_color_mix(lv_color_hex(KEYMAP_ACTIVE_BG), bg, effect_mix);
        fg = lv_color_mix(lv_color_hex(0xC7B695), fg, effect_mix);
    }
#elif SM62_EFFECT == 8
    if (state == KEY_VISUAL_PRESSED && effect_mix) {
        bg = lv_color_mix(lv_color_hex(KEYMAP_KEY_BG), bg, effect_mix);
    }
#else
    (void)effect_mix;
#endif
#if SM62_EFFECT == 4
    lv_obj_set_style_border_width(widget->box, state == KEY_VISUAL_PRESSED && effect_mix ? (effect_mix > 40U ? 2 : 1) : 0, 0);
    lv_obj_set_style_border_color(widget->box, lv_color_hex(0xFFF1C9), 0);
    lv_obj_set_style_border_opa(widget->box, state == KEY_VISUAL_PRESSED && effect_mix ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
#endif
    lv_obj_set_style_bg_color(widget->box, bg, 0);
    lv_obj_set_style_radius(widget->box,
#if SM62_EFFECT == 2 || SM62_EFFECT == 6 || SM62_EFFECT == 9
                            (state == KEY_VISUAL_REST && effect_mix == 0U) ? 0 : KEYMAP_ACTIVE_RADIUS,
#else
                            state == KEY_VISUAL_REST ? 0 : KEYMAP_ACTIVE_RADIUS,
#endif
                            0);
    if (widget->icon) lv_obj_set_style_img_recolor(widget->icon, fg, 0);
    lv_obj_set_style_text_color(widget->primary, fg, 0);
    lv_obj_set_style_text_color(widget->secondary, fg, 0);
    lv_obj_set_style_text_color(widget->shifted, fg, 0);
}

static void update_widget_content(key_widgets_t *widget, bool left, uint8_t layer, uint8_t row, uint8_t col) {
    if (!physical_key_exists(left, row, col)) {
        lv_obj_add_flag(widget->box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Match the effective key seen through transparent layers. KC_NO is an
     * explicit empty binding and must stop inheritance. */
    uint8_t source_layer = layer;
    while (source_layer > 0 && hid_sm62_default_keycode(source_layer, global_row(left, row), col) == KC_TRNS) --source_layer;
    uint16_t keycode = sm62_effective_keycode(layer, global_row(left, row), col);
    widget->icon_running = false;
    for (uint8_t c = 0; c < 62; ++c) {
        if (sm62_application_positions[c][0] != global_row(left, row) || sm62_application_positions[c][1] != col) continue;
        uint8_t visual, state;
        if (!hid_app_layout_get_visual(layer, c, &visual, &state)) break;
        if (!widget->icon) {
            widget->icon = lv_img_create(widget->box);
            lv_obj_set_pos(widget->icon, 18, 17);
        }
        memset(widget->icon_bits, 0, sizeof(widget->icon_bits));
        if (state == 1) integration_icon_spinner(widget->icon_bits, (timer_read32() / 120) % 8);
        else if (state == 2) integration_icon_check(widget->icon_bits);
        else if (state == 3) integration_icon_error(widget->icon_bits);
        else integration_icon_base(widget->icon_bits, visual);
        widget->icon_running = state == 1;
        widget->icon_dsc = (lv_img_dsc_t){
            .header.always_zero = 0, .header.w = EH_ICON_RENDER_SIZE,
            .header.h = EH_ICON_RENDER_SIZE, .header.cf = LV_IMG_CF_ALPHA_1BIT,
            .data_size = EH_ICON_RENDER_BYTES, .data = widget->icon_bits,
        };
        lv_img_cache_invalidate_src(&widget->icon_dsc);
        lv_img_set_src(widget->icon, &widget->icon_dsc);
        lv_obj_set_style_img_recolor(widget->icon, lv_color_hex(KEYMAP_TEXT), 0);
        lv_obj_set_style_img_recolor_opa(widget->icon, LV_OPA_COVER, 0);
        lv_obj_clear_flag(widget->box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget->icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget->primary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget->secondary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget->shifted, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (widget->icon) lv_obj_add_flag(widget->icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget->primary, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget->secondary, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget->shifted, LV_OBJ_FLAG_HIDDEN);
    if (keycode == KC_NO || keycode == KC_TRNS) {
        lv_obj_clear_flag(widget->box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_static(widget->primary, "");
        lv_label_set_text_static(widget->secondary, "");
        lv_label_set_text_static(widget->shifted, "");
        return;
    }

    lv_obj_clear_flag(widget->box, LV_OBJ_FLAG_HIDDEN);
    const char *text = display_keycode_text(keycode, widget->primary_text);
    bool icon = text_has_icon(text);
    /* Only actual single-character glyphs use the existing 28 px lettering.
     * Esc, F1, Ins, Pau and every other function use one optical scale. */
    bool compact = icon || text_codepoints(text) != 1U;
    const lv_font_t *font = icon ? &eh_font_iosevka_18 :
                            (compact ? &sm62_font_zen_medium_17 : &sm62_font_zen_medium_28);
    lv_obj_set_style_text_font(widget->primary, font, 0);
    if (compact || icon) {
        /* A single standard size for words, with wrapping for longer QMK
         * names. Unlike the old 23 px label, two-line codes are not clipped. */
        lv_point_t text_size;
        lv_txt_get_size(&text_size, text, font, 0, 0, 63, LV_TEXT_FLAG_NONE);
        lv_coord_t y = text_size.y >= 61 ? 4 : (69 - text_size.y) / 2;
        lv_obj_set_pos(widget->primary, 3, y);
        lv_obj_set_size(widget->primary, 63, 69 - y - 3);
        lv_obj_set_style_text_align(widget->primary, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(widget->primary, LV_LABEL_LONG_WRAP);
    } else {
        lv_obj_set_pos(widget->primary, 9, 4);
        lv_obj_set_size(widget->primary, 56, 34);
        lv_obj_set_style_text_align(widget->primary, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(widget->primary, LV_LABEL_LONG_CLIP);
    }
    lv_label_set_text(widget->primary, text);
    lv_label_set_text_static(widget->secondary, source_layer == 0 && sm62_local_display_settings()->cyrillic_enabled ? secondary_legend(left, row, col, keycode) : "");
    lv_label_set_text_static(widget->shifted, shifted_legend(keycode));
}

static void reset_focus(bool left) {
    first_row = 1;
    first_visual_col = left ? 2 : 0; /* QWE / ASD / ZXC; YUI / HJK / NM, */
}

/* Center every real press, clamping to the physical span through that key.
 * Shifted outside switches and the short thumb row have their own edges;
 * the rectangular matrix bounds alone would leave empty rows/columns. */
static void focus_key(bool left, uint8_t row, uint8_t col) {
    if (!physical_key_exists(left, row, col)) return;
    uint8_t visual_row = visual_row_from_matrix(left, row, col);
    uint8_t visual_col = visual_col_from_matrix(left, col);
    uint8_t min_row = visual_row, max_row = visual_row;
    uint8_t min_col = visual_col, max_col = visual_col;
    while (min_row > 0U && visual_key_exists(left, min_row - 1U, col)) --min_row;
    while (visual_key_exists(left, max_row + 1U, col)) ++max_row;
    while (min_col > 0U && visual_key_exists(left, visual_row, matrix_col_from_visual(left, min_col - 1U))) --min_col;
    while (max_col < 6U && visual_key_exists(left, visual_row, matrix_col_from_visual(left, max_col + 1U))) ++max_col;

    first_row = visual_row > 0U ? visual_row - 1U : 0U;
    first_visual_col = visual_col > 0U ? visual_col - 1U : 0U;
    if (first_row < min_row) first_row = min_row;
    if (first_row + 2U > max_row) first_row = max_row - 2U;
    if (first_visual_col < min_col) first_visual_col = min_col;
    if (first_visual_col + 2U > max_col) first_visual_col = max_col - 2U;
}

/* Stock layout hold is 200 ms, not the 96 ms color fade. Temporary views
 * use max(1, hold / (2 * 200)); permanent keymap remains unchanged.
 * Read the local setting live for painting and view lifetime alike.
 * uint32_t intermediates are safe across the uint16_t setting range. */
static uint32_t keymap_release_fade_ms(void) {
    uint32_t hold = sm62_local_keymap_hold_ms();
    if (sm62_local_display_settings()->mode == 3 || hold <= 400U)
        return KEYMAP_RELEASE_FADE_MS;
    return (KEYMAP_RELEASE_FADE_MS * hold) / 400U;
}

static bool keymap_release_pending(uint8_t row, uint8_t col) {
    if (released_fading[row][col] &&
        timer_elapsed32(released_at[row][col]) >= keymap_release_fade_ms() + KEYMAP_POST_FADE_HOLD_MS)
        released_fading[row][col] = false;
    return released_fading[row][col];
}

static key_visual_state_t key_visual_state(uint8_t row, uint8_t col, uint8_t *release_mix) {
    *release_mix = 0U;
    if (row >= 5U || col >= 7U) return KEY_VISUAL_REST;
    if (old_pressed[row][col]) return KEY_VISUAL_PRESSED;
    if (!keymap_release_pending(row, col)) return KEY_VISUAL_REST;

    uint32_t elapsed = timer_elapsed32(released_at[row][col]);
    uint32_t fade = keymap_release_fade_ms();
    if (elapsed >= fade) {
        /* Keep the layout visible for 70 ms after the release fade, while
         * rendering the key in its normal resting state. */
        return KEY_VISUAL_REST;
    }

    *release_mix = (uint8_t)(((fade - elapsed) * 255U) / fade);
    return KEY_VISUAL_RELEASED;
}

static void refresh_grid_lines(bool left) {
    /* Keep separators around a missing physical switch when an adjacent
     * switch exists; omit only segments between two empty cells. */
    for (uint8_t row = 0; row < 3; ++row) {
        uint8_t actual_row = first_row + row;
        for (uint8_t boundary = 0; boundary < 4; ++boundary) {
            bool left_exists = false;
            bool right_exists = false;
            if (boundary > 0) {
                uint8_t matrix_col = matrix_col_from_visual(left, first_visual_col + boundary - 1U);
                left_exists = visual_key_exists(left, actual_row, matrix_col);
            }
            if (boundary < 3) {
                uint8_t matrix_col = matrix_col_from_visual(left, first_visual_col + boundary);
                right_exists = visual_key_exists(left, actual_row, matrix_col);
            }
            lv_obj_t *line = vertical_grid_lines[row * 4U + boundary];
            /* The reference has open outer edges: only internal separators exist. */
            if (boundary > 0 && boundary < 3 &&
                (left_exists || right_exists)) {
                lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    for (uint8_t boundary = 0; boundary < 4; ++boundary) {
        for (uint8_t col = 0; col < 3; ++col) {
            bool top_exists = false;
            bool bottom_exists = false;
            uint8_t matrix_col = matrix_col_from_visual(left, first_visual_col + col);
            if (boundary > 0) {
                top_exists = visual_key_exists(left, first_row + boundary - 1U, matrix_col);
            }
            if (boundary < 3) {
                bottom_exists = visual_key_exists(left, first_row + boundary, matrix_col);
            }
            lv_obj_t *line = horizontal_grid_lines[boundary * 3U + col];
            if (boundary > 0 && boundary < 3 &&
                (top_exists || bottom_exists)) {
                lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* Each visible node is a transparent raster of the four SVG fillets.
     * Unlike v12, no background mask can cut into the active key fill. */
    for (uint8_t row_boundary = 1; row_boundary < 3; ++row_boundary) {
        for (uint8_t col_boundary = 1; col_boundary < 3; ++col_boundary) {
            uint8_t top_row = first_row + row_boundary - 1U;
            uint8_t bottom_row = first_row + row_boundary;
            uint8_t left_col = matrix_col_from_visual(left, first_visual_col + col_boundary - 1U);
            uint8_t right_col = matrix_col_from_visual(left, first_visual_col + col_boundary);
            bool top_left = visual_key_exists(left, top_row, left_col);
            bool top_right = visual_key_exists(left, top_row, right_col);
            bool bottom_left = visual_key_exists(left, bottom_row, left_col);
            bool bottom_right = visual_key_exists(left, bottom_row, right_col);
            uint8_t occupied = (uint8_t)top_left + (uint8_t)top_right +
                               (uint8_t)bottom_left + (uint8_t)bottom_right;
            uint8_t node = (row_boundary - 1U) * 2U + col_boundary - 1U;
            const lv_img_dsc_t *image = NULL;
            if (top_left && top_right && !bottom_left && bottom_right) {
                /* Round this physical Z/ALT junction in every visible viewport. */
                image = left && top_row == 3U && left_col == 4U && right_col == 3U
                            ? &grid_junction_image : &grid_junction_no_bottom_left_image;
            } else if (!top_left && top_right && !bottom_left && !bottom_right) {
                image = &grid_junction_top_right_image;
            } else if (!top_left && top_right && bottom_left && bottom_right) {
                /* PRINT/ESC view: round the marked upper-left side of this node. */
                image = left && first_row == 0U && first_visual_col == 0U &&
                        row_boundary == 1U && col_boundary == 1U
                            ? &grid_junction_image : &grid_junction_no_top_left_image;
            } else if (occupied >= 3U) {
                image = &grid_junction_image;
            } else if (top_left && top_right && !bottom_left && !bottom_right) {
                image = &grid_junction_top_image;
            }
            if (image) {
                lv_img_set_src(grid_junctions[node], image);
                lv_obj_clear_flag(grid_junctions[node], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(grid_junctions[node], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void refresh_grid(bool left, uint8_t layer, bool content_refresh) {
    bool geometry_changed = false;
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t col = 0; col < 3; ++col) {
            uint8_t index = row * 3U + col;
            uint8_t visual_row = first_row + row;
            uint8_t matrix_col = matrix_col_from_visual(left, first_visual_col + col);
            uint8_t actual_row = matrix_row_from_visual(left, visual_row, matrix_col);
            if (content_refresh || rendered_row[index] != actual_row || rendered_col[index] != matrix_col) {
                rendered_row[index] = actual_row;
                rendered_col[index] = matrix_col;
                rendered_state[index] = 0xFF;
                rendered_mix[index] = 0xFF;
#if SM62_EFFECT > 0
                effect_rendered[index] = 0xFF;
#endif
                update_widget_content(&keys[index], left, layer, actual_row, matrix_col);
                geometry_changed = true;
            }
            uint8_t release_mix;
            key_visual_state_t state = key_visual_state(actual_row, matrix_col, &release_mix);
            uint8_t effect_mix = 0;
#if SM62_EFFECT > 0
            effect_mix = effect_at(actual_row, matrix_col);
#if SM62_EFFECT == 4
            if (state == KEY_VISUAL_PRESSED) {
                for (uint8_t i = 0; i < SM62_EFFECT_SLOTS; ++i) {
                    if (effect_events[i].active && effect_events[i].row == actual_row && effect_events[i].col == matrix_col) {
                        uint8_t ring = effect_envelope(timer_elapsed32(effect_events[i].started), 0, 100, 80);
                        if (ring > effect_mix) effect_mix = ring;
                    }
                }
            }
#endif
#endif
            if (rendered_state[index] != state || (state == KEY_VISUAL_RELEASED && rendered_mix[index] != release_mix)
#if SM62_EFFECT > 0
                || effect_rendered[index] != effect_mix
#endif
                ) {
                rendered_state[index] = state;
                rendered_mix[index] = release_mix;
#if SM62_EFFECT > 0
                effect_rendered[index] = effect_mix;
#endif
                style_key(&keys[index], state, release_mix, effect_mix);
            }
        }
    }
    if (geometry_changed) refresh_grid_lines(left);
#if SM62_EFFECT > 0
    render_grid_effect(left);
#endif
}

/* Main-loop capture only: never move or invalidate LVGL objects from input.
 * Retain the last real local matrix press even if release precedes the next
 * housekeeping. No fabricated Combo/Tap-Dance/magic-row event reaches here. */
static struct {
    bool pending, released;
    uint8_t row, col, layer;
    uint32_t pressed_at, released_at;
} captured_press;

#if defined(SM62_DUALCORE_RENDER)
static bool dc_feedback_pending;
static void dc_keymap_owner_event(uint8_t row, uint8_t col, bool pressed) {
#else
void display_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
#endif
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return;
    bool left = is_keyboard_left();
    if ((row < MATRIX_ROWS / 2) != left) return;
    row %= MATRIX_ROWS / 2;
    if (pressed) {
        captured_press.row = row;
        captured_press.col = col;
        captured_press.layer = get_current_layer();
        captured_press.pressed_at = timer_read32();
        captured_press.pending = true;
        captured_press.released = false;
    } else if (captured_press.pending && captured_press.row == row && captured_press.col == col) {
        captured_press.released = true;
        captured_press.released_at = timer_read32();
    }
}

static bool scan_keys(bool left) {
    bool changed = false;
    for (uint8_t row = 0; row < 5; ++row) {
        for (uint8_t col = 0; col < 7; ++col) {
            bool pressed = matrix_is_on(global_row(left, row), col);
            if (pressed != old_pressed[row][col]) {
                old_pressed[row][col] = pressed;
                changed = true;
                if (pressed) {
                    recent_press_valid = true;
                    recent_press_row = row;
                    recent_press_col = col;
                    released_fading[row][col] = false;
                    if (focus_next_press_after_layer_change) {
                        uint16_t keycode = sm62_effective_keycode(get_current_layer(), global_row(left, row), col);
                        if (!keycode_is_layer_switch(keycode)) {
                            focus_key(left, row, col);
                            focus_next_press_after_layer_change = false;
                        } else {
                            focus_key(left, row, col);
                        }
                    } else {
                        focus_key(left, row, col);
                    }
#if SM62_EFFECT > 0
                    effect_start(row, col);
#endif
                } else {
                    released_fading[row][col] = true;
                    released_at[row][col] = timer_read32();
                    fade_frame_timer = released_at[row][col];
                }
            }
        }
    }
    return changed;
}

/* Observe every scan, including while MAIN/ORBIT is visible. A short tap
 * must not disappear while LVGL is drawing the previous screen. */
bool sm62_keymap_observe_matrix(void) {
    bool changed = scan_keys(is_keyboard_left());
#if defined(SM62_DUALCORE_RENDER)
    changed |= dc_feedback_pending;
    dc_feedback_pending = false;
#endif
    if (captured_press.pending) {
        bool left = is_keyboard_left();
        uint8_t row = captured_press.row, col = captured_press.col;
        recent_press_valid = true;
        recent_press_row = row;
        recent_press_col = col;
        if (focus_next_press_after_layer_change && !keycode_is_layer_switch(sm62_effective_keycode(captured_press.layer, global_row(left, row), col))) {
            focus_key(left, row, col);
            focus_next_press_after_layer_change = false;
        } else {
            focus_key(left, row, col);
        }
        if (captured_press.released) {
            released_fading[row][col] = true;
            released_at[row][col] = captured_press.released_at;
            fade_frame_timer = captured_press.released_at;
        }
        captured_press.pending = false;
        changed = true;
    }
    pending_matrix_change |= changed;
    return changed;
}

#if defined(SM62_DUALCORE_RENDER)
void sm62_keymap_owner_feedback(uint64_t presses, uint64_t releases, bool last, uint8_t row, uint8_t col) {
    if (!(presses | releases)) return;
    uint32_t visible_at = timer_read32();
    for (uint8_t r=0;r<5;r++) for (uint8_t c=0;c<7;c++) {
        uint64_t bit=UINT64_C(1)<<(r*7U+c);
        if (presses & bit) {
            released_fading[r][c]=false;
#if SM62_EFFECT > 0
            effect_start(r,c);
#endif
        }
        if ((releases & bit) && !matrix_is_on(global_row(is_keyboard_left(),r),c)) {
            /* Visual presentation time only; native debounce/TD is untouched. */
            released_fading[r][c]=true;
            released_at[r][c]=visible_at;
            fade_frame_timer=visible_at;
        }
    }
    if (last) {
        dc_keymap_owner_event(row,col,true);
        if (!matrix_is_on(row,col)) dc_keymap_owner_event(row,col,false);
    }
    dc_feedback_pending=true;
}
#endif

static void screen_keymap_init(void) {
    screen_keymap = display_view_create();
    lv_obj_set_style_pad_all(screen_keymap, 0, 0);
    lv_obj_set_style_border_width(screen_keymap, 0, 0);
    lv_obj_set_style_bg_color(screen_keymap, lv_color_hex(KEYMAP_BG), 0);
    lv_obj_set_style_bg_opa(screen_keymap, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_keymap, LV_OBJ_FLAG_SCROLLABLE);
    for (uint8_t i = 0; i < ARRAY_SIZE(keys); ++i) {
        keys[i] = make_key(screen_keymap, 10 + (i % 3U) * 75, 30 + (i / 3U) * 75);
    }
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t boundary = 0; boundary < 4; ++boundary) {
            static const lv_coord_t line_y[3] = {30, 103, 178};
            /* Reach the next visible horizontal separator even when its node is hidden. */
            static const lv_coord_t line_h[3] = {73, 75, 72};
            lv_coord_t x = 6 + boundary * 75;
            vertical_grid_lines[row * 4U + boundary] = make_grid_line(screen_keymap, x, line_y[row], KEYMAP_GRID_LINE_SIZE, line_h[row]);
        }
    }
    for (uint8_t boundary = 0; boundary < 4; ++boundary) {
        for (uint8_t col = 0; col < 3; ++col) {
            static const lv_coord_t line_x[3] = {10, 83, 158};
            /* Meet the vertical separator without one-pixel gaps at T-junctions. */
            static const lv_coord_t line_w[3] = {73, 75, 73};
            static const lv_coord_t line_y[4] = {26, 101, 176, 251};
            horizontal_grid_lines[boundary * 3U + col] = make_grid_line(screen_keymap, line_x[col], line_y[boundary], line_w[col], KEYMAP_GRID_LINE_SIZE);
        }
    }
    static const lv_coord_t node_x[2] = {72, 147};
    static const lv_coord_t node_y[2] = {92, 167};
    for (uint8_t row = 0; row < 2; ++row) {
        for (uint8_t col = 0; col < 2; ++col) {
            uint8_t node = row * 2U + col;
            lv_coord_t x = node_x[col];
            lv_coord_t y = node_y[row];
            grid_junctions[node] = make_grid_junction(screen_keymap, x, y);
        }
    }
    for (uint8_t i = 0; i < ARRAY_SIZE(vertical_grid_lines); ++i) lv_obj_move_foreground(vertical_grid_lines[i]);
    for (uint8_t i = 0; i < ARRAY_SIZE(horizontal_grid_lines); ++i) lv_obj_move_foreground(horizontal_grid_lines[i]);
    for (uint8_t i = 0; i < ARRAY_SIZE(grid_junctions); ++i) lv_obj_move_foreground(grid_junctions[i]);
    previous_side_left = is_keyboard_left();
    reset_focus(previous_side_left);
    memset(old_pressed, 0, sizeof(old_pressed));
    memset(released_fading, 0, sizeof(released_fading));
    memset(released_at, 0, sizeof(released_at));
    memset(rendered_row, 0xFF, sizeof(rendered_row));
    memset(rendered_col, 0xFF, sizeof(rendered_col));
    memset(rendered_state, 0xFF, sizeof(rendered_state));
    memset(rendered_mix, 0xFF, sizeof(rendered_mix));
#if SM62_EFFECT > 0
    memset(effect_events, 0, sizeof(effect_events));
    memset(effect_rendered, 0xFF, sizeof(effect_rendered));
    memset(grid_rendered, 0xFF, sizeof(grid_rendered));
#endif
    focus_next_press_after_layer_change = false;
    recent_press_valid = false;
    force_refresh = true;
}

static void screen_keymap_housekeep(void);

static void screen_keymap_load(void) {
    /* Populate the hidden view and sample the current matrix before the
     * full-screen switch; the first visible frame already has live content. */
    force_refresh = true;
    screen_keymap_housekeep();
    display_show_view(screen_keymap);
}

static void screen_keymap_housekeep(void) {
    bool left = is_keyboard_left();
    uint8_t layer = get_current_layer();
    bool changed = pending_matrix_change | scan_keys(left);
    pending_matrix_change = false;
    bool content_refresh = force_refresh;
    static uint32_t visual_generation;
    uint32_t generation = hid_app_layout_visual_generation();
    if (generation != visual_generation) {
        visual_generation = generation;
        changed = true;
        content_refresh = true;
    }
    static uint32_t icon_frame;
    uint32_t frame = timer_read32() / 120;
    if (frame != icon_frame) {
        icon_frame = frame;
        for (uint8_t i = 0; i < ARRAY_SIZE(keys); ++i) {
            if (keys[i].icon_running) { changed = true; content_refresh = true; break; }
        }
    }
    uint8_t cyrillic_enabled = sm62_local_display_settings()->cyrillic_enabled;
    if (cyrillic_enabled != previous_cyrillic_enabled) {
        previous_cyrillic_enabled = cyrillic_enabled;
        changed = true;
        content_refresh = true;
    }
    if (left != previous_side_left) {
        previous_side_left = left;
        reset_focus(left);
        changed = true;
        content_refresh = true;
    }
    if (layer != previous_layer) {
        uint8_t old_layer = previous_layer;
        previous_layer = layer;
        /* A simultaneous non-layer press can arrive in the same matrix scan. */
        if (old_layer != 0xFF && recent_press_valid) {
            uint16_t keycode = sm62_effective_keycode(old_layer, global_row(left, recent_press_row), recent_press_col);
            if (!keycode_is_layer_switch(keycode)) {
                focus_key(left, recent_press_row, recent_press_col);
                focus_next_press_after_layer_change = false;
            } else {
                focus_next_press_after_layer_change = true;
            }
        } else {
            focus_next_press_after_layer_change = old_layer != 0xFF;
            if (!changed) reset_focus(left);
        }
        changed = true;
        content_refresh = true;
    }
    recent_press_valid = false;
    bool any_release_fading = false;
    for (uint8_t row = 0; row < 5; ++row) {
        for (uint8_t col = 0; col < 7; ++col) {
            if (released_fading[row][col]) {
                if (keymap_release_pending(row, col)) any_release_fading = true;
                else changed = true; /* Ensure final resting frame is painted. */
            }
        }
    }
    if (any_release_fading && timer_elapsed32(fade_frame_timer) >= KEYMAP_FADE_FRAME_MS) {
        fade_frame_timer = timer_read32();
        changed = true;
    }
#if SM62_EFFECT > 0
    bool effect_active = effect_advance();
    bool effect_cleanup = false;
    if (!effect_active) {
        for (uint8_t i = 0; i < ARRAY_SIZE(effect_rendered); ++i) {
            if (effect_rendered[i] != 0 && effect_rendered[i] != 0xFF) effect_cleanup = true;
        }
        for (uint8_t i = 0; i < ARRAY_SIZE(grid_rendered); ++i) {
            if (grid_rendered[i] != 0 && grid_rendered[i] != 0xFF) effect_cleanup = true;
        }
    }
    if ((effect_active || effect_cleanup) && timer_elapsed32(effect_frame_timer) >= 16U) {
        effect_frame_timer = timer_read32();
        changed = true;
    }
#endif
    if (changed || force_refresh) {
        force_refresh = false;
        refresh_grid(left, layer, content_refresh);
    }
}

const eh_screen_t eh_screen_keymap = {
    .init = screen_keymap_init,
    .load = screen_keymap_load,
    .housekeep = screen_keymap_housekeep,
};

/* Used by the low-idle variant to keep the layout visible until every
 * pressed key and release fade has completed. */
bool sm62_keymap_visual_active(void) {
    bool left = is_keyboard_left();
    bool active = false;
    for (uint8_t row = 0; row < 5; ++row) {
        for (uint8_t col = 0; col < 7; ++col) {
            if (matrix_is_on(global_row(left, row), col)) active = true;
            if (keymap_release_pending(row, col)) active = true;
        }
    }
    return active;
}
