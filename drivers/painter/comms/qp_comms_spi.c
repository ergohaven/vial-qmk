// Copyright 2021 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef QUANTUM_PAINTER_SPI_ENABLE

#    include "spi_master.h"
#    include "qp_comms_spi.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Base SPI support

bool qp_comms_spi_init(painter_device_t device) {
    painter_driver_t *     driver       = (painter_driver_t *)device;
    qp_comms_spi_config_t *comms_config = (qp_comms_spi_config_t *)driver->comms_config;

    // Initialize the SPI peripheral
    spi_init();

    // Set up CS as output high
    gpio_set_pin_output(comms_config->chip_select_pin);
    gpio_write_pin_high(comms_config->chip_select_pin);

    return true;
}

#ifdef QP_LVGL_SPI_TRANSACTION
static painter_device_t transaction_device;
static uint16_t transaction_depth;
static bool transaction_acquired, transaction_failed;

bool qp_comms_spi_transaction_begin(painter_device_t device) {
    painter_driver_t *driver = (painter_driver_t *)device;
    if (transaction_device || !driver || !driver->validate_ok ||
        driver->comms_vtable != &spi_comms_with_dc_vtable.base) return false;
    transaction_device = device;
    transaction_depth = 0;
    transaction_acquired = transaction_failed = false;
    return true;
}

bool qp_comms_spi_transaction_end(painter_device_t device) {
    if (!device || transaction_device != device) return false;
    bool ok = !transaction_failed && transaction_depth == 0;
    if (transaction_acquired) {
        spi_stop();
        painter_driver_t *driver = (painter_driver_t *)device;
        qp_comms_spi_config_t *config = (qp_comms_spi_config_t *)driver->comms_config;
        gpio_write_pin_high(config->chip_select_pin);
    }
    transaction_device = NULL;
    transaction_depth = 0;
    transaction_acquired = false;
    return ok;
}
#endif

bool qp_comms_spi_start(painter_device_t device) {
    painter_driver_t *     driver       = (painter_driver_t *)device;
    qp_comms_spi_config_t *comms_config = (qp_comms_spi_config_t *)driver->comms_config;

#ifdef QP_LVGL_SPI_TRANSACTION
    if (transaction_device) {
        if (transaction_device != device || transaction_failed || transaction_depth == UINT16_MAX) {
            transaction_failed = true;
            return false;
        }
        if (transaction_depth == 0) {
            if (!transaction_acquired) {
                if (!spi_start(comms_config->chip_select_pin, comms_config->lsb_first, comms_config->mode, comms_config->divisor)) {
                    transaction_failed = true;
                    return false;
                }
                transaction_acquired = true;
            } else gpio_write_pin_low(comms_config->chip_select_pin);
        }
        ++transaction_depth;
        return true;
    }
#endif
    return spi_start(comms_config->chip_select_pin, comms_config->lsb_first, comms_config->mode, comms_config->divisor);
}

uint32_t qp_comms_spi_send_data(painter_device_t device, const void *data, uint32_t byte_count) {
    uint32_t       bytes_remaining = byte_count;
    const uint8_t *p               = (const uint8_t *)data;
    const uint32_t max_msg_length  = 1024;

    while (bytes_remaining > 0) {
        uint32_t bytes_this_loop = QP_MIN(bytes_remaining, max_msg_length);
#ifdef QP_LVGL_SPI_TRANSACTION
        if (transaction_device) {
            if (transaction_device != device || !transaction_depth || transaction_failed ||
                spi_transmit(p, bytes_this_loop) < 0) {
                transaction_failed = true;
                break;
            }
        } else
#endif
        spi_transmit(p, bytes_this_loop);
        p += bytes_this_loop;
        bytes_remaining -= bytes_this_loop;
    }

    return byte_count - bytes_remaining;
}

void qp_comms_spi_stop(painter_device_t device) {
    painter_driver_t *     driver       = (painter_driver_t *)device;
    qp_comms_spi_config_t *comms_config = (qp_comms_spi_config_t *)driver->comms_config;
#ifdef QP_LVGL_SPI_TRANSACTION
    if (transaction_device) {
        if (transaction_device != device || transaction_depth == 0) {
            transaction_failed = true;
            return;
        }
        if (--transaction_depth == 0) gpio_write_pin_high(comms_config->chip_select_pin);
        return;
    }
#endif
    spi_stop();
    gpio_write_pin_high(comms_config->chip_select_pin);
}

const painter_comms_vtable_t spi_comms_vtable = {
    .comms_init  = qp_comms_spi_init,
    .comms_start = qp_comms_spi_start,
    .comms_send  = qp_comms_spi_send_data,
    .comms_stop  = qp_comms_spi_stop,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SPI with D/C and RST pins

#    ifdef QUANTUM_PAINTER_SPI_DC_RESET_ENABLE

bool qp_comms_spi_dc_reset_init(painter_device_t device) {
    if (!qp_comms_spi_init(device)) {
        return false;
    }

    painter_driver_t *              driver       = (painter_driver_t *)device;
    qp_comms_spi_dc_reset_config_t *comms_config = (qp_comms_spi_dc_reset_config_t *)driver->comms_config;

    // Set up D/C as output low, if specified
    if (comms_config->dc_pin != NO_PIN) {
        gpio_set_pin_output(comms_config->dc_pin);
        gpio_write_pin_low(comms_config->dc_pin);
    }

    // Set up RST as output, if specified, performing a reset in the process
    if (comms_config->reset_pin != NO_PIN) {
        gpio_set_pin_output(comms_config->reset_pin);
        gpio_write_pin_low(comms_config->reset_pin);
        wait_ms(20);
        gpio_write_pin_high(comms_config->reset_pin);
        wait_ms(20);
    }

    return true;
}

uint32_t qp_comms_spi_dc_reset_send_data(painter_device_t device, const void *data, uint32_t byte_count) {
    painter_driver_t *              driver       = (painter_driver_t *)device;
    qp_comms_spi_dc_reset_config_t *comms_config = (qp_comms_spi_dc_reset_config_t *)driver->comms_config;
    gpio_write_pin_high(comms_config->dc_pin);
    return qp_comms_spi_send_data(device, data, byte_count);
}

void qp_comms_spi_dc_reset_send_command(painter_device_t device, uint8_t cmd) {
    painter_driver_t *              driver       = (painter_driver_t *)device;
    qp_comms_spi_dc_reset_config_t *comms_config = (qp_comms_spi_dc_reset_config_t *)driver->comms_config;
    gpio_write_pin_low(comms_config->dc_pin);
#ifdef QP_LVGL_SPI_TRANSACTION
    if (transaction_device) {
        if (transaction_device != device || !transaction_depth || transaction_failed || spi_write(cmd) < 0)
            transaction_failed = true;
        return;
    }
#endif
    spi_write(cmd);
}

void qp_comms_spi_dc_reset_bulk_command_sequence(painter_device_t device, const uint8_t *sequence, size_t sequence_len) {
    painter_driver_t *              driver       = (painter_driver_t *)device;
    qp_comms_spi_dc_reset_config_t *comms_config = (qp_comms_spi_dc_reset_config_t *)driver->comms_config;
    for (size_t i = 0; i < sequence_len;) {
        uint8_t command   = sequence[i];
        uint8_t delay     = sequence[i + 1];
        uint8_t num_bytes = sequence[i + 2];
        qp_comms_spi_dc_reset_send_command(device, command);
        if (num_bytes > 0) {
            if (comms_config->command_params_uses_command_pin) {
                for (uint8_t j = 0; j < num_bytes; j++) {
                    qp_comms_spi_dc_reset_send_command(device, sequence[i + 3 + j]);
                }
            } else {
                qp_comms_spi_dc_reset_send_data(device, &sequence[i + 3], num_bytes);
            }
        }
        if (delay > 0) {
            wait_ms(delay);
        }
        i += (3 + num_bytes);
    }
}

const painter_comms_with_command_vtable_t spi_comms_with_dc_vtable = {
    .base =
        {
            .comms_init  = qp_comms_spi_dc_reset_init,
            .comms_start = qp_comms_spi_start,
            .comms_send  = qp_comms_spi_dc_reset_send_data,
            .comms_stop  = qp_comms_spi_stop,
        },
    .send_command          = qp_comms_spi_dc_reset_send_command,
    .bulk_command_sequence = qp_comms_spi_dc_reset_bulk_command_sequence,
};

#    endif // QUANTUM_PAINTER_SPI_DC_RESET_ENABLE

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QUANTUM_PAINTER_SPI_ENABLE
