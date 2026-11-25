VIAL_ENABLE = yes
OPT_DEFS += -DEH_HPD_FACTORY_LAYER_NAMES
ENCODER_ENABLE = yes
ENCODER_MAP_ENABLE = yes

POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = analog_joystick

RGBLIGHT_ENABLE = yes
RGBLIGHT_DRIVER = ws2812
WS2812_DRIVER = vendor

SRC += keyboards/ergohaven/ergohaven_rgb.c
SRC += keyboards/ergohaven/src/eh_pointing.c
AUTO_SHIFT_ENABLE = yes
