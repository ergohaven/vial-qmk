#pragma once

#include "quantum.h"
#include "ergohaven.h"

typedef enum {
    POINTING_MODE_NORMAL = 0,
    POINTING_MODE_SNIPER,
    POINTING_MODE_SCROLL,
    POINTING_MODE_TEXT,
    POINTING_MODE_USR1,
    POINTING_MODE_USR2,
    POINTING_MODE_USR3,
} pointing_mode_t;

enum {
    EH_LED_BL = EH_RSRV7 + 1,
    EH_SNP,
    EH_SCR,
    EH_TXT,
    EH_USR1,
    EH_USR2,
    EH_USR3,
};

typedef enum { ROT_0, ROT_90, ROT_180, ROT_270 } orientation_t;

bool process_record_pointing(uint16_t keycode, keyrecord_t *record);

void set_scroll_sens(int32_t sens);

void set_sniper_sens(int32_t sens);

void set_text_sens(int32_t sens);

void set_automouse(uint8_t layer);

void set_invert_scroll(bool invert);

void set_acceleration(bool acc);

void set_orientation(orientation_t orientation);

void set_pointing_mode(pointing_mode_t mode);

void set_pointing_mode_from_hid(pointing_mode_t mode);

void set_led_blinks(bool led);

bool get_led_blinks(void);
