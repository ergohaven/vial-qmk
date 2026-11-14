#include "sm62_starfield.h"

#include "timer.h"

static uint32_t next_random(sm62_starfield_t *field) {
    field->random_state = field->random_state * 1664525U + 1013904223U;
    return field->random_state;
}

static void reset_star(sm62_starfield_t *field, uint8_t index, bool initial) {
    field->x[index]     = initial ? (int16_t)(next_random(field) % 240U) : 239;
    field->y[index]     = (int16_t)(next_random(field) % 280U);
    field->speed[index] = (uint8_t)(next_random(field) % 8U) + 3U;
}

/* Match Orbit's palette, 1 px / 2x2 / five-pixel cross shapes and speed
 * thresholds, without allocating its full RGB565 framebuffer. */
static const uint32_t orbit_star_rgb[] = {
    0xFFFFFF, 0xFF9223, 0xFFF1EB, 0xE9ECFF,
    0xBFD3FF, 0xFEF9FF, 0xFF7800, 0xBACFFF,
};

static lv_area_t star_bounds(int16_t x, int16_t y, uint8_t speed) {
    lv_area_t area = {.x1 = x, .y1 = y, .x2 = x, .y2 = y};
    if (speed >= 7U && x > 0 && x < 238 && y > 0 && y < 278) {
        area.x1--; area.y1--; area.x2++; area.y2++;
    } else if (speed >= 5U && x < 239 && y < 279) {
        area.x2++; area.y2++;
    }
    return area;
}

static void draw_pixel(lv_draw_ctx_t *ctx, lv_draw_rect_dsc_t *rect, int16_t x, int16_t y) {
    lv_area_t area = {.x1 = x, .y1 = y, .x2 = x, .y2 = y};
    lv_draw_rect(ctx, rect, &area);
}

static void draw_stars(lv_event_t *event) {
    sm62_starfield_t *field = lv_event_get_user_data(event);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t origin;
    lv_obj_get_coords(field->object, &origin);
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_opa = LV_OPA_COVER;
    rect.border_opa = LV_OPA_TRANSP;
    rect.radius = 0;
    for (uint8_t i = 0; i < SM62_STAR_COUNT; ++i) {
        int16_t local_x = field->x[i];
        int16_t local_y = field->y[i];
        int16_t x = origin.x1 + local_x;
        int16_t y = origin.y1 + local_y;
        uint8_t speed = field->speed[i];
        lv_area_t bounds = star_bounds(local_x, local_y, speed);
        bounds.x1 += origin.x1; bounds.x2 += origin.x1;
        bounds.y1 += origin.y1; bounds.y2 += origin.y1;
        if (!_lv_area_is_on(&bounds, ctx->clip_area)) continue;
        rect.bg_color = lv_color_hex(orbit_star_rgb[i % (sizeof(orbit_star_rgb) / sizeof(orbit_star_rgb[0]))]);
        draw_pixel(ctx, &rect, x, y);
        if (speed >= 7U && local_x > 0 && local_x < 238 && local_y > 0 && local_y < 278) {
            draw_pixel(ctx, &rect, x - 1, y);
            draw_pixel(ctx, &rect, x + 1, y);
            draw_pixel(ctx, &rect, x, y - 1);
            draw_pixel(ctx, &rect, x, y + 1);
        } else if (speed >= 5U && local_x < 239 && local_y < 279) {
            draw_pixel(ctx, &rect, x + 1, y);
            draw_pixel(ctx, &rect, x, y + 1);
            draw_pixel(ctx, &rect, x + 1, y + 1);
        }
    }
}

void sm62_starfield_init(sm62_starfield_t *field, lv_obj_t *parent, uint32_t seed) {
    field->random_state = seed ? seed : 1U;
    field->timer = timer_read32();
    field->frame_pending = true;
    for (uint8_t i = 0; i < SM62_STAR_COUNT; ++i) reset_star(field, i, true);

    field->object = lv_obj_create(parent);
    /* LVGL stores size/position in local styles: clear the theme first. */
    lv_obj_remove_style_all(field->object);
    lv_obj_set_pos(field->object, 0, 0);
    lv_obj_set_size(field->object, 240, 280);
    lv_obj_clear_flag(field->object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(field->object, draw_stars, LV_EVENT_DRAW_MAIN, field);
}

static void offset_area(lv_area_t *area, const lv_area_t *origin) {
    area->x1 += origin->x1; area->x2 += origin->x1;
    area->y1 += origin->y1; area->y2 += origin->y1;
}

void sm62_starfield_wait_for_frame(sm62_starfield_t *field) {
    field->frame_pending = true;
}

void sm62_starfield_frame_completed(sm62_starfield_t *field) {
    field->frame_pending = false;
}

extern void sm62_diag_star(void);
void sm62_starfield_update(sm62_starfield_t *field) {
    if (field->frame_pending || timer_elapsed32(field->timer) < 50U) return;
    sm62_diag_star();
    field->frame_pending = true;
    field->timer = timer_read32();
    lv_area_t origin;
    lv_obj_get_coords(field->object, &origin);
    for (uint8_t i = 0; i < SM62_STAR_COUNT; ++i) {
        lv_area_t old = star_bounds(field->x[i], field->y[i], field->speed[i]);
        field->x[i] -= field->speed[i];
        bool wrapped = field->x[i] < 0;
        if (wrapped) reset_star(field, i, false);
        lv_area_t next = star_bounds(field->x[i], field->y[i], field->speed[i]);
        if (!wrapped) {
            /* One small dirty strip covers both endpoints and the swept path. */
            if (next.x1 < old.x1) old.x1 = next.x1;
            if (next.y1 < old.y1) old.y1 = next.y1;
            if (next.x2 > old.x2) old.x2 = next.x2;
            if (next.y2 > old.y2) old.y2 = next.y2;
        }
        offset_area(&old, &origin);
        lv_obj_invalidate_area(field->object, &old);
        if (wrapped) {
            offset_area(&next, &origin);
            lv_obj_invalidate_area(field->object, &next);
        }
    }
}
