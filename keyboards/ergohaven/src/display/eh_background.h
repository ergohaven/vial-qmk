#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EH_BACKGROUND_WIDTH 240
#define EH_BACKGROUND_HEIGHT 280
#define EH_BACKGROUND_IMAGE_PALETTE_SIZE 1024
#define EH_BACKGROUND_IMAGE_FRAME_SIZE (EH_BACKGROUND_IMAGE_PALETTE_SIZE + EH_BACKGROUND_WIDTH * EH_BACKGROUND_HEIGHT)
#define EH_BACKGROUND_ANIMATION_PALETTE_SIZE 256
#define EH_BACKGROUND_ANIMATION_PIXEL_SIZE (EH_BACKGROUND_WIDTH * EH_BACKGROUND_HEIGHT * 3 / 4)
// Keep every frame page-aligned so no frame crosses the reserved settings gap.
#define EH_BACKGROUND_ANIMATION_FRAME_SIZE (EH_BACKGROUND_ANIMATION_PALETTE_SIZE + EH_BACKGROUND_ANIMATION_PIXEL_SIZE + 32)
#define EH_BACKGROUND_FORMAT_MAX_FRAMES 56
// The first region fits below the historical settings slot on 2 MiB boards.
#if PICO_FLASH_SIZE_BYTES <= 2097152
#    define EH_BACKGROUND_MAX_FRAMES 20
#else
#    define EH_BACKGROUND_MAX_FRAMES EH_BACKGROUND_FORMAT_MAX_FRAMES
#endif
#define EH_BACKGROUND_FIRST_FRAME_COUNT 20
#define EH_BACKGROUND_MAX_PACKAGE_SIZE (256 + EH_BACKGROUND_ANIMATION_FRAME_SIZE * EH_BACKGROUND_MAX_FRAMES)
#define EH_BACKGROUND_SPEED_DEFAULT_PERCENT 100
#define EH_BACKGROUND_SPEED_MIN_PERCENT 25
#define EH_BACKGROUND_SPEED_MAX_PERCENT 400

enum {
    EH_BACKGROUND_KIND_NONE = 0,
    EH_BACKGROUND_KIND_IMAGE = 1,
    EH_BACKGROUND_KIND_ANIMATION = 2,
};

void eh_background_init(void);
bool eh_background_process_hid(uint8_t *data, uint8_t length);
bool eh_background_is_valid(void);
uint8_t eh_background_kind(void);
uint8_t eh_background_frame_count(void);
const uint8_t *eh_background_frame_data(uint8_t frame);
uint16_t eh_background_frame_delay(uint8_t frame);
uint16_t eh_background_speed_percent(void);
uint32_t eh_background_generation(void);
bool eh_background_animation_paused(void);

void eh_background_note_config_read(uint8_t command, uint8_t subcommand);
