// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

void eh_encoder_settings_init(void);
void eh_encoder_settings_reset(void);
uint8_t eh_encoder_setting_get(bool steps);
bool eh_encoder_setting_set(bool steps, uint8_t value);
