include keyboards/ergohaven/macropad/common.mk

# Preserve the production board's 4 MiB flash and EEPROM display settings.
OPT_DEFS += -DPICO_FLASH_SIZE_BYTES=4194304
