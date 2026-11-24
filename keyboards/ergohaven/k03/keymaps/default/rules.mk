OLED_GAMING = yes

ifeq ($(strip $(OLED_ENABLE)), yes)
  SRC += font_block.c
endif

ifeq ($(strip $(OLED_GAMING)), yes)
  EXTRAINCDIRS += keyboards/ergohaven/src/oled
  SRC += keyboards/ergohaven/src/oled/game/game.c
  SRC += keyboards/ergohaven/src/oled/game/drawing.c
  SRC += keyboards/ergohaven/src/oled/game/rndgen.c
endif
AUTO_SHIFT_ENABLE = yes
