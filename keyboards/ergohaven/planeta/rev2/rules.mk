SERIAL_DRIVER = vendor

QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes
RGBLIGHT_ENABLE = yes
BACKLIGHT_ENABLE = yes
BACKLIGHT_DRIVER = pwm

SRC += keyboards/ergohaven/src/display/lvgl_helpers.c
SRC += keyboards/ergohaven/src/display/eh_display_modes.c
SRC += keyboards/ergohaven/src/display/eh_display.c
SRC += keyboards/ergohaven/src/display/eh_screen_splash.c
SRC += keyboards/ergohaven/src/display/eh_screen_home.c
SRC += keyboards/ergohaven/src/display/eh_screen_volume.c
SRC += keyboards/ergohaven/src/display/eh_logo.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_20.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_montserrat_28.c

SRC += keyboards/ergohaven/ergohaven_rgb.c
