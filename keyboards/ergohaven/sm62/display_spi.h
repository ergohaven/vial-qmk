// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hal.h"

bool sm62_display_spi_start(const SPIConfig *config);
bool sm62_display_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length);
void sm62_display_spi_stop(void);
bool sm62_display_spi_failed(void);
bool sm62_spi_quiescent(void);
