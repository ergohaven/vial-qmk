#pragma once
/* Shared Macropad/SM62 application action and execution-state artwork. */
#include "macropad/pictogram_render.h"
#include "macropad/action_icons.h"
static void integration_icon_pixel(uint8_t *bits, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= EH_ICON_RENDER_SIZE || y >= EH_ICON_RENDER_SIZE) return;
    bits[y * EH_ICON_RENDER_STRIDE + x / 8] |= 0x80u >> (x % 8);
}

static void integration_icon_line(uint8_t *bits, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t width) {
    int16_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -(y1 >= y0 ? y1 - y0 : y0 - y1);
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t error = dx + dy;
    // Width 2 used to rasterize as a three-pixel square brush.
    int8_t low = width == 2 ? -1 : -(int8_t)(width / 2);
    int8_t high = width == 2 ? 0 : (int8_t)(width / 2);
    while (true) {
        for (int8_t oy = low; oy <= high; oy++) {
            for (int8_t ox = low; ox <= high; ox++) {
                integration_icon_pixel(bits, x0 + ox, y0 + oy);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int16_t twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void integration_icon_rect(uint8_t *bits, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    integration_icon_line(bits, x1, y1, x2, y1, 1);
    integration_icon_line(bits, x2, y1, x2, y2, 1);
    integration_icon_line(bits, x2, y2, x1, y2, 1);
    integration_icon_line(bits, x1, y2, x1, y1, 1);
}

static void integration_icon_spinner(uint8_t *bits, uint8_t frame) {
    static const int8_t points[8][2] = {
        {17, 4}, {26, 8}, {30, 17}, {26, 26}, {17, 30}, {8, 26}, {4, 17}, {8, 8},
    };
    for (uint8_t offset = 0; offset < 3; offset++) {
        uint8_t point = (frame + 8 - offset) % 8;
        uint8_t width = offset == 0 ? 3 : 1;
        integration_icon_line(bits, points[point][0], points[point][1], points[point][0], points[point][1], width);
    }
}

static void integration_icon_check(uint8_t *bits) {
    integration_icon_line(bits, 6, 18, 14, 27, 3);
    integration_icon_line(bits, 14, 27, 30, 8, 3);
}

static void integration_icon_error(uint8_t *bits) {
    integration_icon_line(bits, 8, 8, 27, 27, 3);
    integration_icon_line(bits, 27, 8, 8, 27, 3);
}

static void integration_icon_circle(uint8_t *bits, int16_t center_x, int16_t center_y, int16_t radius, uint8_t width) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t error = 1 - radius;
    while (x >= y) {
        const int16_t points[8][2] = {
            {center_x + x, center_y + y}, {center_x + y, center_y + x},
            {center_x - y, center_y + x}, {center_x - x, center_y + y},
            {center_x - x, center_y - y}, {center_x - y, center_y - x},
            {center_x + y, center_y - x}, {center_x + x, center_y - y},
        };
        for (uint8_t point = 0; point < 8; point++) {
            integration_icon_line(bits, points[point][0], points[point][1], points[point][0], points[point][1], width);
        }
        y++;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

static void integration_icon_base(uint8_t *bits, uint8_t visual) {
    if (visual >= EH_ACTION_ICON_FIRST_VISUAL && visual < EH_ACTION_ICON_FIRST_VISUAL + EH_ACTION_ICON_COUNT) {
        eh_render_pictogram(eh_action_icons[visual - EH_ACTION_ICON_FIRST_VISUAL], 35, bits);
        return;
    }
    if (visual == 1) {
        integration_icon_rect(bits, 6, 8, 28, 27);
        integration_icon_line(bits, 11, 13, 23, 13, 1);
        integration_icon_line(bits, 11, 18, 20, 18, 1);
        integration_icon_line(bits, 23, 20, 23, 30, 3);
        integration_icon_line(bits, 18, 25, 28, 25, 3);
    } else if (visual == 2) {
        integration_icon_rect(bits, 8, 5, 27, 30);
        integration_icon_line(bits, 13, 12, 22, 12, 1);
        integration_icon_line(bits, 13, 17, 22, 17, 1);
        integration_icon_line(bits, 17, 21, 17, 28, 3);
        integration_icon_line(bits, 13, 25, 21, 25, 3);
    } else if (visual == 3) {
        integration_icon_rect(bits, 5, 5, 30, 30);
        integration_icon_check(bits);
    } else if (visual == 4) {
        integration_icon_line(bits, 8, 10, 17, 5, 2);
        integration_icon_line(bits, 17, 5, 26, 10, 2);
        integration_icon_line(bits, 26, 10, 29, 17, 2);
        integration_icon_line(bits, 29, 17, 25, 16, 2);
        integration_icon_line(bits, 29, 17, 30, 13, 2);
        integration_icon_line(bits, 26, 25, 17, 30, 2);
        integration_icon_line(bits, 17, 30, 8, 25, 2);
        integration_icon_line(bits, 8, 25, 5, 18, 2);
        integration_icon_line(bits, 5, 18, 9, 19, 2);
        integration_icon_line(bits, 5, 18, 4, 22, 2);
    } else if (visual == 5) {
        // OBS recording: ring with a filled record dot.
        integration_icon_circle(bits, 17, 17, 13, 2);
        integration_icon_circle(bits, 17, 17, 6, 5);
    } else if (visual == 6) {
        // OBS streaming: antenna and two broadcast waves.
        integration_icon_circle(bits, 17, 18, 2, 3);
        integration_icon_line(bits, 17, 20, 17, 29, 2);
        integration_icon_line(bits, 12, 29, 22, 29, 2);
        integration_icon_line(bits, 11, 14, 8, 18, 2);
        integration_icon_line(bits, 8, 18, 11, 22, 2);
        integration_icon_line(bits, 23, 14, 26, 18, 2);
        integration_icon_line(bits, 26, 18, 23, 22, 2);
        integration_icon_line(bits, 7, 9, 3, 18, 2);
        integration_icon_line(bits, 3, 18, 7, 27, 2);
        integration_icon_line(bits, 27, 9, 31, 18, 2);
        integration_icon_line(bits, 31, 18, 27, 27, 2);
    } else if (visual == 7) {
        // OBS scene: stacked canvas frames.
        integration_icon_rect(bits, 8, 5, 30, 24);
        integration_icon_rect(bits, 4, 10, 26, 29);
        integration_icon_line(bits, 8, 25, 14, 18, 2);
        integration_icon_line(bits, 14, 18, 19, 23, 2);
        integration_icon_line(bits, 19, 23, 24, 17, 2);
    } else if (visual == 8) {
        // OBS audio: microphone.
        integration_icon_rect(bits, 13, 5, 21, 21);
        integration_icon_line(bits, 9, 16, 9, 20, 2);
        integration_icon_line(bits, 9, 20, 13, 25, 2);
        integration_icon_line(bits, 13, 25, 21, 25, 2);
        integration_icon_line(bits, 21, 25, 25, 20, 2);
        integration_icon_line(bits, 25, 20, 25, 16, 2);
        integration_icon_line(bits, 17, 25, 17, 31, 2);
        integration_icon_line(bits, 11, 31, 23, 31, 2);
    } else if (visual == 9) {
        // OBS source visibility: eye.
        integration_icon_line(bits, 3, 18, 9, 11, 2);
        integration_icon_line(bits, 9, 11, 17, 8, 2);
        integration_icon_line(bits, 17, 8, 25, 11, 2);
        integration_icon_line(bits, 25, 11, 31, 18, 2);
        integration_icon_line(bits, 31, 18, 25, 25, 2);
        integration_icon_line(bits, 25, 25, 17, 28, 2);
        integration_icon_line(bits, 17, 28, 9, 25, 2);
        integration_icon_line(bits, 9, 25, 3, 18, 2);
        integration_icon_circle(bits, 17, 18, 5, 3);
    } else if (visual == 10) {
        // OBS Studio Mode: preview and program side by side.
        integration_icon_rect(bits, 3, 8, 15, 27);
        integration_icon_rect(bits, 20, 8, 32, 27);
        integration_icon_line(bits, 16, 14, 19, 14, 2);
        integration_icon_line(bits, 18, 12, 20, 14, 2);
        integration_icon_line(bits, 18, 16, 20, 14, 2);
        integration_icon_line(bits, 19, 21, 16, 21, 2);
        integration_icon_line(bits, 17, 19, 15, 21, 2);
        integration_icon_line(bits, 17, 23, 15, 21, 2);
    } else if (visual == 11) {
        // OBS refresh: two circular arrows.
        integration_icon_line(bits, 7, 17, 9, 11, 2);
        integration_icon_line(bits, 9, 11, 15, 7, 2);
        integration_icon_line(bits, 15, 7, 23, 9, 2);
        integration_icon_line(bits, 23, 9, 28, 14, 2);
        integration_icon_line(bits, 28, 14, 29, 9, 2);
        integration_icon_line(bits, 28, 14, 23, 14, 2);
        integration_icon_line(bits, 28, 19, 26, 25, 2);
        integration_icon_line(bits, 26, 25, 20, 29, 2);
        integration_icon_line(bits, 20, 29, 12, 27, 2);
        integration_icon_line(bits, 12, 27, 7, 22, 2);
        integration_icon_line(bits, 7, 22, 6, 27, 2);
        integration_icon_line(bits, 7, 22, 12, 22, 2);
    }
}
