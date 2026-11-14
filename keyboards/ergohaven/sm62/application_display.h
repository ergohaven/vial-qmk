#pragma once
#include <stdint.h>
uint16_t sm62_effective_keycode(uint8_t layer, uint8_t row, uint8_t col);
const char *sm62_effective_layer_name(uint8_t layer);
