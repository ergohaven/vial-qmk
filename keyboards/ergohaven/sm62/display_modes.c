// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboards/ergohaven/sm62/render_alias.h"
#include "display.h"
#include "hid.h"
#include "src/display/eh_display.h"
#include "src/eh_settings.h"
#include "sm62_effect_config.h"
#include "sm62_settings.h"
#include "timer.h"

extern const eh_screen_t eh_screen_clock;
extern const eh_screen_t eh_screen_player;
extern const eh_screen_t eh_screen_keymap;
extern const eh_screen_t eh_screen_orbit;
extern void sm62_main_frame_completed(void);
extern void sm62_player_frame_completed(void);
extern void sm62_orbit_frame_completed(void);
extern void sm62_orbit_restart(void);
extern void sm62_diag_frame(void);
extern void sm62_fps_frame(uint8_t screen);
extern uint32_t sm62_display_boot_elapsed(void);

#if SM62_FAST_RETURN
extern bool sm62_keymap_visual_active(void);
extern bool sm62_keymap_observe_matrix(void);
#endif

typedef enum { SM62_SCREEN_CLOCK, SM62_SCREEN_PLAYER, SM62_SCREEN_KEYMAP, SM62_SCREEN_ORBIT } sm62_screen_t;
static sm62_screen_t active_screen = SM62_SCREEN_CLOCK;
static eh_screen_t current_screen;
static bool orbit_initialized;
static bool startup_display_released;
static bool screens_were_off;
uint8_t sm62_diag_screen(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (!sm62_render_is_owner()) return sm62_render_screen();
#endif
    return screens_were_off || sm62_local_screens_off() ? 4 : (uint8_t)active_screen;
}
#if SM62_FAST_RETURN
static uint32_t local_activity_at;
static uint32_t display_ready_at;
static bool local_activity_seen;
static bool waiting_for_keymap_frame;
static uint32_t keymap_frame_at;

/* Called only after LVGL has flushed the final chunk of a frame. */
void display_frame_completed(void) {
#if defined(SM62_DUALCORE_RENDER)
    sm62_render_frame(sm62_diag_screen());
#else
    sm62_fps_frame(sm62_diag_screen());
    sm62_diag_frame();
#endif
    if (active_screen == SM62_SCREEN_CLOCK) sm62_main_frame_completed();
    if (active_screen == SM62_SCREEN_PLAYER) sm62_player_frame_completed();
    if (active_screen == SM62_SCREEN_ORBIT) sm62_orbit_frame_completed();
    if (active_screen == SM62_SCREEN_KEYMAP && waiting_for_keymap_frame) {
        keymap_frame_at = timer_read32();
        waiting_for_keymap_frame = false;
    }
}
#endif

void display_init_screens_kb(void) {
    eh_screen_clock.init();
    eh_screen_player.init();
    eh_screen_keymap.init();
    /* The orbit framebuffer is not needed on the startup screen. Allocate it
     * only when the idle orbit is first entered, after startup is complete. */
    orbit_initialized = false;
    current_screen = eh_screen_clock;
    display_load_root();
    current_screen.load();
    /* Both panels stay dark through the same pre-init-based startup window.
     * Releasing the peer at view init made it visibly precede the USB half.
     * Keep this independent of USB role and the synchronized activity clock. */
    if (sm62_local_screens_off()) display_turn_off();
    active_screen = SM62_SCREEN_CLOCK;
#if SM62_FAST_RETURN
    display_ready_at = timer_read32();
#endif
}

void display_housekeeping_task(void) {
    /* Service host data even if painter initialization failed. */
#if !defined(SM62_DUALCORE_RENDER)
    hid_housekeeping_task();
#endif
#ifdef LV_USE_COOPERATIVE_REFR
    /* Do not advance a view/frame lease while its pixels are in flight.
     * HID/split ownership still progresses on the ordinary main loop. */
    if (lv_disp_refr_pending(NULL)) return;
#endif
    bool mode_transition_pending = sm62_local_mode_transition_pending();
    if (!is_display_enabled() && !display_init_kb()) return;
    /* QMK's split activity clock includes keys on either half, not media HID.
     * Keep it separate from this half's accepted keymap/ship activity. */
    uint32_t screen_timeout = get_split_lcd_timeout_ms();
    uint32_t matrix_idle = last_matrix_activity_elapsed();
    /* Activity and clock snapshots travel independently. A fresh peer key
     * timestamp can briefly be ahead of the synchronized clock; unsigned
     * subtraction would look like ~49 days idle and pulse the panel off.
     * Clamp only bounded future skew on the peer, retaining real rollover
     * and genuinely long inactivity. Do not modify the shared QMK clock. */
    if (!is_keyboard_master() && matrix_idle > UINT32_MAX - 1000U) matrix_idle = 0;
    if (sm62_local_screens_off() || (screen_timeout && matrix_idle >= screen_timeout)) {
        screens_were_off = true;
        display_turn_off();
        return;
    }
    // Restores the saved brightness, including after a persistent night-mode off.
    if (!screens_were_off && startup_display_released) display_turn_on();

    /* A mode SET is already visible to GET. Keep the old view on-screen for
     * the short readback window, then paint the requested mode once. */
    if (mode_transition_pending) return;

    if (!startup_display_released &&
        sm62_display_boot_elapsed() >= SM62_DISPLAY_SYNC_DELAY_MS) {
        if (!screens_were_off) display_turn_on();
        startup_display_released = true;
    }

    const sm62_side_settings_t *settings = sm62_local_display_settings();
    sm62_screen_t requested;
#if SM62_FAST_RETURN
    /* The split activity timestamp can arrive late or be zero at boot.
     * Observe this half's matrix directly and retain the event through an
     * expensive LVGL draw, including a press/release between two frames. */
    bool local_change = sm62_keymap_observe_matrix();
    /* last_matrix_activity_time() is shared across both halves. It must not
     * switch this half to KEYMAP for a key pressed only on the other half. */
    if (local_change) {
        local_activity_at = timer_read32();
        local_activity_seen = true;
    }
    uint32_t idle = timer_elapsed32(local_activity_seen ? local_activity_at : display_ready_at);
    bool local_hold = local_activity_seen && idle < sm62_local_keymap_hold_ms();
    bool frame_hold = active_screen == SM62_SCREEN_KEYMAP &&
                      (waiting_for_keymap_frame || timer_elapsed32(keymap_frame_at) < 32U);
    if ((settings->keymap_enabled || settings->mode == 3) &&
        (local_change || local_hold || sm62_keymap_visual_active() || frame_hold)) {
#else
    uint32_t idle = last_matrix_activity_elapsed();
    if ((settings->keymap_enabled || settings->mode == 3) &&
        last_matrix_activity_time() != 0U && idle < sm62_local_keymap_hold_ms()) {
#endif
        requested = SM62_SCREEN_KEYMAP;
    /* Idle/ORBIT is local too: split activity from the USB half must not
     * change the other half's visible screen. */
    } else if (settings->mode == 2 ||
               (settings->ship_enabled && sm62_local_orbit_idle_ms(idle) >= (uint32_t)settings->ship_timeout_s * 1000U)) {
        requested = SM62_SCREEN_ORBIT;
    } else {
        requested = settings->mode == 1 ? SM62_SCREEN_PLAYER :
                    settings->mode == 3 ? SM62_SCREEN_KEYMAP : SM62_SCREEN_CLOCK;
    }

    if (requested != active_screen) {
        active_screen = requested;
#if SM62_FAST_RETURN
        if (active_screen == SM62_SCREEN_KEYMAP) waiting_for_keymap_frame = true;
#endif
        switch (active_screen) {
            case SM62_SCREEN_CLOCK:
                current_screen = eh_screen_clock;
                break;
            case SM62_SCREEN_PLAYER:
                current_screen = eh_screen_player;
                break;
            case SM62_SCREEN_KEYMAP:
                current_screen = eh_screen_keymap;
                break;
            case SM62_SCREEN_ORBIT:
                if (!orbit_initialized) {
                    eh_screen_orbit.init();
                    orbit_initialized = true;
                }
                /* Each idle timeout is a new animation. A permanently selected
                 * ship instead resumes after the temporary keymap view. */
                if (settings->mode != 2) sm62_orbit_restart();
                current_screen = eh_screen_orbit;
                break;
        }
        current_screen.load();
        if (screens_were_off) {
            display_refresh();
            if (startup_display_released) display_turn_on();
            screens_were_off = false;
        }
        return;
    }
    if (screens_were_off) {
        // Reload the current view without restarting the saved ship sequence;
        // its load callback releases an outstanding animation frame lease.
        current_screen.load();
        display_refresh();
        if (startup_display_released) display_turn_on();
        screens_were_off = false;
        return;
    }
    current_screen.housekeep();
}
