MCU = RP2040
BOARD = GENERIC_RP_RP2040
BOOTLOADER = rp2040
# ALLOW_WARNINGS = yes
# PICO_INTRINSICS_ENABLED = no

# Build Options
SLEEP_LED_ENABLE = no       # Breathing sleep LED during USB suspend
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality
RGBLIGHT_ENABLE = no        # Enable keyboard RGB underglow
WS2812_DRIVER = vendor
RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = ws2812
VIALRGB_ENABLE = yes
SERIAL_DRIVER = vendor

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
NO_USB_STARTUP_CHECK = no
RAW_ENABLE = yes

UNICODE_COMMON = yes
UNICODE_ENABLE = yes

SRC += keyboards/ergohaven/ergohaven_main.c
SRC += keyboards/ergohaven/src/eh_settings.c
SRC += keyboards/ergohaven/src/eh_ruen.c
SRC += keyboards/ergohaven/hid.c
SRC += keyboards/ergohaven/src/eh_pointing.c
