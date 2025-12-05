/*******************************************************************************
 * Size: 20 px
 * Bpp: 1
 * Opts: --bpp 1 --size 20 --no-compress --stride 1 --align 1 --font MaterialIcons-Regular.ttf --range 60135,60136,60134 --font fa-solid-900.ttf --range 57699,61549,61956,61957 --format lvgl -o custom_icons_20.c
 ******************************************************************************/

#include "qp_lvgl.h"
#ifndef CUSTOM_ICONS_20
#define CUSTOM_ICONS_20 1
#endif

#if CUSTOM_ICONS_20

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+E163 "" */
    0x7f, 0xff, 0xfb, 0xff, 0xff, 0xfe, 0x0, 0x0,
    0xf8, 0x0, 0x3, 0xe0, 0x0, 0xf, 0x80, 0x0,
    0x3e, 0x0, 0x0, 0xf8, 0x0, 0x3, 0xe0, 0x0,
    0xf, 0x80, 0x0, 0x3e, 0x0, 0x0, 0xf8, 0x0,
    0x3, 0xe0, 0x0, 0xf, 0x80, 0x0, 0x3f, 0xff,
    0xff, 0xdf, 0xff, 0xfe, 0x0, 0x7c, 0x0, 0x1f,
    0xff, 0x0, 0x7f, 0xfc, 0x0,

    /* U+EAE6 "" */
    0x0, 0x0, 0x60, 0xf, 0x1, 0x98, 0x30, 0xc6,
    0x6, 0x40, 0x20,

    /* U+EAE7 "" */
    0x78, 0x3d, 0x98, 0xcf, 0x31, 0x9e, 0x63, 0x37,
    0xff, 0xc1, 0x8c, 0x3, 0x18, 0x6, 0x30, 0xc,
    0x60, 0xff, 0xfb, 0x31, 0x9e, 0x63, 0x3c, 0xc6,
    0x6f, 0x7, 0x80,

    /* U+EAE8 "" */
    0xf8, 0x3e, 0x18, 0x7c, 0x10, 0x0, 0x30, 0x0,
    0x30, 0x0, 0x20, 0x0, 0x60, 0x0, 0x40, 0x0,
    0xc0, 0x0, 0x80, 0x0, 0xf8,

    /* U+F06D "" */
    0x7, 0x0, 0x7, 0xc0, 0x3, 0xe6, 0x3, 0xff,
    0x83, 0xff, 0xe1, 0xff, 0xf1, 0xff, 0xfc, 0xff,
    0xfe, 0xf9, 0xff, 0xfc, 0x7f, 0xfc, 0x1b, 0xfe,
    0x0, 0xff, 0x0, 0x7f, 0x80, 0x7d, 0xe0, 0x3c,
    0xf8, 0x3e, 0x3f, 0xfe, 0xf, 0xfe, 0x3, 0xfe,
    0x0, 0x7c, 0x0,

    /* U+F204 "" */
    0xf, 0xff, 0x80, 0x7f, 0xff, 0x83, 0x80, 0xf,
    0x18, 0x0, 0xe, 0x67, 0x80, 0x3b, 0x3f, 0x0,
    0x7d, 0xfe, 0x1, 0xf7, 0xf8, 0x7, 0xdf, 0xe0,
    0x1f, 0x3f, 0x0, 0x76, 0x78, 0x3, 0x98, 0x0,
    0xe, 0x38, 0x0, 0xf0, 0x7f, 0xff, 0x80, 0xff,
    0xfc, 0x0,

    /* U+F205 "" */
    0xf, 0xff, 0x80, 0x7f, 0xff, 0x83, 0xff, 0xff,
    0x1f, 0xfe, 0x3e, 0x7f, 0xf0, 0x7b, 0xff, 0x80,
    0xff, 0xfe, 0x3, 0xff, 0xf8, 0xf, 0xff, 0xe0,
    0x3f, 0xff, 0x80, 0xf7, 0xff, 0x7, 0x9f, 0xfe,
    0x3e, 0x3f, 0xff, 0xf0, 0x7f, 0xff, 0x80, 0xff,
    0xfc, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 360, .box_w = 22, .box_h = 19, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 53, .adv_w = 320, .box_w = 12, .box_h = 7, .ofs_x = 4, .ofs_y = 9},
    {.bitmap_index = 64, .adv_w = 320, .box_w = 15, .box_h = 14, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 91, .adv_w = 320, .box_w = 15, .box_h = 11, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 112, .adv_w = 280, .box_w = 17, .box_h = 20, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 155, .adv_w = 360, .box_w = 22, .box_h = 15, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 360, .box_w = 22, .box_h = 15, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x983, 0x984, 0x985, 0xf0a, 0x10a1, 0x10a2
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 57699, .range_length = 4259, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 7, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t ergohaven_symbols_20;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t custom_icons_20 = {
#else
lv_font_t custom_icons_20 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 20,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -5,
    .underline_thickness = 2,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &ergohaven_symbols_20,
#endif
    .user_data = NULL,
};



#endif /*#if CUSTOM_ICONS_20*/
