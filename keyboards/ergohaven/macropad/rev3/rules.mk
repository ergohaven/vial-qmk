MCU = RP2040
BOARD = GENERIC_RP_RP2040
BOOTLOADER = rp2040
OPT_DEFS += -DPICO_FLASH_SIZE_BYTES=4194304
OPT_DEFS += -DEH_PICTOGRAM_ENABLE
EXTRALDFLAGS += -Wl,--defsym=FLASH_LEN=0x000B0000
# ALLOW_WARNINGS = yes
# PICO_INTRINSICS_ENABLED = no

# Build Options
ENCODER_ENABLE = yes
OPT_DEFS += -DEH_DISPLAY_COLOR_SETTINGS

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
BACKLIGHT_ENABLE = yes
BACKLIGHT_DRIVER = pwm

SERIAL_DRIVER = vendor

# OPT_DEFS = -O2
QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes
RGBLIGHT_ENABLE = no
RAW_ENABLE = yes

UNICODE_COMMON = yes
UNICODE_ENABLE = yes

SRC += ../display_modes.c
SRC += ../screen_layout.c
SRC += keyboards/ergohaven/src/display/lvgl_helpers.c
SRC += keyboards/ergohaven/src/display/eh_keycode_str.c
SRC += keyboards/ergohaven/src/display/eh_display.c
SRC += keyboards/ergohaven/src/display/eh_screen_splash.c
SRC += keyboards/ergohaven/src/display/eh_screen_home.c
SRC += keyboards/ergohaven/src/display/eh_screen_volume.c
SRC += keyboards/ergohaven/src/display/eh_background.c
SRC += keyboards/ergohaven/src/display/eh_startup_image.c
SRC += keyboards/ergohaven/src/display/eh_pictograms.c
SRC += keyboards/ergohaven/src/display/eh_logo.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_sans_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_sans_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_sans_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_mono_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_mono_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_mono_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_mono_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_mono_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_mono_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_sans_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_sans_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_sans_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_serif_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_serif_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_serif_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_mono_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_mono_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_mono_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_sans_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_sans_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_sans_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_serif_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_serif_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_serif_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_narrow_28.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_narrow_40.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_narrow_48.c

SRC += keyboards/ergohaven/ergohaven_main.c
SRC += keyboards/ergohaven/src/eh_ruen.c
SRC += keyboards/ergohaven/hid.c
SRC += keyboards/ergohaven/src/eh_pointing.c
SRC += keyboards/ergohaven/src/eh_settings.c

OPT_DEFS += -DEH_DATE_SETTINGS_ENABLE
SRC += keyboards/ergohaven/src/display/eh_date_settings.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_montserrat_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_sans_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_ubuntu_mono_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_mono_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_sans_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_serif_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_dejavu_mono_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_sans_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_serif_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_liberation_narrow_64.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_montserrat_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_ubuntu_sans_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_ubuntu_mono_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_liberation_mono_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_dejavu_sans_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_dejavu_serif_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_dejavu_mono_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_liberation_sans_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_liberation_serif_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_date_liberation_narrow_20.c

SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_montserrat_28.c

SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_montserrat_40.c

SRC += keyboards/ergohaven/src/display/fonts/eh_font_clock_montserrat_48.c

# Dedicated asset transport; the legacy Vial interface stays unchanged.
OPT_DEFS += -DEH_FAST_UPLOAD_ENABLE
