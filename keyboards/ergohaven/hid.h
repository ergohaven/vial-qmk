#pragma once

#include <stdint.h>
#include "src/eh_pointing.h"

typedef struct {
    bool    hid_changed;
    uint16_t year;
    uint8_t month, day;
    bool date_valid;
    uint8_t hours;
    uint8_t minutes;
    bool    time_changed;
#if defined(KEYBOARD_ergohaven_sm62)
    uint8_t date_day;
    uint8_t date_month;
    uint16_t date_year;
    bool    date_changed;
#endif
    uint8_t volume;
    bool    volume_changed;
    uint8_t layout;
    bool    layout_changed;
    char    media_artist[32];
    bool    media_artist_changed;
#if defined(KEYBOARD_ergohaven_sm62)
    char    media_title[256]; /* Up to 255 UTF-8 bytes, independent of display width. */
#else
    char    media_title[32];
#endif
    bool    media_title_changed;
#if defined(KEYBOARD_ergohaven_sm62)
    uint32_t media_position_seconds;
    uint32_t media_duration_seconds;
    uint32_t media_progress_time;
    bool media_progress_valid;
    uint64_t sm62_pressed_mask;
    uint32_t sm62_key_event_sequence;
    uint8_t  sm62_last_row;
    uint8_t  sm62_last_col;
#endif
} hid_data_t;

hid_data_t* get_hid_data(void);

void keyboard_post_init_hid(void);

bool is_hid_active(void);
bool is_hid_volume_active(void);
bool is_hid_time_active(void);
/* True only for an EN/RU layout report received within the host sync window. */
bool is_hid_layout_valid(void);

void hid_housekeeping_task(void);

void hid_send_pointing_mode(pointing_mode_t mode);

#ifdef EH_APP_LAYOUT_ENABLE
void hid_app_layout_changed_kb(void);
bool hid_app_layout_process_packet(uint8_t *data, uint8_t length);
bool hid_app_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed);
bool hid_app_layout_get_combo_keycode(uint8_t row, uint8_t col, uint16_t *keycode);
#if defined(KEYBOARD_ergohaven_sm62)
bool hid_app_layout_process_cached_keyevent(uint8_t row, uint8_t col, bool pressed, uint16_t keycode, uint32_t revision, uint8_t layer);
void hid_app_layout_capture_identity(uint32_t *revision, uint8_t *layer);
#endif
bool hid_app_layout_process_encoder_event(uint8_t index, bool clockwise, bool pressed);
bool hid_app_layout_get_keycode(uint8_t layer, uint8_t control, uint16_t *keycode);
bool hid_app_layout_get_visual(uint8_t layer, uint8_t control, uint8_t *visual, uint8_t *state);
uint32_t hid_app_layout_visual_generation(void);
bool hid_app_layout_get_name(uint8_t layer, char *name, uint8_t size);
void hid_app_layout_task(void);
#ifdef EH_SM62_SPLIT_RUNTIME_ENABLE
const char *hid_sm62_default_layer_name(uint8_t layer);
uint16_t hid_sm62_default_keycode(uint8_t layer, uint8_t row, uint8_t col);
void hid_app_layout_split_init(void);
void hid_app_layout_split_task(void);
#endif
#endif
