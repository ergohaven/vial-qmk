#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EH_STARTUP_IMAGE_WIDTH 240
#define EH_STARTUP_IMAGE_HEIGHT 280
#define EH_STARTUP_IMAGE_PALETTE_SIZE 1024
#define EH_STARTUP_IMAGE_DATA_SIZE (EH_STARTUP_IMAGE_PALETTE_SIZE + EH_STARTUP_IMAGE_WIDTH * EH_STARTUP_IMAGE_HEIGHT)

void eh_startup_image_init(void);
bool eh_startup_image_process_hid(uint8_t *data, uint8_t length);
bool eh_startup_image_is_valid(void);
const uint8_t *eh_startup_image_data(void);
