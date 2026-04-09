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

typedef union {
    uint64_t raw;
    struct {
        uint8_t       sens[4];
        uint16_t      dpi;
        bool          invert_scroll : 1;
        bool          acceleration : 1;
        orientation_t orientation : 2;
        uint8_t       mode : 2; // trackball mini v1/v2 modes
        bool          sticky_mode : 1;
        bool          led_blinks : 1;
    };
} kb_settings_pointing_t;

// should override if needed
kb_settings_pointing_t get_settings_pointing_default(void);

bool process_record_pointing(uint16_t keycode, keyrecord_t *record);

void     set_cpi(uint16_t dpi);
uint16_t get_cpi(void);

void    set_scroll_sens(uint8_t sens);
uint8_t get_scroll_sens(void);

void    set_sniper_sens(uint8_t sens);
uint8_t get_sniper_sens(void);

void    set_text_sens(uint8_t sens);
uint8_t get_text_sens(void);

void set_invert_scroll(bool invert);
bool get_invert_scroll(void);

void set_acceleration(bool acc);
bool get_acceleration(void);

void          set_orientation(orientation_t orientation);
orientation_t get_orientation(void);

void set_sticky_mode(bool invert);
bool get_sticky_mode(void);

void set_pointing_mode(pointing_mode_t mode);

void set_pointing_mode_from_hid(pointing_mode_t mode);

void set_led_blinks(bool led);

bool get_led_blinks(void);

void kb_settings_pointing_init(void);

void kb_settings_pointing_reset(void);
