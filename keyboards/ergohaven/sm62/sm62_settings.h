// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <qmk_settings.h>

typedef struct {
    uint8_t mode;           // 0 clock, 1 player, 2 ship, 3 persistent keymap
    uint8_t ship_enabled;   // idle ship overrides clock/player after timeout
    uint8_t ship_timeout_s;
    uint8_t keymap_enabled;
    uint8_t cyrillic_enabled;
} sm62_side_settings_t;

const sm62_side_settings_t *sm62_local_display_settings(void);
const sm62_side_settings_t *sm62_remote_display_settings(void);
bool sm62_local_mode_transition_pending(void);
bool sm62_local_date_enabled(void);
bool sm62_local_language_enabled(void);
bool sm62_local_stars_enabled(void);
bool sm62_local_screens_off(void);
uint16_t sm62_local_keymap_hold_ms(void);
uint32_t sm62_local_orbit_idle_ms(uint32_t activity_idle);
int sm62_setting_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz);
int sm62_setting_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz);
void sm62_settings_register_rpc(void);
void sm62_settings_sync_task(void);
