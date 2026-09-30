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
    uint8_t volume;
    bool    volume_changed;
    uint8_t layout;
    bool    layout_changed;
    char    media_artist[32];
    bool    media_artist_changed;
    char    media_title[32];
    bool    media_title_changed;
} hid_data_t;

hid_data_t* get_hid_data(void);

void keyboard_post_init_hid(void);

bool is_hid_active(void);
bool is_hid_volume_active(void);
bool is_hid_time_active(void);

void hid_send_pointing_mode(pointing_mode_t mode);

#ifdef EH_APP_LAYOUT_ENABLE
bool hid_app_layout_process_packet(uint8_t *data, uint8_t length);
bool hid_app_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed);
bool hid_app_layout_process_encoder_event(uint8_t index, bool clockwise, bool pressed);
bool hid_app_layout_get_keycode(uint8_t layer, uint8_t control, uint16_t *keycode);
bool hid_app_layout_get_visual(uint8_t layer, uint8_t control, uint8_t *visual, uint8_t *state);
uint32_t hid_app_layout_visual_generation(void);
bool hid_app_layout_get_name(uint8_t layer, char *name, uint8_t size);
void hid_app_layout_task(void);
#endif
