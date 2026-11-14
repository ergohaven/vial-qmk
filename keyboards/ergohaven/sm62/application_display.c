#include "application_display.h"
#include "application_geometry.h"
#include "hid.h"
#include "dynamic_keymap.h"
#include "src/eh_settings.h"

uint16_t sm62_effective_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    uint16_t code;
    for (uint8_t c = 0; c < 62; ++c) {
        if (sm62_application_positions[c][0] == row && sm62_application_positions[c][1] == col) {
            if (hid_app_layout_get_keycode(layer, c, &code)) return code;
            break;
        }
    }
    do {
        code = hid_sm62_default_keycode(layer, row, col);
        if (code != KC_TRNS || layer == 0) return code;
        --layer;
    } while (true);
}

const char *sm62_effective_layer_name(uint8_t layer) {
    static char name[23];
    return hid_app_layout_get_name(layer, name, sizeof(name)) ? name : hid_sm62_default_layer_name(layer);
}
