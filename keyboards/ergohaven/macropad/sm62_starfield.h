#pragma once

#include <lvgl.h>
#include <stdint.h>

#define SM62_STAR_COUNT 20

typedef struct {
    lv_obj_t *object;
    int16_t   x[SM62_STAR_COUNT];
    int16_t   y[SM62_STAR_COUNT];
    uint8_t   speed[SM62_STAR_COUNT];
    uint32_t  timer;
    uint32_t  random_state;
    bool      frame_pending;
} sm62_starfield_t;

void sm62_starfield_init(sm62_starfield_t *field, lv_obj_t *parent, uint32_t seed);
void sm62_starfield_update(sm62_starfield_t *field);
void sm62_starfield_wait_for_frame(sm62_starfield_t *field);
void sm62_starfield_frame_completed(sm62_starfield_t *field);
