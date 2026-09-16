#pragma once
#include <lvgl.h>

#define EH_DISPLAY_CONTENT_INSET 5
#define EH_DISPLAY_CONTENT_WIDTH (240 - 2 * EH_DISPLAY_CONTENT_INSET)
#define EH_DISPLAY_CONTENT_HEIGHT (280 - 2 * EH_DISPLAY_CONTENT_INSET)

/* A real smaller layout area, not a full-size canvas hidden behind a clip.
 * Foreground coordinates are relative to this inset; backgrounds stay on root. */
static inline lv_obj_t *eh_display_safe_content(lv_obj_t *screen) {
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_pos(content, EH_DISPLAY_CONTENT_INSET, EH_DISPLAY_CONTENT_INSET);
    lv_obj_set_size(content, EH_DISPLAY_CONTENT_WIDTH, EH_DISPLAY_CONTENT_HEIGHT);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    return content;
}
