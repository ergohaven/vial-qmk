#pragma once
#include "quantum.h"

#define EH_VERSION_STR "4.0.2"

#ifndef EH_SHORT_PRODUCT_NAME
#    define EH_SHORT_PRODUCT_NAME PRODUCT
#endif

#define _BASE 0
#define _LOWER 1
#define _RAISE 2
#define _ADJUST 3
#define _FOUR 4
#define _FIVE 5
#define _SIX 6
#define _SEVEN 7
#define _EIGHT 8
#define _NINE 9
#define _TEN 10
#define _ELEVEN 11
#define _TWELVE 12
#define _THIRTEEN 13
#define _FOURTEEN 14
#define _FIFTEEN 15

#define BASE      DF(_BASE)
#define RAISE     MO(_RAISE)
#define LOWER     MO(_LOWER)
#define ADJUST    MO(_ADJUST)
#define PREVWRD   LCTL(KC_LEFT)
#define NEXTWRD   LCTL(KC_RIGHT)
#define CTRLUP    LCTL(KC_UP)
#define CTRLDN    LCTL(KC_DOWN)
#define LANG      LALT(KC_LSFT)

#define CT_Q      LCTL_T(KC_Q)
#define CT_CM     RCTL_T(KC_COMM)
#define SF_Z      LSFT_T(KC_Z)
#define SF_SS     RSFT_T(KC_SLSH)
#define AL_X      LALT_T(KC_X)
#define AL_DT     RALT_T(KC_DOT)
#define LO_TB     LT(LOWER, KC_TAB)

#define APP_A     LSG(KC_A)
#define APP_S     LSG(KC_S)
#define APP_D     LSG(KC_D)
#define APP_F     LSG(KC_F)
#define APP_G     LSG(KC_G)
#define APP_X     LSG(KC_X)
#define APP_C     LSG(KC_C)
#define APP_V     LSG(KC_V)
#define APP_B     LSG(KC_B)

enum custom_keycodes {
    EH_PRINFO = QK_KB,
    WRD_NXT,
    WRD_PRV,
    WNEXT,
    WPREV,
    LAYER_NEXT,
    LAYER_PREV,
    EH_RSRV1,

    EH_RSRV2 = QK_KB + 51,
    EH_RSRV3,
    EH_RSRV4,
    EH_RSRV5,
    EH_RSRV6,
    EH_RSRV7,
};

// 1st layer on the cycle
#define LAYER_CYCLE_START 0
// Last layer on the cycle
#define LAYER_CYCLE_END   15

uint8_t get_current_layer(void);

uint8_t split_get_lang(void);

bool split_get_mac(void);

bool split_get_caps_word(void);
