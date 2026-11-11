#pragma once
#include "sm62_settings.h"
#include "keyboards/ergohaven/macropad/sm62_starfield.h"
#include "timer.h"

// Only clock/player use this helper. Orbit owns its separate animation.
static inline void sm62_stars_update(sm62_starfield_t *field) {
    bool hidden = lv_obj_has_flag(field->object, LV_OBJ_FLAG_HIDDEN);
    if (!sm62_local_stars_enabled()) {
        if (!hidden) lv_obj_add_flag(field->object, LV_OBJ_FLAG_HIDDEN);
        return; // no star drawing, movement or invalidation on a plain dark view
    }
    if (hidden) {
        lv_obj_clear_flag(field->object, LV_OBJ_FLAG_HIDDEN);
        field->timer = timer_read32();
        sm62_starfield_wait_for_frame(field);
    }
    sm62_starfield_update(field);
}
