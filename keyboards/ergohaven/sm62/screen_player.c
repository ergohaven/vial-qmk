// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboards/ergohaven/sm62/render_alias.h"
#include "src/display/eh_display.h"
#include <lvgl.h>
#include "application_display.h"
#include "hid.h"
#include "ergohaven.h"
#include "src/eh_settings.h"
#include <string.h>
#include "src/eh_ruen.h"
#include "sm62_effect_config.h"
#include "sm62_settings.h"
#include "keyboards/ergohaven/macropad/sm62_starfield.h"
#include "sm62_starfield_control.h"
#include "timer.h"
#include "player_fonts.h"

LV_FONT_DECLARE(sm62_font_zen_medium_12);
LV_FONT_DECLARE(sm62_font_zen_medium_16);
LV_FONT_DECLARE(sm62_font_zen_regular_20);
LV_FONT_DECLARE(sm62_font_zen_medium_20);
LV_FONT_DECLARE(sm62_font_zen_regular_24);
LV_FONT_DECLARE(sm62_font_zen_bold_24);

static lv_obj_t *screen_right_player;
static lv_obj_t *label_title;
static lv_obj_t *label_artist;
static lv_obj_t *label_language;
static lv_obj_t *label_layer_number;
static lv_obj_t *label_layer_name;
static lv_obj_t *label_progress_fill;
static lv_obj_t *label_track_time;
static uint8_t displayed_layer = UINT8_MAX;
static char displayed_layer_name[32];
static sm62_starfield_t stars;
static bool force_update_pending;
static uint8_t displayed_language = UINT8_MAX;

#define TRACK_LABEL_X             27
#define TRACK_LABEL_Y             117
#define TRACK_LABEL_WIDTH         195
#define TRACK_LABEL_HEIGHT        32
#define TRACK_LABEL_LEFT_PAD      10
#define TRACK_TEXT_AVAILABLE      (TRACK_LABEL_WIDTH - TRACK_LABEL_LEFT_PAD)
#define TRACK_ACCENT_Y            121
#define TRACK_ACCENT_HEIGHT       17

static lv_obj_t *make_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, uint32_t color) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static lv_obj_t *make_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, const lv_font_t *font, uint32_t color, lv_text_align_t align, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    return label;
}

static void make_title_fade(lv_obj_t *parent) {
    /* Five narrow masks make the marquee dissolve into the orange marker. */
    static const lv_opa_t opacity[] = {LV_OPA_COVER, 210, 155, 95, 40};
    for (uint8_t i = 0; i < ARRAY_SIZE(opacity); ++i) {
        lv_obj_t *mask = make_box(parent, TRACK_LABEL_X + (i * 2), TRACK_LABEL_Y, 2, TRACK_LABEL_HEIGHT, 0x070705);
        lv_obj_set_style_bg_opa(mask, opacity[i], 0);
    }
}

static void track_scroll_x(void *label, int32_t x) {
    lv_obj_set_x(label, (lv_coord_t)x);
}

static void set_track_text(const char *text) {
    static bool text_initialized;
    static size_t displayed_text_length;
    size_t text_length = strlen(text);
    /* HID refreshes and forced view loads are not track changes. The label
     * owns the source bytes (followed by the circular copy for long titles),
     * so compare the original byte span without a truncated second buffer. */
    if (text_initialized && text_length == displayed_text_length &&
        strncmp(lv_label_get_text(label_title), text, text_length) == 0) {
        return;
    }
    text_initialized = true;
    displayed_text_length = text_length;

    lv_point_t text_size;
    lv_txt_get_size(&text_size, text, sm62_player_title_font(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    lv_anim_del(label_title, track_scroll_x);
    lv_obj_set_x(label_title, TRACK_LABEL_LEFT_PAD);
    if (text_size.x <= TRACK_TEXT_AVAILABLE) {
        lv_label_set_text(label_title, text);
        lv_obj_set_width(label_title, TRACK_TEXT_AVAILABLE);
        return;
    }

    /* Move the label inside an unpadded viewport. LVGL's built-in marquee
     * clips to the label's padded content area, so pad_left=10 discarded
     * every glyph before it could reach the fade masks at x27..36.
     * Repeating the text gives the same seamless 3-space circular gap,
     * without moving the initial text origin or the viewport's right edge. */
    lv_label_set_text_fmt(label_title, "%s   %s", text, text);
    lv_obj_set_width(label_title, LV_SIZE_CONTENT);
    lv_coord_t period = text_size.x + 3 * lv_font_get_glyph_width(sm62_player_title_font(), ' ', ' ');
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, label_title);
    lv_anim_set_exec_cb(&animation, track_scroll_x);
    lv_anim_set_values(&animation, TRACK_LABEL_LEFT_PAD, TRACK_LABEL_LEFT_PAD - period);
    lv_anim_set_time(&animation, lv_anim_speed_to_time(18, 0, period));
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation);
}

static void screen_right_player_init(void) {
    screen_right_player = display_view_create();
    lv_obj_set_style_pad_all(screen_right_player, 0, 0);
    lv_obj_set_style_border_width(screen_right_player, 0, 0);
    lv_obj_set_style_bg_color(screen_right_player, lv_color_hex(0x070705), 0);
    lv_obj_set_style_bg_opa(screen_right_player, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_right_player, LV_OBJ_FLAG_SCROLLABLE);

    sm62_starfield_init(&stars, screen_right_player, 0x0239A62U);
    sm62_stars_update(&stars);

    /* Frame 72: restore the v48 centred header grid. */
    label_layer_number = make_label(screen_right_player, 40, 17, 160, 21, &sm62_font_zen_regular_20, 0xC1C1C0, LV_TEXT_ALIGN_CENTER, "LAYER / 0");
    make_box(screen_right_player, 72, 41, 96, 3, 0xF2DB95);
    label_layer_name = make_label(screen_right_player, 40, 49, 160, 21, &sm62_font_zen_medium_20, 0xFFF8E7, LV_TEXT_ALIGN_CENTER, "MAIN");

    /* Match the accent marker to the visible glyph area, not the 32 px
     * clipping container. 22 px covers the Regular 24 glyph extents,
     * including descenders, while retaining vertical breathing room. */
    make_box(screen_right_player, 20, TRACK_ACCENT_Y, 5, TRACK_ACCENT_HEIGHT, 0xF2DB95);
    lv_obj_t *title_viewport = lv_obj_create(screen_right_player);
    lv_obj_remove_style_all(title_viewport);
    lv_obj_set_pos(title_viewport, TRACK_LABEL_X, TRACK_LABEL_Y);
    lv_obj_set_size(title_viewport, TRACK_LABEL_WIDTH, TRACK_LABEL_HEIGHT);
    lv_obj_clear_flag(title_viewport, LV_OBJ_FLAG_SCROLLABLE);
    label_title = make_label(title_viewport, TRACK_LABEL_LEFT_PAD, 0, TRACK_TEXT_AVAILABLE, TRACK_LABEL_HEIGHT, sm62_player_title_font(), 0xFFF8E7, LV_TEXT_ALIGN_LEFT, "NO SIGNAL");
    set_track_text("NO SIGNAL");
    make_title_fade(screen_right_player);

    label_artist = make_label(screen_right_player, 20, 149, 200, 32, sm62_player_artist_font(), 0xFFF8E7, LV_TEXT_ALIGN_LEFT, "Waiting for host");

    make_box(screen_right_player, 20, 193, 200, 5, 0x837F76);
    label_progress_fill = make_box(screen_right_player, 20, 193, 0, 5, 0xFFF8E7);
    label_track_time = make_label(screen_right_player, 20, 202, 200, 23, &sm62_font_zen_medium_20, 0x929291, LV_TEXT_ALIGN_RIGHT, "--:-- / --:--");

    label_language = make_label(screen_right_player, 180, 243, 40, 21, &sm62_font_zen_medium_20, 0xFFF8E7, LV_TEXT_ALIGN_RIGHT, "EN");
    lv_obj_add_flag(label_language, LV_OBJ_FLAG_HIDDEN);
}

#define MEDIA_EMPTY_GRACE_MS 1500U

typedef struct {
    bool displayed;
    bool pending;
    uint32_t empty_since;
} media_empty_grace_t;

/* Empty metadata is also the companion's shutdown signal. Keep a previously
 * displayed value briefly, without restarting its deadline on repeated empty
 * reports or forced view loads. Title and artist arrive independently. */
static bool media_text_ready(media_empty_grace_t *state, const char *text) {
    if (text[0]) {
        state->displayed = true;
        state->pending = false;
        return true;
    }
    if (!state->displayed) return true;
    if (!state->pending) {
        state->empty_since = timer_read32();
        state->pending = true;
    }
    if (timer_elapsed32(state->empty_since) < MEDIA_EMPTY_GRACE_MS) return false;
    state->displayed = false;
    state->pending = false;
    return true;
}

static void update_player(bool force) {
    static bool previous_active;
    static media_empty_grace_t title_grace, artist_grace;
    hid_data_t *hid = get_hid_data();
    bool active = is_hid_active();

    if (!active) {
        if (force || previous_active || hid->media_title_changed || hid->media_artist_changed) {
            set_track_text("NO SIGNAL");
            lv_label_set_text_static(label_artist, "Waiting for host");
        }
        title_grace = (media_empty_grace_t){0};
        artist_grace = (media_empty_grace_t){0};
    } else {
        /* Pending clears must be drained by housekeeping even if no further
         * changed flag arrives. Unsigned timer_elapsed32 also handles wrap. */
        if ((force || hid->media_title_changed || title_grace.pending) &&
            media_text_ready(&title_grace, hid->media_title)) {
            set_track_text(hid->media_title[0] ? hid->media_title : "NO SIGNAL");
        }
        if ((force || hid->media_artist_changed || artist_grace.pending) &&
            media_text_ready(&artist_grace, hid->media_artist)) {
            lv_label_set_text(label_artist, hid->media_artist[0] ? hid->media_artist : "Waiting for host");
        }
    }
    previous_active = active;
    hid->media_title_changed = false;
    hid->media_artist_changed = false;
}

static void update_layer(bool force) {
    uint8_t layer = get_current_layer();
    const char *name = sm62_effective_layer_name(layer);
    if (!force && layer == displayed_layer && strcmp(name, displayed_layer_name) == 0) return;
    displayed_layer = layer;
    lv_label_set_text_fmt(label_layer_number, "LAYER / %u", (unsigned)layer);
    lv_label_set_text(label_layer_name, name);
    strncpy(displayed_layer_name, name, sizeof(displayed_layer_name) - 1U);
    displayed_layer_name[sizeof(displayed_layer_name) - 1U] = '\0';
}

static void update_progress(bool force) {
    static bool shown;
    static uint32_t previous_position = UINT32_MAX;
    static uint32_t previous_duration = UINT32_MAX;
    hid_data_t *hid = get_hid_data();
    bool valid = is_hid_active() && hid->media_progress_valid &&
                 hid->media_duration_seconds > 0 &&
                 timer_elapsed32(hid->media_progress_time) < 10000U;
    uint32_t position = valid ? hid->media_position_seconds : 0;
    uint32_t duration = valid ? hid->media_duration_seconds : 0;
    if (position > duration) position = duration;
    if (!force && shown == valid && position == previous_position && duration == previous_duration) return;
    shown = valid;
    previous_position = position;
    previous_duration = duration;
    if (valid) {
        lv_obj_set_width(label_progress_fill, (lv_coord_t)(((uint64_t)position * 200U) / duration));
        lv_label_set_text_fmt(label_track_time, "%02lu:%02lu / %02lu:%02lu",
                              (unsigned long)(position / 60U), (unsigned long)(position % 60U),
                              (unsigned long)(duration / 60U), (unsigned long)(duration % 60U));
    } else {
        lv_obj_set_width(label_progress_fill, 0);
        lv_label_set_text_static(label_track_time, "--:-- / --:--");
    }
}

static void update_language(bool force) {
    /* Check validity before the language cache: expiry/re-sync may leave the
     * same language value and must still change visibility. */
    if (!sm62_local_language_enabled() || !is_hid_layout_valid()) {
        lv_obj_add_flag(label_language, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(label_language, LV_OBJ_FLAG_HIDDEN);
    uint8_t language = get_cur_lang();
    if (!force && language == displayed_language) {
        return;
    }
    displayed_language = language;
    lv_label_set_text_static(label_language, language == LANG_EN ? "EN" : "RU");
}

static void screen_right_player_load(void) {
    stars.timer = timer_read32();
    sm62_starfield_wait_for_frame(&stars);
    sm62_stars_update(&stars);
    display_show_view(screen_right_player);
    update_layer(true);
    update_language(true);
    update_progress(true);
    force_update_pending = true;
}

void sm62_player_frame_completed(void) {
    sm62_starfield_frame_completed(&stars);
}

static void screen_right_player_housekeep(void) {
    static uint32_t update_timer;
    sm62_stars_update(&stars);
    update_layer(false);
    update_language(false);
    if (timer_elapsed32(update_timer) < 100U) {
        return;
    }
    update_timer = timer_read32();
    update_player(force_update_pending);
    update_progress(force_update_pending);
    force_update_pending = false;
}

const eh_screen_t eh_screen_player = {
    .init      = screen_right_player_init,
    .load      = screen_right_player_load,
    .housekeep = screen_right_player_housekeep,
};
