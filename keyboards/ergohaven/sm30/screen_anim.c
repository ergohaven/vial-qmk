#include <lvgl.h>

#include "ergohaven.h"
#include "src/display/lvgl_helpers.h"
#include "src/display/eh_display.h"

static lv_obj_t *screen_anim;

LV_IMG_DECLARE(anim_00);
LV_IMG_DECLARE(anim_01);
LV_IMG_DECLARE(anim_02);
LV_IMG_DECLARE(anim_03);
LV_IMG_DECLARE(anim_04);
LV_IMG_DECLARE(anim_05);
LV_IMG_DECLARE(anim_06);
LV_IMG_DECLARE(anim_07);
LV_IMG_DECLARE(anim_08);
LV_IMG_DECLARE(anim_09);
LV_IMG_DECLARE(anim_10);
LV_IMG_DECLARE(anim_11);
LV_IMG_DECLARE(anim_12);
LV_IMG_DECLARE(anim_13);
LV_IMG_DECLARE(anim_14);
LV_IMG_DECLARE(anim_15);

LV_IMG_DECLARE(flame_00);
LV_IMG_DECLARE(flame_01);
LV_IMG_DECLARE(flame_02);
LV_IMG_DECLARE(flame_03);
LV_IMG_DECLARE(flame_04);
LV_IMG_DECLARE(flame_05);
LV_IMG_DECLARE(flame_06);
LV_IMG_DECLARE(flame_07);
LV_IMG_DECLARE(flame_08);
LV_IMG_DECLARE(flame_09);
LV_IMG_DECLARE(flame_10);
LV_IMG_DECLARE(flame_11);
LV_IMG_DECLARE(flame_12);
LV_IMG_DECLARE(flame_13);
LV_IMG_DECLARE(flame_14);
LV_IMG_DECLARE(flame_15);

static const lv_img_dsc_t *ship_frame[] = {
    &anim_00, &anim_01, &anim_02, &anim_03, &anim_04, &anim_05, &anim_06, &anim_07, &anim_08, &anim_09, //
    &anim_10, &anim_11, &anim_12, &anim_13, &anim_14, &anim_15,                                         //
};

static const lv_img_dsc_t *flame_frame[] = {
    &flame_00, &flame_01, &flame_02, &flame_03, &flame_04, &flame_05, &flame_06, &flame_07, &flame_08, &flame_09, //
    &flame_10, &flame_11, &flame_12, &flame_13, &flame_14, &flame_15,                                             //
};

static lv_obj_t *anim_start;
static uint32_t  anim_timer = 0;
static int32_t   anim_index = 0;

const int    IMG_WIDTH  = 240;
const int    IMG_HEIGHT = 224;
lv_img_dsc_t custom_img_dsc;

void anim_screen_init(void) {
    screen_anim = lv_obj_create(NULL);
    lv_obj_add_style(screen_anim, &style_screen, 0);
    use_flex_column(screen_anim);

    anim_start = lv_img_create(screen_anim);
    lv_obj_align(anim_start, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_top(anim_start, 0, 0);
    lv_obj_set_style_pad_bottom(anim_start, 0, 0);

    const int data_size         = IMG_WIDTH * IMG_HEIGHT * 2;
    uint16_t *image_data_buffer = (uint16_t *)lv_mem_alloc(data_size);
    memset(image_data_buffer, 0, data_size);
    custom_img_dsc.header.always_zero = 0;
    custom_img_dsc.header.w           = IMG_WIDTH;
    custom_img_dsc.header.h           = IMG_HEIGHT;
    custom_img_dsc.data_size          = data_size;
    custom_img_dsc.header.cf          = LV_IMG_CF_TRUE_COLOR; /*Set the color format*/
    custom_img_dsc.data               = (uint8_t *)image_data_buffer;
}

void draw_stars(lv_img_dsc_t *out) {
    static bool stars_init = false;
    static int  stars[20][3];
    if (!stars_init) {
        for (int i = 0; i < ARRAY_SIZE(stars); ++i) {
            stars[i][0] = rand() % IMG_WIDTH;
            stars[i][1] = rand() % IMG_HEIGHT;
            stars[i][2] = rand() % 8 + 3;
        }
        stars_init = true;
    }
    uint16_t colors[] = {
        lv_color_make(255, 255, 255).full, //
        lv_color_make(255, 146, 35).full,  //
        lv_color_make(255, 241, 235).full, //
        lv_color_make(233, 236, 255).full, //
        lv_color_make(191, 211, 255).full, //
        lv_color_make(254, 249, 255).full, //
        lv_color_make(255, 120, 0).full,   //
        lv_color_make(186, 207, 255).full, //
    };
    memset((uint8_t *)out->data, 0, out->data_size);
    uint16_t *out_data = (uint16_t *)(out->data);
    for (int i = 0; i < ARRAY_SIZE(stars); ++i) {
        int x = stars[i][0];
        int y = stars[i][1];
        int d = stars[i][2];

        uint16_t color = colors[i % ARRAY_SIZE(colors)];

        if (d >= 10 && x > 1 && x < IMG_WIDTH - 3 && y > 1 && y < IMG_HEIGHT - 3) {
            out_data[(y - 2) * IMG_WIDTH + x]     = color;
            out_data[(y - 1) * IMG_WIDTH + x - 1] = color;
            out_data[(y - 1) * IMG_WIDTH + x]     = color;
            out_data[(y - 1) * IMG_WIDTH + x + 1] = color;
            out_data[y * IMG_WIDTH + x - 2]       = color;
            out_data[y * IMG_WIDTH + x - 1]       = color;
            out_data[y * IMG_WIDTH + x]           = color;
            out_data[y * IMG_WIDTH + x + 1]       = color;
            out_data[y * IMG_WIDTH + x + 2]       = color;
            out_data[(y + 1) * IMG_WIDTH + x - 1] = color;
            out_data[(y + 1) * IMG_WIDTH + x]     = color;
            out_data[(y + 1) * IMG_WIDTH + x + 1] = color;
            out_data[(y + 2) * IMG_WIDTH + x]     = color;
        } else if (d >= 7 && x > 0 && x < IMG_WIDTH - 2 && y > 0 && y < IMG_HEIGHT - 2) {
            out_data[(y - 1) * IMG_WIDTH + x] = color;
            out_data[y * IMG_WIDTH + x - 1]   = color;
            out_data[y * IMG_WIDTH + x]       = color;
            out_data[y * IMG_WIDTH + x + 1]   = color;
            out_data[(y + 1) * IMG_WIDTH + x] = color;
        } else if (d >= 5 && x >= 0 && x < IMG_WIDTH - 2 && y >= 0 && y < IMG_HEIGHT - 2) {
            out_data[y * IMG_WIDTH + x]           = color;
            out_data[y * IMG_WIDTH + x + 1]       = color;
            out_data[(y + 1) * IMG_WIDTH + x]     = color;
            out_data[(y + 1) * IMG_WIDTH + x + 1] = color;
        } else
            out_data[y * IMG_WIDTH + x] = color;

        x -= d;
        if (x < 0) {
            stars[i][0] = IMG_WIDTH - 1;
            stars[i][1] = rand() % IMG_HEIGHT;
            stars[i][2] = rand() % 8 + 3;
        } else
            stars[i][0] = x;
    }
}

const int NCOLORS = 16;

void draw_ship(const lv_img_dsc_t *in, lv_img_dsc_t *out) {
    uint16_t col[NCOLORS];
    for (int i = 0; i < NCOLORS; ++i) {
        uint8_t red   = in->data[i * 3 + 0];
        uint8_t green = in->data[i * 3 + 1];
        uint8_t blue  = in->data[i * 3 + 2];
        col[i]        = lv_color_make(red, green, blue).full;
    }

    int x0 = in->data[NCOLORS * 3 + 0];
    int y0 = in->data[NCOLORS * 3 + 1];
    int w  = in->data[NCOLORS * 3 + 2];
    // int h  = in->data[NCOLORS * 3 + 3];
    int u = 0;

    uint16_t *out_data = (uint16_t *)(out->data) + y0 * out->header.w + x0;

    for (int i = NCOLORS * 3 + 4; i < in->data_size; ++i) {
        uint8_t d   = in->data[i];
        uint8_t idx = d & 0b1111;
        int     run = d >> 4;

        for (; run > 0; --run, ++u) {
            if (u >= w) {
                u -= w;
                out_data += out->header.w;
            }
            if (idx != 0) out_data[u] = col[idx];
        }
    }
}

void draw_flame(const lv_img_dsc_t *in, lv_img_dsc_t *out) {
    lv_color_t col[NCOLORS];
    uint8_t    alpha[NCOLORS];
    for (int i = 0; i < NCOLORS; ++i) {
        uint8_t idx = in->data[i * 3 + 1];
        col[i]      = lv_color_make(255, 255, 255);
        alpha[i]    = idx;
    }

    int x0 = in->data[NCOLORS * 3 + 0];
    int y0 = in->data[NCOLORS * 3 + 1];
    int w  = in->data[NCOLORS * 3 + 2];
    // int h  = in->data[NCOLORS * 3 + 3];
    int u = 0;

    lv_color_t *out_data = (lv_color_t *)(out->data) + y0 * out->header.w + x0;

    for (int i = NCOLORS * 3 + 4; i < in->data_size; ++i) {
        uint8_t d   = in->data[i];
        uint8_t idx = d & 0b1111;
        int     run = d >> 4;

        for (; run > 0; --run, ++u) {
            if (u >= w) {
                u -= w;
                out_data += out->header.w;
            }
            if (idx != 0) out_data[u] = lv_color_mix(col[idx], out_data[u], alpha[idx]);
        }
    }
}

void anim_screen_housekeep(void) {
    static bool frame_drawn = false;
    static int  frame_idx   = -1;
    if (!frame_drawn) {
        draw_stars(&custom_img_dsc);
        if (frame_idx >= 0) {
            draw_ship(ship_frame[frame_idx], &custom_img_dsc);
            draw_flame(flame_frame[frame_idx], &custom_img_dsc);
        }
        frame_drawn = true;
        return;
    } else if (timer_elapsed32(anim_timer) > 50) {
        lv_img_set_src(anim_start, &custom_img_dsc);
        anim_timer = timer_read32();
        anim_index += 1;

        const static int TABLE[] = {
            9,  15, 9,  15, 9,  15, 9,  15, 9,  15, //
            9,  15, 9,  15, 9,  15, 9,  15, 9,  15, //
            9,  15, 9,  15, 9,  15, 9,  15, 9,  15, //
            9,  15, 9,  15, 9,  15, 9,  15, 9,  15, //
            10, 11, 12,                             //
            13, 14, 13, 14, 13, 14, 13, 14, 13, 14, //
            13, 14, 13, 14, 13, 14, 13, 14, 13, 14, //
            13, 14, 13, 14, 13, 14, 13, 14, 13, 14, //
            13, 14, 13, 14, 13, 14, 13, 14, 13, 14, //
            12, 11, 10,                             //
        };

        if (anim_index >= 14)
            frame_idx = TABLE[(anim_index - 14) % ARRAY_SIZE(TABLE)];
        else if (anim_index > 5)
            frame_idx = anim_index - 5;
        else
            frame_idx = -1;

        frame_drawn = false;
    }
}

void anim_screen_load(void) {
    anim_timer = 0;
    anim_index = 0;
    srand(timer_read32());
    anim_screen_housekeep();
    lv_scr_load(screen_anim);
}

const eh_screen_t eh_screen_anim = {
    .init      = anim_screen_init,
    .load      = anim_screen_load,
    .housekeep = anim_screen_housekeep,
};
