#pragma once

#include <stdint.h>
#include <stdbool.h>

void kb_settings_init(void);

/* should override if needed */
const char *default_layer_label(uint8_t layer);

const char *layer_name(uint8_t layer);

extern bool layer_name_updated;
