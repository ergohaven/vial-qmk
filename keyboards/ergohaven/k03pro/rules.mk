MCU = RP2040
BOARD = GENERIC_RP_RP2040
BOOTLOADER = rp2040
# ALLOW_WARNINGS = yes
# PICO_INTRINSICS_ENABLED = no

# Build Options
ENCODER_ENABLE = no

POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = azoteq_iqs5xx

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

SPLIT_KEYBOARD = yes
SERIAL_DRIVER = vendor
RGBLIGHT_ENABLE = yes

# OPT_DEFS = -O2
QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes
RAW_ENABLE = yes
BACKLIGHT_ENABLE = yes
BACKLIGHT_DRIVER = pwm

UNICODE_COMMON = yes
UNICODE_ENABLE = yes

SRC += keyboards/ergohaven/ergohaven_rgb.c
SRC += keyboards/ergohaven/ergohaven_main.c
SRC += keyboards/ergohaven/src/eh_settings.c
SRC += keyboards/ergohaven/src/eh_ruen.c
SRC += keyboards/ergohaven/hid.c
SRC += keyboards/ergohaven/src/eh_pointing.c

SRC += keyboards/ergohaven/src/display/lvgl_helpers.c
SRC += keyboards/ergohaven/src/display/eh_display_modes.c
SRC += keyboards/ergohaven/src/display/eh_display.c
SRC += keyboards/ergohaven/src/display/eh_screen_splash.c
SRC += keyboards/ergohaven/src/display/eh_screen_home.c
SRC += keyboards/ergohaven/src/display/eh_screen_volume.c
SRC += keyboards/ergohaven/src/display/eh_logo.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_28.c
