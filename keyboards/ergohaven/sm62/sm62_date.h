// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <lvgl.h>
#include <string.h>
#include "hid.h"
#include "sm62_settings.h"

/* One display view is active at a time. force is true whenever that view is
 * loaded, so a date received on another view is still painted immediately. */
static inline void sm62_date_update(lv_obj_t *label, bool force) {
    static bool displayed_live;
    hid_data_t *hid = get_hid_data();
    bool live = is_hid_active() && hid->date_day >= 1 && hid->date_day <= 31 &&
                hid->date_month >= 1 && hid->date_month <= 12 && hid->date_year >= 2000;
    bool show = sm62_local_date_enabled();
    bool hidden = lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN);
    if (!show || !live) {
        if (!hidden) lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        displayed_live = false;
        return;
    }
    if (hidden) lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    if (force || hidden || !displayed_live || hid->date_changed) {
        lv_label_set_text_fmt(label, "%02u.%02u.%04u",
                              hid->date_day, hid->date_month, hid->date_year);
    }
    displayed_live = true;
    hid->date_changed = false;
}
