// Copyright 2021 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef QUANTUM_PAINTER_SPI_ENABLE

#    include <stdint.h>

#    include "gpio.h"
#    include "qp_internal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Base SPI support

typedef struct qp_comms_spi_config_t {
    pin_t    chip_select_pin;
    uint16_t divisor;
    bool     lsb_first;
    int8_t   mode;
} qp_comms_spi_config_t;

bool     qp_comms_spi_init(painter_device_t device);
bool     qp_comms_spi_start(painter_device_t device);
uint32_t qp_comms_spi_send_data(painter_device_t device, const void* data, uint32_t byte_count);
void     qp_comms_spi_stop(painter_device_t device);

extern const painter_comms_vtable_t spi_comms_vtable;

#ifdef QP_LVGL_SPI_TRANSACTION
/* Synchronous, main-loop-only scope. A second begin is rejected, including
 * the same device. Nested driver start/stop pairs are reference counted.
 * CS still brackets each outer operation; no ownership survives end(). */
bool qp_comms_spi_transaction_begin(painter_device_t device);
bool qp_comms_spi_transaction_end(painter_device_t device);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SPI with D/C and RST pins

#    ifdef QUANTUM_PAINTER_SPI_DC_RESET_ENABLE

typedef struct qp_comms_spi_dc_reset_config_t {
    qp_comms_spi_config_t spi_config;
    pin_t                 dc_pin;
    pin_t                 reset_pin;
    bool                  command_params_uses_command_pin; // keep D/C held low when sending command sequences for data bytes
} qp_comms_spi_dc_reset_config_t;

bool     qp_comms_spi_dc_reset_init(painter_device_t device);
void     qp_comms_spi_dc_reset_send_command(painter_device_t device, uint8_t cmd);
uint32_t qp_comms_spi_dc_reset_send_data(painter_device_t device, const void* data, uint32_t byte_count);
void     qp_comms_spi_dc_reset_bulk_command_sequence(painter_device_t device, const uint8_t* sequence, size_t sequence_len);

extern const painter_comms_with_command_vtable_t spi_comms_with_dc_vtable;

#    endif // QUANTUM_PAINTER_SPI_DC_RESET_ENABLE

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QUANTUM_PAINTER_SPI_ENABLE
