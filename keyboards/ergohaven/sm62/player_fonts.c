// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#include "player_fonts.h"
LV_FONT_DECLARE(sm62_font_zen_regular_24);
LV_FONT_DECLARE(sm62_font_zen_bold_24);

/* Preserve the existing glyphs and kerning. Unsupported characters have no
 * pixels or advance, so both player fields skip them without placeholder
 * boxes, extra marquee width, or rewriting the incoming UTF-8 metadata. */
static bool player_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t letter, uint32_t next) {
    if (lv_font_get_glyph_dsc_fmt_txt(font, dsc, letter, next)) {
        return true;
    }
    *dsc = (lv_font_glyph_dsc_t){.bpp = 1};
    return true;
}

/* Only the descriptor callback changes; bitmap data stays const flash.
 * Initialized only on the display/main thread. */
const lv_font_t *sm62_player_title_font(void) {
    static lv_font_t font;
    if (!font.get_glyph_dsc) {
        font = sm62_font_zen_regular_24;
        font.get_glyph_dsc = player_glyph_dsc;
        font.fallback = NULL;
    }
    return &font;
}

const lv_font_t *sm62_player_artist_font(void) {
    static lv_font_t font;
    if (!font.get_glyph_dsc) {
        font = sm62_font_zen_bold_24;
        font.get_glyph_dsc = player_glyph_dsc;
        font.fallback = NULL;
    }
    return &font;
}
