// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#include "player_fonts.h"
LV_FONT_DECLARE(sm62_font_zen_regular_24);
LV_FONT_DECLARE(sm62_font_zen_bold_24);
LV_FONT_DECLARE(sm62_font_cjk_24);

/* Private descriptors retain the original Latin callbacks, glyph data,
 * kerning, line height and baseline; only these two player fields fall back.
 * Initialized only on the display/main thread. Bitmap data stays const flash. */
const lv_font_t *sm62_player_title_font(void) {
    static lv_font_t font;
    if (!font.get_glyph_dsc) {
        font = sm62_font_zen_regular_24;
        font.fallback = &sm62_font_cjk_24;
    }
    return &font;
}

const lv_font_t *sm62_player_artist_font(void) {
    static lv_font_t font;
    if (!font.get_glyph_dsc) {
        font = sm62_font_zen_bold_24;
        font.fallback = &sm62_font_cjk_24;
    }
    return &font;
}
