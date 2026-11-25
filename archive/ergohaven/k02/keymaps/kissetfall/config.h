#pragma once

#define VIAL_KEYBOARD_UID {0x03, 0xF0, 0x83, 0xA9, 0x6A, 0xD3, 0x40, 0x02}
#define VIAL_UNLOCK_COMBO_ROWS { 0, 0 }
#define VIAL_UNLOCK_COMBO_COLS { 0, 1 }

#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define DYNAMIC_KEYMAP_MACRO_COUNT 109

#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET // Activates the double-tap behavior
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 200U // Timeout window in ms in which the double tap can occur.
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_LED_MASK 0U // Specify a optional status led which blinks when entering the bootloader

#define BOTH_SHIFTS_TURNS_ON_CAPS_WORD

#define LEADER_TIMEOUT 250
#define LEADER_PER_KEY_TIMING
#define LEADER_NO_TIMEOUT
