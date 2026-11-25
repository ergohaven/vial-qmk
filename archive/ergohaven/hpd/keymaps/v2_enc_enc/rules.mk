VIAL_ENABLE = yes
OPT_DEFS += -DEH_HPD_FACTORY_LAYER_NAMES -DEH_ENCODER_SETTINGS
SRC += keyboards/ergohaven/src/eh_encoder.c
ENCODER_ENABLE = yes
ENCODER_MAP_ENABLE = yes

RGBLIGHT_ENABLE = yes
RGBLIGHT_DRIVER = ws2812
WS2812_DRIVER = vendor

SRC += keyboards/ergohaven/ergohaven_rgb.c
AUTO_SHIFT_ENABLE = yes
