#pragma once

#include <stdint.h>
#include "src/eh_pointing.h"

typedef struct {
    bool    hid_changed;
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

void hid_send_pointing_mode(pointing_mode_t mode);
