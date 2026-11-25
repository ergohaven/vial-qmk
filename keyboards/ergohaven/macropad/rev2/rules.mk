include keyboards/ergohaven/macropad/common.mk

# RP2040-Zero: preserve its 2 MiB flash and extra-flash display settings.
OPT_DEFS += -DPICO_FLASH_SIZE_BYTES=2097152 -DEH_DISPLAY_SETTINGS_FLASH
# This revision has a display backlight, but no per-layer RGB lighting.
OPT_DEFS += -DEH_QMK_SETTINGS_DISABLE_LAYER_LEDS
