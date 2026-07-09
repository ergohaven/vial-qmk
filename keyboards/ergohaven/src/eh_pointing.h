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
    EH_L_SNP = EH_RSRV2,
    EH_L_SCR,
    EH_L_TXT,
    EH_LED_BL = EH_RSRV7 + 1,
    EH_SNP,
    EH_SCR,
    EH_TXT,
    EH_USR1,
    EH_USR2,
    EH_USR3,
};

typedef enum { ROT_0, ROT_90, ROT_180, ROT_270 } orientation_t;

typedef enum {
    SPLIT_POINTING_DEVICE_LEFT_BALL = 0,
    SPLIT_POINTING_DEVICE_RIGHT_BALL,
    SPLIT_POINTING_DEVICE_LEFT_TOUCH,
    SPLIT_POINTING_DEVICE_RIGHT_TOUCH,
    SPLIT_POINTING_DEVICE_COUNT,
} split_pointing_device_id_t;

typedef enum {
    SPLIT_POINTING_SIDE_LEFT = 0,
    SPLIT_POINTING_SIDE_RIGHT,
    SPLIT_POINTING_SIDE_COUNT,
} split_pointing_side_t;

typedef struct {
    uint8_t axis[SPLIT_POINTING_DEVICE_COUNT];
    uint8_t dpi_idx[SPLIT_POINTING_DEVICE_COUNT];
    uint8_t mode[SPLIT_POINTING_SIDE_COUNT];
    uint8_t sens[SPLIT_POINTING_SIDE_COUNT][3];
    uint8_t invert_scroll[SPLIT_POINTING_SIDE_COUNT];
    uint8_t acceleration[SPLIT_POINTING_SIDE_COUNT];
    uint8_t auto_mouse_enable;
    uint8_t auto_mouse_layer;
#ifdef EH_KEYBOARD_SPLIT_POINTING_V2
    // phenom-side-modules-v0.0.2: per-half sticky/auto-layer settings.
    uint8_t sticky_mode[SPLIT_POINTING_SIDE_COUNT];
    uint8_t side_auto_mouse_enable[SPLIT_POINTING_SIDE_COUNT];
    uint8_t side_auto_mouse_layer[SPLIT_POINTING_SIDE_COUNT];
    uint8_t invert_text[SPLIT_POINTING_SIDE_COUNT];
    uint8_t version;
    // phenom-side-modules-v0.0.3: auto mouse layer timeout, 250 ms * (idx + 1).
    uint8_t auto_mouse_timeout_idx;
#endif
} kb_settings_split_pointing_t;

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
        bool          invert_text : 1;
    };
} kb_settings_pointing_t;

// should override if needed
kb_settings_pointing_t get_settings_pointing_default(void);
kb_settings_pointing_t get_settings_pointing(void);
void kb_settings_pointing_update(kb_settings_pointing_t new_config);
void set_settings_pointing(kb_settings_pointing_t config);

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

void set_invert_text(bool invert);
bool get_invert_text(void);

void set_acceleration(bool acc);
bool get_acceleration(void);

bool    get_split_pointing_auto_mouse_enable(void);
void    set_split_pointing_auto_mouse_enable(bool enable);
uint8_t get_split_pointing_auto_mouse_layer(void);
void    set_split_pointing_auto_mouse_layer(uint8_t layer);
uint8_t get_split_pointing_auto_mouse_timeout_idx(void);
void    set_split_pointing_auto_mouse_timeout_idx(uint8_t idx);
void    set_pointing_auto_mouse_override(bool enabled, bool active);

void          set_orientation(orientation_t orientation);
orientation_t get_orientation(void);

void set_sticky_mode(bool invert);
bool get_sticky_mode(void);

void set_pointing_mode(pointing_mode_t mode);
pointing_mode_t get_pointing_mode(void);

void set_pointing_mode_from_hid(pointing_mode_t mode);

void set_led_blinks(bool led);

bool get_led_blinks(void);

void kb_settings_pointing_init(void);

void kb_settings_pointing_reset(void);

kb_settings_split_pointing_t get_split_pointing_settings_default(void);
kb_settings_split_pointing_t get_split_pointing_settings(void);
void                       kb_settings_split_pointing_init(void);
void                       kb_settings_split_pointing_reset(void);
void                       set_split_pointing_settings(kb_settings_split_pointing_t config);
orientation_t              get_split_pointing_device_orientation(split_pointing_device_id_t device);
void                       set_split_pointing_device_orientation(split_pointing_device_id_t device, orientation_t orientation);
uint8_t                    get_split_pointing_device_dpi_index(split_pointing_device_id_t device);
void                       set_split_pointing_device_dpi_index(split_pointing_device_id_t device, uint8_t dpi_index);
pointing_mode_t            get_split_pointing_side_mode(split_pointing_side_t side);
void                       set_split_pointing_side_mode(split_pointing_side_t side, pointing_mode_t mode);
uint8_t                    get_split_pointing_side_sens(split_pointing_side_t side, pointing_mode_t mode);
void                       set_split_pointing_side_sens(split_pointing_side_t side, pointing_mode_t mode, uint8_t sens);
bool                       get_split_pointing_side_invert_scroll(split_pointing_side_t side);
void                       set_split_pointing_side_invert_scroll(split_pointing_side_t side, bool invert_scroll);
bool                       get_split_pointing_side_acceleration(split_pointing_side_t side);
void                       set_split_pointing_side_acceleration(split_pointing_side_t side, bool acceleration);
bool                       get_split_pointing_side_invert_text(split_pointing_side_t side);
void                       set_split_pointing_side_invert_text(split_pointing_side_t side, bool invert_text);
bool                       get_split_pointing_side_sticky_mode(split_pointing_side_t side);
void                       set_split_pointing_side_sticky_mode(split_pointing_side_t side, bool sticky_mode);
uint8_t                    get_split_pointing_side_auto_mouse_layer(split_pointing_side_t side);
void                       set_split_pointing_side_auto_mouse_layer(split_pointing_side_t side, uint8_t layer);
bool                       get_split_pointing_auto_mouse_mode_enabled(pointing_mode_t mode);
void                       set_split_pointing_auto_mouse_mode_enabled(pointing_mode_t mode, bool enabled);
bool                       get_split_pointing_side_auto_mouse_mode_enabled(split_pointing_side_t side, pointing_mode_t mode);
void                       set_split_pointing_side_auto_mouse_mode_enabled(split_pointing_side_t side, pointing_mode_t mode, bool enabled);
