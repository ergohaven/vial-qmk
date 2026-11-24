// SPDX-License-Identifier: GPL-2.0-or-later
#include "eh_encoder.h"
#include "eeconfig.h"
#include "encoder.h"
#ifdef EH_ENCODER_INTERVAL_SETTINGS
#    include "eh_pointing.h"
#endif

// Fixed-encoder targets reserve the otherwise unused user word; EEPROM offsets stay stable.
#define ENCODER_SETTINGS_MAGIC 0x454E0100UL
static uint8_t settings;
static const uint8_t intervals_ms[] = {0, 5, 10, 15, 20, 30, 40, 60, 80, 100};

void eh_encoder_settings_init(void) {
    uint32_t raw = eeconfig_read_user();
    uint8_t value = (uint8_t)raw;
    settings = (raw & 0xFFFFFF00UL) == ENCODER_SETTINGS_MAGIC && !(value & 8) && (value >> 4) < sizeof(intervals_ms) ? value : 0;
}

void eh_encoder_settings_reset(void) {
    settings = 0; // 0 ms, one action per detent, matching K:03 WE defaults.
    eeconfig_update_user(ENCODER_SETTINGS_MAGIC);
}

uint8_t eh_encoder_setting_get(bool steps) {
#ifdef EH_ENCODER_INTERVAL_SETTINGS
    // Keep the existing pointing-settings interval as its only persistent owner.
    if (!steps) return get_pointing_encoder_interval_idx();
#endif
    return steps ? settings & 7 : settings >> 4;
}

bool eh_encoder_setting_set(bool steps, uint8_t value) {
    if (value >= (steps ? 8 : sizeof(intervals_ms))) return false;
#ifdef EH_ENCODER_INTERVAL_SETTINGS
    if (!steps) {
        set_pointing_encoder_interval_idx(value);
        return true;
    }
#endif
    uint8_t next = steps ? (settings & 0xF0) | value : (settings & 7) | (value << 4);
    if (next != settings) {
        settings = next;
        eeconfig_update_user(ENCODER_SETTINGS_MAGIC | settings);
    }
    return true;
}

uint8_t encoder_map_key_delay(uint8_t index) {
    (void)index;
    return intervals_ms[eh_encoder_setting_get(false)];
}

uint8_t encoder_map_steps(uint8_t index) {
    (void)index;
    return eh_encoder_setting_get(true) + 1;
}
