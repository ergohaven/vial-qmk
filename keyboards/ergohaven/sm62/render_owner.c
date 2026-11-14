// SPDX-License-Identifier: GPL-2.0-or-later
#include "render_owner.h"
#include "display.h"
#include "flash_guard.h"
#include "dualcore_trace.h"
#include "keyboard.h"
#include "src/display/eh_display.h"
#include "src/eh_settings.h"
#include "src/eh_ruen.h"
#include "application_display.h"
#include "src/display/eh_keycode_str.h"
#include "timer.h"
#include "qp_lvgl.h"
#include <string.h>
#if defined(SM62_RENDER_NATIVE_TEST)
extern bool sm62_render_test_owner(void);
extern void sm62_render_test_lock(void);
extern void sm62_render_test_unlock(void);
#define OWNER() sm62_render_test_owner()
#define LOCK() sm62_render_test_lock()
#define UNLOCK() sm62_render_test_unlock()
#else
#include "ch.h"
#include "hal.h"
#include "hardware/structs/sio.h"
#define OWNER() (sio_hw->cpuid == 1U)
#define LOCK() chSysLock()
#define UNLOCK() chSysUnlock()
#endif
#define LOAD(p) __atomic_load_n((p),__ATOMIC_ACQUIRE)
#define STORE(p,v) __atomic_store_n((p),(v),__ATOMIC_RELEASE)

static sm62_render_exchange_t dc_exchange;
static sm62_render_snapshot_t *dc_view; /* core1 only, retains exchange lease */
static hid_data_t dc_hid; /* owner-private, including consumed change flags */
static uint32_t dc_ready, dc_busy, dc_enabled, dc_screen, dc_frames;
static uint32_t dc_pwm, dc_pwm_valid, dc_pwm_token;
/* Core0 publishes power independently of the bounded snapshot exchange.
 * Bit 0 is suspended; the upper bits retain even a suspend/resume pulse.
 * Only core1 writes owner_power; PWM replies belong to that observed token. */
static uint32_t dc_power_request, dc_owner_power;
static uint8_t dc_applied_pwm;
static bool dc_pwm_seen;
/* core0-only producer latches. Backpressure NEVER clears these. Both edge
 * kinds survive a press+release between frames; latest focus is local. */
static uint64_t dc_presses, dc_releases;
static uint8_t dc_last_row, dc_last_col;
static bool dc_last_press;
static uint8_t dc_commands, dc_accent[3], dc_background[3], dc_button;
static bool dc_accent_override, dc_background_override;
/* core1-only delayed feedback/style state (also retained through init retry). */
static uint64_t dc_owner_presses, dc_owner_releases;
static bool dc_owner_last;
static uint8_t dc_owner_row, dc_owner_col, dc_owner_commands;

bool sm62_render_is_owner(void) { return OWNER(); }
bool sm62_render_busy(void) { return LOAD(&dc_busy) != 0; }
bool sm62_render_enabled(void) { return LOAD(&dc_enabled) != 0; }
uint8_t sm62_render_screen(void) { return (uint8_t)LOAD(&dc_screen); }
void sm62_render_suspend(bool off) {
    uint32_t request=LOAD(&dc_power_request);
    if (off) {
        if (!(request&1U)) STORE(&dc_power_request,(request+2U)|1U);
        /* This path runs in the actual core0 suspend loop, not housekeeping.
         * No panel/LVGL access, capture, allocation or saved-setting write. */
        backlight_level_noeeprom(0);
        dc_pwm_seen=false;
    } else {
        /* Wake can be IRQ-delivered: publication only, no HAL or capture. */
        STORE(&dc_power_request,request&~1U);
    }
}
void sm62_render_style(uint8_t bit, uint8_t a, uint8_t b, uint8_t c) {
    dc_commands |= bit;
    if (bit == 1U) { dc_accent[0]=a; dc_accent[1]=b; dc_accent[2]=c; dc_accent_override=true; }
    if (bit == 2U) { dc_background[0]=a; dc_background[1]=b; dc_background[2]=c; dc_background_override=true; }
    if (bit == 4U) dc_button=a;
}
void sm62_render_pwm(uint8_t level) {
    STORE(&dc_pwm,level); STORE(&dc_pwm_token,dc_owner_power); STORE(&dc_pwm_valid,1U);
}
void sm62_view_backlight_init(void) { /* PWM HAL is initialized/owned by core0. */ }
void sm62_render_input(uint8_t row, uint8_t col, bool pressed) {
    if (row>=10 || col>=7 || (row<5)!=is_keyboard_left()) return;
    sm62_dc_trace_key();
    uint64_t bit=UINT64_C(1)<<((row%5U)*7U+col);
    if (pressed) { dc_presses|=bit; dc_last_row=row; dc_last_col=col; dc_last_press=true; }
    else dc_releases|=bit;
}
void display_process_keyevent(uint8_t row, uint8_t col, bool pressed) { sm62_render_input(row,col,pressed); }
void sm62_render_ready(void) {
    backlight_init();
    sm62_render_capture();
    STORE(&dc_ready,1U);
}
void sm62_render_capture(void) {
    uint32_t power=LOAD(&dc_power_request);
    if (!(power&1U) && LOAD(&dc_pwm_valid) && LOAD(&dc_pwm_token)==power) {
        uint8_t next=(uint8_t)LOAD(&dc_pwm);
        if (!dc_pwm_seen || next!=dc_applied_pwm) { backlight_level_noeeprom(next); dc_applied_pwm=next; dc_pwm_seen=true; }
    }
    sm62_render_snapshot_t *s=sm62_exchange_reserve(&dc_exchange);
    if (!s) return;
    memset(s,0,sizeof(*s));
    s->captured_at=timer_read32();
    /* Serial callbacks on core0 cannot preempt this short value copy. Never
     * hold this lock while fetching keymaps/fonts, rendering or parking. */
    LOCK();
    s->hid=*get_hid_data();
    s->live=is_hid_active(); s->layout_valid=is_hid_layout_valid();
    s->time_live=is_hid_time_active(); s->volume_live=is_hid_volume_active();
    s->sides[0]=*sm62_local_display_settings(); s->sides[1]=*sm62_remote_display_settings();
    s->date=sm62_local_date_enabled(); s->language_enabled=sm62_local_language_enabled();
    s->stars=sm62_local_stars_enabled(); s->screens_off=sm62_local_screens_off();
    s->hold_ms=sm62_local_keymap_hold_ms();
    s->orbit_idle=sm62_local_orbit_idle_ms(UINT32_MAX);
    UNLOCK();
    s->left=is_keyboard_left(); s->master=is_keyboard_master(); s->layer=get_current_layer(); s->language=get_cur_lang();
    s->mac_modifiers=eh_keycode_str_uses_mac_modifiers();
    s->activity_time=last_matrix_activity_time(); s->activity_idle=last_matrix_activity_elapsed();
    extern uint32_t sm62_display_boot_elapsed(void);
    s->boot_elapsed=sm62_display_boot_elapsed(); s->mode_pending=sm62_local_mode_transition_pending();
    s->brightness=get_split_lcd_brightness(); s->timeout_ms=get_split_lcd_timeout_ms();
    for (uint8_t r=0;r<10;r++) s->matrix[r]=(uint8_t)matrix_get_row(r);
    for (uint8_t l=0;l<16;l++) {
        const char *name=sm62_effective_layer_name(l);
        strncpy(s->names[l],name,sizeof(s->names[l])-1U);
        for (uint8_t r=0;r<10;r++) for (uint8_t c=0;c<7;c++) {
            s->native_codes[l][r][c]=hid_sm62_default_keycode(l,r,c);
            s->effective_codes[l][r][c]=sm62_effective_keycode(l,r,c);
        }
    }
    for (uint8_t c=0;c<62;c++) {
        if (!hid_app_layout_get_visual(s->layer,c,&s->visuals[c],&s->states[c])) s->visuals[c]=UINT8_MAX;
    }
    s->visual_generation=hid_app_layout_visual_generation();
    s->accent[0]=get_display_accent_red(); s->accent[1]=get_display_accent_green(); s->accent[2]=get_display_accent_blue();
    s->background[0]=get_display_background_red(); s->background[1]=get_display_background_green(); s->background[2]=get_display_background_blue();
    if (dc_accent_override) memcpy(s->accent,dc_accent,3);
    if (dc_background_override) memcpy(s->background,dc_background,3);
    s->command_bits=dc_commands; s->button_style=dc_button;
    s->presses=dc_presses; s->releases=dc_releases; s->last_press=dc_last_press; s->last_row=dc_last_row; s->last_col=dc_last_col;
    sm62_exchange_publish(&dc_exchange);
    dc_presses=dc_releases=0; dc_last_press=false; dc_commands=0;
}

/* Renderers may clear their PRIVATE flags. Native core0 flags/pointers never
 * cross the boundary; comparisons retain changes for an inactive view without
 * an old completion clearing a newer producer publication. */
static void dc_adopt_hid(const hid_data_t *h) {
    bool time=dc_hid.time_changed || h->hours!=dc_hid.hours || h->minutes!=dc_hid.minutes;
    bool date=dc_hid.date_changed || h->date_day!=dc_hid.date_day || h->date_month!=dc_hid.date_month || h->date_year!=dc_hid.date_year;
    bool title=dc_hid.media_title_changed || memcmp(h->media_title,dc_hid.media_title,sizeof(h->media_title));
    bool artist=dc_hid.media_artist_changed || memcmp(h->media_artist,dc_hid.media_artist,sizeof(h->media_artist));
    dc_hid=*h;
    dc_hid.time_changed=time; dc_hid.date_changed=date;
    dc_hid.media_title_changed=title; dc_hid.media_artist_changed=artist;
}

extern bool sm62_spi_quiescent(void);
extern void qp_internal_task(void);
extern bool qp_lvgl_refr_pending(void);
extern uint8_t sm62_diag_screen(void);
void sm62_render_worker_step(void) {
    /* Every QP return has synchronously stopped SPI and released DMA/mutexes;
     * verify the actual backend state, not just a guessed frame-complete bit. */
    sm62_flash_core1_poll(sm62_spi_quiescent());
    sm62_dc_trace_poll();
    uint32_t power=LOAD(&dc_power_request);
    /* A complete suspend/resume pulse before this step still performs off
     * once. Subsequent step adopts resume and reapplies normal saved policy. */
    if ((power&~1U)!=(dc_owner_power&~1U)) dc_owner_power=power|1U;
    else dc_owner_power=power;
    if (dc_owner_power&1U) {
        if (is_display_enabled()) display_turn_off();
        /* Drain an existing cooperative frame/failed-frame lease normally;
         * never mutate the view or start a fresh frame while suspended. */
        if (qp_lvgl_refr_pending()) qp_internal_task();
        if (!qp_lvgl_refr_pending()) STORE(&dc_busy,0U);
        return;
    }
    if (!LOAD(&dc_ready)) return;
    if (!dc_view || !qp_lvgl_refr_pending()) {
        sm62_render_snapshot_t *next=sm62_exchange_adopt(&dc_exchange);
        if (next) {
            dc_view=next; dc_adopt_hid(&next->hid);
            dc_owner_presses|=next->presses; dc_owner_releases|=next->releases;
            dc_owner_commands|=next->command_bits;
            if (next->last_press) { dc_owner_last=true; dc_owner_row=next->last_row; dc_owner_col=next->last_col; }
        }
    }
    if (!dc_view) return;
    if (!is_display_enabled()) {
        if (!display_init_kb()) return;
        STORE(&dc_enabled,1U);
    }
    if (!qp_lvgl_refr_pending()) {
        if (dc_owner_commands&1U) display_apply_accent_color(dc_view->accent[0],dc_view->accent[1],dc_view->accent[2]);
        if (dc_owner_commands&2U) display_apply_background_color(dc_view->background[0],dc_view->background[1],dc_view->background[2]);
        if (dc_owner_commands&4U) display_apply_button_style(dc_view->button_style);
        if (dc_owner_commands&8U) display_apply_clock_settings();
        if (dc_owner_commands&16U) display_apply_brightness();
        if (dc_owner_commands&32U) display_refresh();
        dc_owner_commands=0;
        sm62_keymap_owner_feedback(dc_owner_presses,dc_owner_releases,dc_owner_last,dc_owner_row,dc_owner_col);
        dc_owner_presses=dc_owner_releases=0; dc_owner_last=false;
        display_housekeeping_task();
    }
    STORE(&dc_busy,1U);
    qp_internal_task();
    STORE(&dc_screen,sm62_diag_screen());
    if (!qp_lvgl_refr_pending()) STORE(&dc_busy,0U);
}
void sm62_render_frame(uint8_t screen) { STORE(&dc_frames,LOAD(&dc_frames)+1U); STORE(&dc_screen,screen); }
uint32_t sm62_render_frames(void) { return LOAD(&dc_frames); }

hid_data_t *sm62_view_hid(void) { return &dc_hid; }
bool sm62_view_left(void) { return dc_view->left; }
bool sm62_view_mac_modifiers(void) { return dc_view->mac_modifiers; }
bool sm62_view_master(void) { return dc_view->master; }
bool sm62_view_live(void) { return dc_view->live; }
bool sm62_view_layout_valid(void) { return dc_view->layout_valid; }
bool sm62_view_time_live(void) { return dc_view->time_live; }
bool sm62_view_volume_live(void) { return dc_view->volume_live; }
uint8_t sm62_view_layer(void) { return dc_view->layer; }
uint8_t sm62_view_language(void) { return dc_view->language; }
bool sm62_view_matrix(uint8_t r,uint8_t c) { return r<10 && c<7 && (dc_view->matrix[r]&(1U<<c)); }
uint16_t sm62_view_native_code(uint8_t l,uint8_t r,uint8_t c) { return l<16 && r<10 && c<7 ? dc_view->native_codes[l][r][c] : 0; }
uint16_t sm62_view_effective_code(uint8_t l,uint8_t r,uint8_t c) { return l<16 && r<10 && c<7 ? dc_view->effective_codes[l][r][c] : 0; }
const char *sm62_view_name(uint8_t l) { return l<16 ? dc_view->names[l] : ""; }
bool sm62_view_visual(uint8_t l,uint8_t c,uint8_t *v,uint8_t *s) {
    if (l!=dc_view->layer || c>=62 || dc_view->visuals[c]==UINT8_MAX) return false;
    *v=dc_view->visuals[c]; *s=dc_view->states[c]; return true;
}
uint32_t sm62_view_generation(void) { return dc_view->visual_generation; }
const sm62_side_settings_t *sm62_view_local_settings(void) { return &dc_view->sides[0]; }
const sm62_side_settings_t *sm62_view_remote_settings(void) { return &dc_view->sides[1]; }
bool sm62_view_date(void) { return dc_view->date; }
bool sm62_view_language_enabled(void) { return dc_view->language_enabled; }
bool sm62_view_stars(void) { return dc_view->stars; }
bool sm62_view_off(void) { return dc_view->screens_off || (dc_owner_power&1U); }
bool sm62_view_mode_pending(void) { return dc_view->mode_pending; }
uint16_t sm62_view_hold(void) { return dc_view->hold_ms; }
uint32_t sm62_view_orbit_idle(uint32_t idle) { uint32_t bound=dc_view->orbit_idle; return idle<bound ? idle : bound; }
uint32_t sm62_view_activity_idle(void) { return dc_view->activity_idle; }
uint32_t sm62_view_activity_time(void) { return dc_view->activity_time; }
uint32_t sm62_view_boot(void) { return dc_view->boot_elapsed; }
uint32_t sm62_view_timeout(void) { return dc_view->timeout_ms; }
uint8_t sm62_view_brightness(void) { return dc_view->brightness; }
uint8_t sm62_view_accent_red(void) { return dc_view->accent[0]; }
uint8_t sm62_view_accent_green(void) { return dc_view->accent[1]; }
uint8_t sm62_view_accent_blue(void) { return dc_view->accent[2]; }
uint8_t sm62_view_background_red(void) { return dc_view->background[0]; }
uint8_t sm62_view_background_green(void) { return dc_view->background[1]; }
uint8_t sm62_view_background_blue(void) { return dc_view->background[2]; }

#if !defined(SM62_RENDER_NATIVE_TEST)
/* The supported ChibiOS second-instance flow, not an independent Pico runtime.
 * Render working area is explicit MAIN RAM, not the 2K bootstrap process stack. */
static THD_WORKING_AREA(dc_render_wa, 6144);
static THD_FUNCTION(dc_render_thread, arg) {
    (void)arg;
    while (true) {
        sm62_render_worker_step();
        if (!qp_lvgl_refr_pending()) chThdSleepMilliseconds(1);
    }
}
void c1_main(void) {
    chSysWaitSystemState(ch_sys_running);
    chInstanceObjectInit(&ch1,&ch_core1_cfg);
    chSysUnlock();
    chThdCreateStatic(dc_render_wa,sizeof(dc_render_wa),NORMALPRIO,dc_render_thread,NULL);
    while (true) chThdSleepMilliseconds(1000);
}
#endif
