VIAL_ENABLE = yes

POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = pmw3389

QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes
BACKLIGHT_ENABLE = yes
BACKLIGHT_ENABLE = yes
BACKLIGHT_DRIVER = pwm

RAW_ENABLE = yes

SRC += keyboards/ergohaven/src/display/lvgl_helpers.c
SRC += keyboards/ergohaven/src/display/eh_display_modes.c
SRC += keyboards/ergohaven/src/display/eh_display.c
SRC += keyboards/ergohaven/src/display/eh_screen_splash.c
SRC += keyboards/ergohaven/src/display/eh_screen_home.c
SRC += keyboards/ergohaven/src/display/eh_screen_volume.c
SRC += keyboards/ergohaven/src/display/eh_logo.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_28.c


SRC += keyboards/ergohaven/ergohaven_main.c
SRC += keyboards/ergohaven/src/eh_ruen.c
SRC += keyboards/ergohaven/hid.c
SRC += keyboards/ergohaven/src/eh_pointing.c
AUTO_SHIFT_ENABLE = yes
