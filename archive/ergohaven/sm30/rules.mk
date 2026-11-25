MCU = RP2040
BOARD = GENERIC_RP_RP2040
BOOTLOADER = rp2040
# ALLOW_WARNINGS = yes
# PICO_INTRINSICS_ENABLED = no

# Build Options
ENCODER_ENABLE = yes

NKRO_ENABLE = yes
BOOTMAGIC_ENABLE = yes
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
LTO_ENABLE = no
VIA_ENABLE = yes
TAP_DANCE_ENABLE = yes
COMBO_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
DYNAMIC_MACRO_ENABLE = yes
CAPS_WORD_ENABLE = yes
REPEAT_KEY_ENABLE = yes
AUTO_SHIFT_ENABLE = yes

SPLIT_KEYBOARD = no
SERIAL_DRIVER = vendor
RGBLIGHT_ENABLE = no

# OPT_DEFS = -O2
QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes
RAW_ENABLE = yes
BACKLIGHT_ENABLE = yes
BACKLIGHT_DRIVER = pwm

UNICODE_COMMON = yes
UNICODE_ENABLE = yes

SRC += keyboards/ergohaven/ergohaven_main.c
SRC += keyboards/ergohaven/src/eh_ruen.c
SRC += keyboards/ergohaven/hid.c
SRC += keyboards/ergohaven/src/eh_pointing.c
SRC += keyboards/ergohaven/src/eh_settings.c

SRC += keyboards/ergohaven/src/display/eh_display.c
SRC += keyboards/ergohaven/src/display/lvgl_helpers.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_iosevka_16.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_iosevka_22.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_iosevka_28.c
SRC += keyboards/ergohaven/src/display/eh_keycode_str.c

SRC += anim.c
SRC += display_modes.c
SRC += screen_layout.c
SRC += screen_anim.c
