// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "hid.h"
#include "sm62_settings.h"
#include "render_exchange.h"
/* Complete value-owned data, no core0 transient pointers. Native action
 * identity stays per physical MCU; transparent fallback and all 16 layers are
 * captured by the same core0 owner which dispatches those actions. */
typedef struct {
    hid_data_t hid;
    uint16_t native_codes[16][10][7];
    uint16_t effective_codes[16][10][7];
    char names[16][23];
    uint8_t visuals[62], states[62], matrix[10];
    sm62_side_settings_t sides[2];
    uint64_t presses, releases;
    uint32_t captured_at, activity_time, activity_idle, boot_elapsed, orbit_idle, visual_generation;
    uint16_t hold_ms;
    uint32_t timeout_ms;
    uint8_t layer, language, brightness, accent[3], background[3], command_bits, button_style;
    uint8_t last_row, last_col;
    bool left, master, live, layout_valid, time_live, volume_live;
    bool date, language_enabled, stars, screens_off, mode_pending, last_press, mac_modifiers;
} sm62_render_snapshot_t;
_Static_assert(sizeof(sm62_render_snapshot_t) <= SM62_SNAPSHOT_BYTES, "snapshot exceeds bounded exchange");

void sm62_render_capture(void); /* core0 only, nonblocking on full exchange */
void sm62_render_ready(void); /* core0 after native post-init */
void sm62_render_input(uint8_t row, uint8_t col, bool pressed); /* core0 only */
bool sm62_render_is_owner(void);
bool sm62_render_busy(void);
bool sm62_render_enabled(void);
uint8_t sm62_render_screen(void);
void sm62_render_suspend(bool off);
void sm62_render_style(uint8_t bit, uint8_t a, uint8_t b, uint8_t c);
void sm62_render_pwm(uint8_t level); /* owner publication, no HAL on core1 */
void sm62_render_frame(uint8_t screen); /* owner completion */
uint32_t sm62_render_frames(void);
void sm62_render_worker_step(void);
void sm62_keymap_owner_feedback(uint64_t presses, uint64_t releases, bool last, uint8_t row, uint8_t col);

hid_data_t *sm62_view_hid(void);
bool sm62_view_left(void);
bool sm62_view_mac_modifiers(void);
bool sm62_view_master(void);
bool sm62_view_live(void);
bool sm62_view_layout_valid(void);
bool sm62_view_time_live(void);
bool sm62_view_volume_live(void);
uint8_t sm62_view_layer(void);
uint8_t sm62_view_language(void);
bool sm62_view_matrix(uint8_t row, uint8_t col);
uint16_t sm62_view_native_code(uint8_t layer, uint8_t row, uint8_t col);
uint16_t sm62_view_effective_code(uint8_t layer, uint8_t row, uint8_t col);
const char *sm62_view_name(uint8_t layer);
bool sm62_view_visual(uint8_t layer, uint8_t control, uint8_t *visual, uint8_t *state);
uint32_t sm62_view_generation(void);
const sm62_side_settings_t *sm62_view_local_settings(void);
const sm62_side_settings_t *sm62_view_remote_settings(void);
bool sm62_view_date(void);
bool sm62_view_language_enabled(void);
bool sm62_view_stars(void);
bool sm62_view_off(void);
bool sm62_view_mode_pending(void);
uint16_t sm62_view_hold(void);
uint32_t sm62_view_orbit_idle(uint32_t idle);
uint32_t sm62_view_activity_idle(void);
uint32_t sm62_view_activity_time(void);
uint32_t sm62_view_boot(void);
uint32_t sm62_view_timeout(void);
uint8_t sm62_view_brightness(void);
uint8_t sm62_view_accent_red(void);
uint8_t sm62_view_accent_green(void);
uint8_t sm62_view_accent_blue(void);
uint8_t sm62_view_background_red(void);
uint8_t sm62_view_background_green(void);
uint8_t sm62_view_background_blue(void);
void sm62_view_backlight_init(void);
