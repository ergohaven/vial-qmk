#pragma once
#include <stdint.h>
#include <stdbool.h>
uint8_t eh_date_get(uint8_t index);
bool eh_date_set(uint8_t index, uint8_t value);
void eh_extra_settings_housekeep(void);
void eh_extra_settings_read(uint8_t offset, void *data, uint8_t size);
void eh_extra_settings_write(uint8_t offset, const void *data, uint8_t size);
void eh_date_reset(void);
void eh_extra_settings_flush(void);
