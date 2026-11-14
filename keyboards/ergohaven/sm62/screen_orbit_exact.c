// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include <lvgl.h>

#include "ergohaven.h"
#include "src/display/eh_display.h"

LV_IMG_DECLARE(sm62_layer_04);
LV_IMG_DECLARE(anim_00); LV_IMG_DECLARE(anim_01); LV_IMG_DECLARE(anim_02); LV_IMG_DECLARE(anim_03);
LV_IMG_DECLARE(anim_04); LV_IMG_DECLARE(anim_05); LV_IMG_DECLARE(anim_06); LV_IMG_DECLARE(anim_07);
LV_IMG_DECLARE(anim_08); LV_IMG_DECLARE(anim_09); LV_IMG_DECLARE(anim_10); LV_IMG_DECLARE(anim_11);
LV_IMG_DECLARE(anim_12); LV_IMG_DECLARE(anim_13); LV_IMG_DECLARE(anim_14); LV_IMG_DECLARE(anim_15);
LV_IMG_DECLARE(flame_00); LV_IMG_DECLARE(flame_01); LV_IMG_DECLARE(flame_02); LV_IMG_DECLARE(flame_03);
LV_IMG_DECLARE(flame_04); LV_IMG_DECLARE(flame_05); LV_IMG_DECLARE(flame_06); LV_IMG_DECLARE(flame_07);
LV_IMG_DECLARE(flame_08); LV_IMG_DECLARE(flame_09); LV_IMG_DECLARE(flame_10); LV_IMG_DECLARE(flame_11);
LV_IMG_DECLARE(flame_12); LV_IMG_DECLARE(flame_13); LV_IMG_DECLARE(flame_14); LV_IMG_DECLARE(flame_15);

#define ORBIT_WIDTH 240
#define ORBIT_HEIGHT 224
#define ORBIT_SOURCE_WIDTH 240
#define ORBIT_SOURCE_HEIGHT 224
#define ORBIT_COLORS 16
#define ORBIT_FRAME_MS 50U
#define ORBIT_BG 0x070705

static const lv_img_dsc_t *const ship_frames[] = {
    &anim_00, &anim_01, &anim_02, &anim_03, &anim_04, &anim_05, &anim_06, &anim_07,
    &anim_08, &anim_09, &anim_10, &anim_11, &anim_12, &anim_13, &anim_14, &anim_15,
};
static const lv_img_dsc_t *const flame_frames[] = {
    &flame_00, &flame_01, &flame_02, &flame_03, &flame_04, &flame_05, &flame_06, &flame_07,
    &flame_08, &flame_09, &flame_10, &flame_11, &flame_12, &flame_13, &flame_14, &flame_15,
};

static lv_obj_t *screen_orbit;
static lv_obj_t *fallback_image;
static lv_obj_t *animation_image;
static lv_img_dsc_t frame_buffer;
static uint32_t animation_timer;
static int32_t animation_index;
static int8_t frame_index;
static bool frame_drawn;
static bool frame_in_flight;
static bool animation_ready;
static bool animation_started;

static void clear_frame(lv_img_dsc_t *out) {
    lv_color_t background = lv_color_hex(ORBIT_BG);
    lv_color_t *pixels = (lv_color_t *)out->data;
    for (uint32_t i = 0; i < ORBIT_WIDTH * ORBIT_HEIGHT; ++i) {
        pixels[i] = background;
    }
}

static void draw_stars(lv_img_dsc_t *out) {
    static bool stars_initialized;
    static int stars[20][3];
    static const uint32_t rgb[] = {
        0xFFFFFF, 0xFF9223, 0xFFF1EB, 0xE9ECFF,
        0xBFD3FF, 0xFEF9FF, 0xFF7800, 0xBACFFF,
    };
    if (!stars_initialized) {
        for (uint8_t i = 0; i < ARRAY_SIZE(stars); ++i) {
            stars[i][0] = rand() % ORBIT_WIDTH;
            stars[i][1] = rand() % ORBIT_HEIGHT;
            stars[i][2] = rand() % 8 + 3;
        }
        stars_initialized = true;
    }

    clear_frame(out);
    uint16_t *pixels = (uint16_t *)out->data;
    for (uint8_t i = 0; i < ARRAY_SIZE(stars); ++i) {
        int x = stars[i][0];
        int y = stars[i][1];
        int speed = stars[i][2];
        uint32_t c = rgb[i % ARRAY_SIZE(rgb)];
        uint16_t color = lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF).full;

        if (speed >= 7 && x > 0 && x < ORBIT_WIDTH - 2 && y > 0 && y < ORBIT_HEIGHT - 2) {
            pixels[(y - 1) * ORBIT_WIDTH + x] = color;
            pixels[y * ORBIT_WIDTH + x - 1] = color;
            pixels[y * ORBIT_WIDTH + x] = color;
            pixels[y * ORBIT_WIDTH + x + 1] = color;
            pixels[(y + 1) * ORBIT_WIDTH + x] = color;
        } else if (speed >= 5 && x < ORBIT_WIDTH - 1 && y < ORBIT_HEIGHT - 1) {
            pixels[y * ORBIT_WIDTH + x] = color;
            pixels[y * ORBIT_WIDTH + x + 1] = color;
            pixels[(y + 1) * ORBIT_WIDTH + x] = color;
            pixels[(y + 1) * ORBIT_WIDTH + x + 1] = color;
        } else {
            pixels[y * ORBIT_WIDTH + x] = color;
        }

        x -= speed;
        if (x < 0) {
            stars[i][0] = ORBIT_WIDTH - 1;
            stars[i][1] = rand() % ORBIT_HEIGHT;
            stars[i][2] = rand() % 8 + 3;
        } else {
            stars[i][0] = x;
        }
    }
}

static void draw_ship(const lv_img_dsc_t *input, lv_img_dsc_t *output) {
    uint16_t palette[ORBIT_COLORS];
    for (uint8_t i = 0; i < ORBIT_COLORS; ++i) {
        palette[i] = lv_color_make(input->data[i * 3], input->data[i * 3 + 1], input->data[i * 3 + 2]).full;
    }
    int x0 = input->data[ORBIT_COLORS * 3];
    int y0 = input->data[ORBIT_COLORS * 3 + 1];
    int width = input->data[ORBIT_COLORS * 3 + 2];
    int u = 0;
    uint16_t *pixels = (uint16_t *)output->data + y0 * output->header.w + x0;
    for (uint32_t i = ORBIT_COLORS * 3 + 4; i < input->data_size; ++i) {
        uint8_t packed = input->data[i];
        uint8_t index = packed & 0x0F;
        int run = packed >> 4;
        for (; run > 0; --run, ++u) {
            if (u >= width) {
                u -= width;
                pixels += output->header.w;
            }
            if (index) pixels[u] = palette[index];
        }
    }
}

static void draw_flame(const lv_img_dsc_t *input, lv_img_dsc_t *output) {
    uint8_t alpha[ORBIT_COLORS];
    for (uint8_t i = 0; i < ORBIT_COLORS; ++i) alpha[i] = input->data[i * 3 + 1];
    int x0 = input->data[ORBIT_COLORS * 3];
    int y0 = input->data[ORBIT_COLORS * 3 + 1];
    int width = input->data[ORBIT_COLORS * 3 + 2];
    int u = 0;
    lv_color_t *pixels = (lv_color_t *)output->data + y0 * output->header.w + x0;
    for (uint32_t i = ORBIT_COLORS * 3 + 4; i < input->data_size; ++i) {
        uint8_t packed = input->data[i];
        uint8_t index = packed & 0x0F;
        int run = packed >> 4;
        for (; run > 0; --run, ++u) {
            if (u >= width) {
                u -= width;
                pixels += output->header.w;
            }
            if (index) pixels[u] = lv_color_mix(lv_color_white(), pixels[u], alpha[index]);
        }
    }
}

static void screen_orbit_init(void) {
    screen_orbit = display_view_create();
    lv_obj_set_style_pad_all(screen_orbit, 0, 0);
    lv_obj_set_style_border_width(screen_orbit, 0, 0);
    lv_obj_set_style_bg_color(screen_orbit, lv_color_hex(ORBIT_BG), 0);
    lv_obj_set_style_bg_opa(screen_orbit, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_orbit, LV_OBJ_FLAG_SCROLLABLE);

    fallback_image = lv_img_create(screen_orbit);
    lv_img_set_src(fallback_image, &sm62_layer_04);
    lv_obj_set_pos(fallback_image, 0, 0);

    uint16_t *pixels = lv_mem_alloc(ORBIT_WIDTH * ORBIT_HEIGHT * sizeof(uint16_t));
    if (!pixels) return;
    frame_buffer.header.always_zero = 0;
    frame_buffer.header.w = ORBIT_WIDTH;
    frame_buffer.header.h = ORBIT_HEIGHT;
    frame_buffer.header.cf = LV_IMG_CF_TRUE_COLOR;
    frame_buffer.data_size = ORBIT_WIDTH * ORBIT_HEIGHT * sizeof(uint16_t);
    frame_buffer.data = (uint8_t *)pixels;
    clear_frame(&frame_buffer);

    animation_image = lv_img_create(screen_orbit);
    lv_img_set_src(animation_image, &frame_buffer);
    lv_obj_set_pos(animation_image, 0, 28);
    /* The supplied layer-4 bitmap contains a one-pixel top seam. It is only a
     * fallback for allocation failure; hide it when the live buffer exists. */
    lv_obj_add_flag(fallback_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *top_mask = lv_obj_create(screen_orbit);
    lv_obj_set_pos(top_mask, 0, 0);
    lv_obj_set_size(top_mask, ORBIT_WIDTH, 3);
    lv_obj_set_style_radius(top_mask, 0, 0);
    lv_obj_set_style_pad_all(top_mask, 0, 0);
    lv_obj_set_style_border_width(top_mask, 0, 0);
    lv_obj_set_style_bg_color(top_mask, lv_color_hex(ORBIT_BG), 0);
    lv_obj_set_style_bg_opa(top_mask, LV_OPA_COVER, 0);
    lv_obj_clear_flag(top_mask, LV_OBJ_FLAG_SCROLLABLE);
    animation_ready = true;
}

void sm62_orbit_restart(void) { animation_started = false; }

static void screen_orbit_load(void) {
    /* Idle entries replay the arrival; a permanent ship keeps its sequence
     * when the temporary keymap view is dismissed. */
    if (!animation_started) {
        animation_index = 0;
        frame_index = -1;
        srand(timer_read32());
        /* Replace the previous ship frame before exposing this view again. */
        if (animation_ready) draw_stars(&frame_buffer);
        frame_drawn = animation_ready;
        animation_started = true;
    }
    animation_timer = timer_read32();
    frame_in_flight = false;
    display_show_view(screen_orbit);
}

static void screen_orbit_housekeep(void) {
    if (!animation_ready || frame_in_flight) return;
    if (!frame_drawn) {
        draw_stars(&frame_buffer);
        if (frame_index >= 0) {
            draw_ship(ship_frames[frame_index], &frame_buffer);
            draw_flame(flame_frames[frame_index], &frame_buffer);
        }
        frame_drawn = true;
        return;
    }
    if (timer_elapsed32(animation_timer) < ORBIT_FRAME_MS) return;

    display_diag_mark(0x610);
    /* The descriptor and backing storage never change. Resetting the source
     * on every frame needlessly rebuilds image state and adds another full
     * invalidation; one explicit invalidate is sufficient. */
    lv_obj_invalidate(animation_image);
    frame_in_flight = true;
    display_diag_mark(0x611);
    animation_timer = timer_read32();
    animation_index++;

    static const uint8_t table[] = {
        9,15,9,15,9,15,9,15,9,15, 9,15,9,15,9,15,9,15,9,15,
        9,15,9,15,9,15,9,15,9,15, 9,15,9,15,9,15,9,15,9,15,
        10,11,12, 13,14,13,14,13,14,13,14,13,14,
        13,14,13,14,13,14,13,14,13,14, 13,14,13,14,13,14,13,14,13,14,
        12,11,10,
    };
    if (animation_index >= 14) frame_index = table[(animation_index - 14) % ARRAY_SIZE(table)];
    else if (animation_index > 5) frame_index = animation_index - 5;
    else frame_index = -1;
    frame_drawn = false;
}

void sm62_orbit_frame_completed(void) { frame_in_flight = false; }

const eh_screen_t eh_screen_orbit = {
    .init = screen_orbit_init,
    .load = screen_orbit_load,
    .housekeep = screen_orbit_housekeep,
};
