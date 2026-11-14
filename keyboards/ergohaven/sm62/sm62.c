// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include "display.h"
#include "sm62_effect_config.h"
#include "sm62_side.h"
#include "src/display/eh_display.h"
#include "timer.h"
#include "raw_hid.h"
#include "src/eh_settings.h"
#include "sm62_settings.h"
#include "eeconfig.h"
#include "dynamic_keymap.h"
#include "keymap_introspection.h"
#include "transactions.h"
#include "hid.h"
#include "sm62_trace.h"
#if defined(SM62_DUALCORE_RENDER)
#    include "render_owner.h"
#    include "dualcore_trace.h"
#endif

void kb_settings_layer_labels_init(void);
#include <string.h>
#include "fps_bridge.inc"
#if defined(SM62_STAGE_TRACE) && !defined(SM62_DUALCORE_RENDER)
#include "stage_trace.inc"
#endif

/* Physical handedness uses QMK's SPLIT_HAND_PIN implementation (GP29).
 * Do not override is_keyboard_left_impl: the same image serves both halves. */

const char *default_layer_label(uint8_t layer) {
    static const char labels[][16] = {"MAIN", "NAV", "RAISE", "ADJST", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVN", "TWLVE", "THRTN", "FRTN", "FIFTN"};
    return layer < ARRAY_SIZE(labels) ? labels[layer] : "";
}

/* Cold-start trace: fixed RAM, no USB output or allocation during capture. */
#define DIAG_BINS 16
#define DIAG_WINDOW_MS 250U
typedef struct {
    uint16_t handlers, handler_ms, handler_max;
    uint16_t flushes, flush_ms, flush_max;
    uint16_t frames, star_steps, house_max, loop_gap_max;
} diag_bin_t;
static diag_bin_t diag_bins[DIAG_BINS];
static uint32_t diag_epoch, diag_last_house;
static uint16_t diag_stages[5];
static uint32_t diag_absolute[6]; /* pre-init, post-init, painter, LVGL, screens, first frame */
static bool diag_started;

static diag_bin_t *diag_current(void) {
    if (!diag_started) return NULL;
    uint32_t age = timer_elapsed32(diag_epoch);
    if (age >= DIAG_BINS * DIAG_WINDOW_MS) return NULL;
    return &diag_bins[age / DIAG_WINDOW_MS];
}
static uint16_t diag_clip(uint32_t v) { return v > 65535U ? 65535U : (uint16_t)v; }
static void diag_add(uint16_t *dst, uint32_t v) { *dst = diag_clip((uint32_t)*dst + v); }
static void diag_max(uint16_t *dst, uint32_t v) { if (v > *dst) *dst = diag_clip(v); }
void sm62_diag_stage(uint8_t stage) {
#if defined(SM62_DUALCORE_RENDER)
    if (sm62_render_is_owner()) return;
#endif
    if (stage < 5 && diag_started) {
        uint32_t now = timer_read32();
        diag_stages[stage] = diag_clip(now - diag_epoch);
        diag_absolute[stage + 1] = now;
    }
}
void sm62_diag_handler(uint32_t start) {
#if defined(SM62_DUALCORE_RENDER)
    if (sm62_render_is_owner()) return;
#endif
    diag_bin_t *b = diag_current(); if (!b) return;
    uint32_t ms = timer_elapsed32(start);
    diag_add(&b->handlers, 1); diag_add(&b->handler_ms, ms); diag_max(&b->handler_max, ms);
}
void sm62_diag_flush(uint32_t start) {
#if defined(SM62_DUALCORE_RENDER)
    if (sm62_render_is_owner()) return;
#endif
    diag_bin_t *b = diag_current(); if (!b) return;
    uint32_t ms = timer_elapsed32(start);
    diag_add(&b->flushes, 1); diag_add(&b->flush_ms, ms); diag_max(&b->flush_max, ms);
}
void sm62_diag_frame(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (sm62_render_is_owner()) return;
#endif
    diag_bin_t *b=diag_current(); if (b) diag_add(&b->frames,1); if (!diag_stages[4]) sm62_diag_stage(4);
}
void sm62_diag_star(void) {
#if defined(SM62_DUALCORE_RENDER)
    if (sm62_render_is_owner()) return;
#endif
    diag_bin_t *b=diag_current(); if (b) diag_add(&b->star_steps,1);
}
void sm62_diag_house(uint32_t start) {
    diag_bin_t *b=diag_current(); if (!b) return;
    diag_max(&b->house_max,timer_elapsed32(start));
    if (diag_last_house) diag_max(&b->loop_gap_max,timer_elapsed32(diag_last_house));
    diag_last_house=timer_read32();
}
static void diag_put16(uint8_t *p, uint16_t v) { p[0]=v&255; p[1]=v>>8; }
static void diag_put32(uint8_t *p, uint32_t v) {
    for (uint8_t i=0; i<4; ++i) p[i]=(uint8_t)(v >> (8*i));
}
/* Diagnostic RPC is registered at boot, but only executed on host request. */
static void sm62_diag_rpc(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
#if defined(SM62_DUALCORE_RENDER)
    if (out && out_len>=8) { memset(out,0,out_len); ((uint8_t *)out)[0]=0xD7; ((uint8_t *)out)[1]=0x62; ((uint8_t *)out)[2]=0x18; ((uint8_t *)out)[7]=0xFE; }
    return; /* old peer/master-only statistics are not dual-core measurements */
#endif
    if (in_len) { (void)sm62_fps_rpc(in_len,in,out_len,out); return; }
    (void)in_len; (void)in;
    if (out_len != 32 || !out) return;
    uint8_t *p=out;
    memset(p,0,32);
    p[0]=0xD7; p[1]=0x62; p[2]=2; p[3]=sm62_side_id();
    p[4]=is_keyboard_master() ? 1 : 2;
    p[5]=diag_started ? 1 : 0;
    for (uint8_t i=0;i<6;i++) diag_put32(p+6+4*i,diag_absolute[i]);
    diag_put16(p+30,diag_clip(timer_read32()));
}
bool sm62_diag_raw(uint8_t *data, uint8_t length) {
#if defined(SM62_DUALCORE_RENDER)
    return sm62_dc_diag_raw(data,length);
#endif
#if defined(SM62_STAGE_TRACE) && !defined(SM62_DUALCORE_RENDER)
    if(sm62_stage_raw(data,length)) return true;
#endif
    if (sm62_fps_raw(data,length)) return true;
    if (length < 4 || data[0] != 0xD7 || data[1] != 0x62 || data[2] != 1) return false;
    uint8_t index=data[3], reply[32]={0};
    reply[0]=0xD7; reply[1]=0x62; reply[2]=1; reply[3]=index;
    reply[4]=sm62_side_id(); reply[5]=DIAG_BINS;
    if (index==0) {
        for (uint8_t i=0;i<5;i++) diag_put16(reply+6+2*i,diag_stages[i]);
        reply[16]=diag_started;
    } else if (index<=DIAG_BINS) {
        const diag_bin_t *b=&diag_bins[index-1];
        const uint16_t *v=(const uint16_t *)b;
        for (uint8_t i=0;i<10;i++) diag_put16(reply+6+2*i,v[i]);
    } else if (index>=17 && index<=20) {
        uint8_t payload[32]={0};
        bool ok=true;
        if (index==18 || index==20) ok=is_keyboard_master() && transaction_rpc_recv(RPC_SM62_DIAG,32,payload);
        else sm62_diag_rpc(0,NULL,32,payload);
        reply[4]=ok ? 1 : 0;
        if (ok) {
            if (index<=18) memcpy(reply+5,payload,27); /* identity and stages pre-init..screens */
            else { memcpy(reply+5,payload,6); memcpy(reply+11,payload+26,6); } /* first frame and sample uptime */
        }
    } else reply[2]=0xFF;
    raw_hid_send(reply,sizeof(reply));
    return true;
}

void sm62_diag_house(uint32_t start);
void sm62_diag_stage(uint8_t stage);
void housekeeping_task_user(void) {
    sm62_trace_count(13);
    sm62_trace_edge(7,true);
    sm62_fps_tick();
    uint32_t start=timer_read32();
    sm62_trace_edge(9,true);
    sm62_settings_sync_task();
    sm62_trace_edge(9,false);
    uint32_t app_start=timer_read32();
    sm62_trace_edge(8,true);
    hid_app_layout_split_task();
    sm62_trace_edge(8,false);
    fps_duration(&fps_data,app_start,timer_read32(),24);
#if defined(SM62_DUALCORE_RENDER)
    hid_housekeeping_task();
    sm62_render_capture();
#else
    display_housekeeping_task();
#endif
    sm62_diag_house(start);
    sm62_trace_edge(7,false);
}

/* Compatible upgrades only reload native names. The historical heuristic
 * migration remains available solely outside the preservation policy. */
static void sm62_migrate_factory_defaults(void) {
#ifdef SM62_PRESERVE_LEGACY_DEFAULT
    /* Legacy-looking values can be intentional user settings. Compatible
     * upgrades must not guess their provenance or write EEPROM at boot.
     * Fresh/reset initialization already installs current factory defaults.
     * Keep the native label cache populated without migrating stored bytes. */
    kb_settings_layer_labels_init();
    layer_name_updated = true;
#else
    char old_label[LAYER_LABEL_SIZE];
    for (uint8_t layer = 2; layer < 16; ++layer) {
        snprintf(old_label, sizeof(old_label), "LAYER %u", layer);
        if (strcmp(layer_name(layer), old_label) != 0) continue;
        const char *new_label = default_layer_label(layer);
        eeconfig_update_kb_datablock(new_label, KB_SETTINGS_LAYER_LABELS_OFFSET + layer * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
    }
    kb_settings_layer_labels_init();
    layer_name_updated = true;

    /* Older builds placed the right outer switches at rows 6-8 although
     * the PCB reports rows 5-7. Move stored mappings for all layers only
     * when the old base-layer signature is present. The empty old row 8
     * then makes the migration idempotent, without resetting other keys. */
    if (is_keyboard_master() &&
        dynamic_keymap_get_keycode(0, 5, 6) == KC_NO &&
        dynamic_keymap_get_keycode(0, 6, 6) == KC_EQL &&
        dynamic_keymap_get_keycode(0, 7, 6) == KC_QUOT &&
        dynamic_keymap_get_keycode(0, 8, 6) == KC_BSLS) {
        for (uint8_t layer = 0; layer < DYNAMIC_KEYMAP_LAYER_COUNT; ++layer) {
            uint16_t top = dynamic_keymap_get_keycode(layer, 6, 6);
            uint16_t middle = dynamic_keymap_get_keycode(layer, 7, 6);
            uint16_t bottom = dynamic_keymap_get_keycode(layer, 8, 6);
            dynamic_keymap_set_keycode(layer, 5, 6, top);
            dynamic_keymap_set_keycode(layer, 6, 6, middle);
            dynamic_keymap_set_keycode(layer, 7, 6, bottom);
            dynamic_keymap_set_keycode(layer, 8, 6, KC_NO);
        }
    }


    /* Match the physical legends on an existing factory base layer.
     * Leave custom bindings and other layers untouched. */
    if (is_keyboard_master() &&
        dynamic_keymap_get_keycode(0, 7, 5) == KC_RBRC &&
        dynamic_keymap_get_keycode(0, 6, 6) == KC_QUOT) {
        dynamic_keymap_set_keycode(0, 7, 5, KC_QUOT);
        dynamic_keymap_set_keycode(0, 6, 6, KC_RBRC);
    }

    if (dynamic_keymap_get_keycode(0, 9, 0) == TG(1) &&
        dynamic_keymap_get_keycode(0, 9, 1) == KC_LGUI &&
        dynamic_keymap_get_keycode(0, 9, 2) == KC_RALT &&
        dynamic_keymap_get_keycode(0, 9, 3) == KC_ENT) {
        dynamic_keymap_set_keycode(0, 9, 0, KC_BSPC);
        dynamic_keymap_set_keycode(0, 9, 1, KC_ENT);
        dynamic_keymap_set_keycode(0, 9, 2, KC_LGUI);
        dynamic_keymap_set_keycode(0, 9, 3, KC_RALT);
    }

    /* LAYOUT's left MAIN NAV thumb is matrix [4][0]. QMK caches its source
     * layer on press, so the existing NAV-layer TG does not handle its release.
     * Upgrade only a complete recognized factory MAIN after older migrations;
     * any custom base binding (including a different TG target) opts out.
     * Read flash defaults, not the dynamic EEPROM keymap. No other layer is
     * rewritten and no EEPROM reset is needed. Only the master owns the map. */
    if (is_keyboard_master() && dynamic_keymap_get_keycode(0, 4, 0) == TG(1)) {
        bool factory_main = true;
        for (uint8_t row = 0; row < MATRIX_ROWS && factory_main; ++row) {
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                uint16_t expected = row == 4 && col == 0 ? TG(1) :
                                    keycode_at_keymap_location_raw(0, row, col);
                if (dynamic_keymap_get_keycode(0, row, col) != expected) {
                    factory_main = false;
                    break;
                }
            }
        }
        if (factory_main) dynamic_keymap_set_keycode(0, 4, 0, MO(1));
    }
#endif
}

void keyboard_pre_init_user(void) { diag_absolute[0]=timer_read32(); }

uint32_t sm62_display_boot_elapsed(void) { return timer_elapsed32(diag_absolute[0]); }

void keyboard_post_init_user(void) {
    sm62_migrate_factory_defaults();
    diag_epoch=timer_read32(); diag_started=true; sm62_diag_stage(0);
#if defined(SM62_DUALCORE_RENDER)
    sm62_render_ready();
#else
    display_init_kb();
#endif
    sm62_diag_stage(3);
    transaction_register_rpc(RPC_SM62_DIAG,sm62_diag_rpc);
    sm62_settings_register_rpc();
    hid_app_layout_split_init();
}
