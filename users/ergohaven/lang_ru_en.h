#pragma once
#include "ergohaven.h"

enum lang_ru_en_custom_keycodes {
    LG_TOGGLE = LAYER_PREV + 2,
    LG_SYNC,
    LG_SET_EN,
    LG_SET_RU,
    LG_SET_M30,
    LG_SET_M31,
    LG_SET_SFT_ALT,

    // symbols exist in russian and english layout
    LG_RU_EN_START,
    LG_DOT = LG_RU_EN_START,
    LG_COMMA,
    LG_SCLN,
    LG_COLON,
    LG_DQUO,
    LG_QUES,
    LG_SLASH,

    // symbols exist in english only layout
    LG_EN_START,
    LG_LBR = LG_EN_START,
    LG_RBR,
    LG_LCBR,
    LG_RCBR,
    LG_LT,
    LG_GT,
    LG_GRAVE,
    LG_TILD,
    LG_AT,
    LG_HASH,
    LG_DLR,
    LG_CIRC,
    LG_AMPR,
    LG_PIPE,
    LG_QUOTE,
    LG_END,
};

enum { LANG_EN = 0, LANG_RU };

enum { TG_MACRO30 = 0, TG_MACRO31, TG_SFT_ALT };

bool process_record_lang(uint16_t keycode, keyrecord_t *record);

void lang_sync_external(uint8_t lang);

uint8_t get_cur_lang(void);
