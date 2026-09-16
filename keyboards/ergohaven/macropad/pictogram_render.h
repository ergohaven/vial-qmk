#pragma once
#include <stdint.h>
#include <string.h>
#include "src/display/eh_pictograms.h"

// Stored/editor bit 1 is foreground. LVGL ALPHA_1BIT maps it to opacity 255.
// Resample to an exact native 35px mask instead of a fractional LVGL transform.
#define EH_ICON_RENDER_SIZE 35
#define EH_ICON_RENDER_STRIDE ((EH_ICON_RENDER_SIZE + 7) / 8)
#define EH_ICON_RENDER_BYTES (EH_ICON_RENDER_STRIDE * EH_ICON_RENDER_SIZE)
static inline void eh_render_pictogram(const uint8_t *source, uint8_t width, uint8_t *dest) {
    memset(dest, 0, EH_ICON_RENDER_BYTES);
    for (unsigned y = 0; y < EH_ICON_RENDER_SIZE; ++y) {
        unsigned sy = (2 * y + 1) * width / (2 * EH_ICON_RENDER_SIZE);
        for (unsigned x = 0; x < EH_ICON_RENDER_SIZE; ++x) {
            unsigned sx = (2 * x + 1) * width / (2 * EH_ICON_RENDER_SIZE);
            unsigned bit = sy * width + sx;
            if (source[bit / 8] & (0x80u >> (bit % 8)))
                dest[y * EH_ICON_RENDER_STRIDE + x / 8] |= 0x80u >> (x % 8);
        }
    }
}
