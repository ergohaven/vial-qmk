// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#if defined(SM62_DUALCORE_RENDER)
/* Include public native declarations BEFORE applying aliases. This header is
 * only for render translation units/inline helpers, NEVER core0 producers. */
#include "keyboard.h"
#include "src/display/eh_display.h"
#include "src/eh_settings.h"
#include "src/eh_ruen.h"
#include "keyboards/ergohaven/sm62/application_display.h"
#include "keyboards/ergohaven/sm62/render_owner.h"
#define get_hid_data sm62_view_hid
#define is_keyboard_left sm62_view_left
#define is_keyboard_master sm62_view_master
#define is_hid_active sm62_view_live
#define is_hid_layout_valid sm62_view_layout_valid
#define is_hid_time_active sm62_view_time_live
#define is_hid_volume_active sm62_view_volume_live
#define get_current_layer sm62_view_layer
#define get_cur_lang sm62_view_language
#define matrix_is_on sm62_view_matrix
#define hid_sm62_default_keycode sm62_view_native_code
#define sm62_effective_keycode sm62_view_effective_code
#define sm62_effective_layer_name sm62_view_name
#define layer_name sm62_view_name
#define hid_app_layout_get_visual sm62_view_visual
#define hid_app_layout_visual_generation sm62_view_generation
#define sm62_local_display_settings sm62_view_local_settings
#define sm62_remote_display_settings sm62_view_remote_settings
#define sm62_local_date_enabled sm62_view_date
#define sm62_local_language_enabled sm62_view_language_enabled
#define sm62_local_stars_enabled sm62_view_stars
#define sm62_local_screens_off sm62_view_off
#define sm62_local_mode_transition_pending sm62_view_mode_pending
#define sm62_local_keymap_hold_ms sm62_view_hold
#define sm62_local_orbit_idle_ms sm62_view_orbit_idle
#define last_matrix_activity_elapsed sm62_view_activity_idle
#define last_matrix_activity_time sm62_view_activity_time
#define sm62_display_boot_elapsed sm62_view_boot
#define get_split_lcd_timeout_ms sm62_view_timeout
#define get_split_lcd_brightness sm62_view_brightness
#define get_display_accent_red sm62_view_accent_red
#define get_display_accent_green sm62_view_accent_green
#define get_display_accent_blue sm62_view_accent_blue
#define get_display_background_red sm62_view_background_red
#define get_display_background_green sm62_view_background_green
#define get_display_background_blue sm62_view_background_blue
#define backlight_init sm62_view_backlight_init
#define backlight_level_noeeprom sm62_render_pwm
#endif
