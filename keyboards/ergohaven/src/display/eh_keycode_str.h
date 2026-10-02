#pragma once
#include <stdbool.h>
#include <stdint.h>

void get_keycode_str(char* str, uint16_t);
const char *keycode_to_str(uint16_t keycode);
bool eh_keycode_str_uses_mac_modifiers(void);
