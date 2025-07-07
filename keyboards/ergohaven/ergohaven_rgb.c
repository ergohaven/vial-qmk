#include "quantum.h"
#include "ergohaven.h"
#include "ergohaven_rgb.h"

const rgblight_segment_t PROGMEM layer0_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_WHITE});
const rgblight_segment_t PROGMEM layer1_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_BLUE});
const rgblight_segment_t PROGMEM layer2_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_SPRINGGREEN});
const rgblight_segment_t PROGMEM layer3_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_RED});
const rgblight_segment_t PROGMEM layer4_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_YELLOW});
const rgblight_segment_t PROGMEM layer5_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_SPRINGGREEN});
const rgblight_segment_t PROGMEM layer6_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_RED});
const rgblight_segment_t PROGMEM layer7_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_GREEN});
const rgblight_segment_t PROGMEM layer8_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_RED});
const rgblight_segment_t PROGMEM layer9_rgb[]  = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_CYAN});
const rgblight_segment_t PROGMEM layer10_rgb[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_GOLDENROD});
const rgblight_segment_t PROGMEM layer11_rgb[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_GREEN});
const rgblight_segment_t PROGMEM layer12_rgb[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_PURPLE});
const rgblight_segment_t PROGMEM layer13_rgb[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_MAGENTA});
const rgblight_segment_t PROGMEM layer14_rgb[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_CHARTREUSE});
const rgblight_segment_t PROGMEM layer15_rgb[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, HSV_ORANGE});

// Now define the array of layers. Later layers take precedence
// clang-format off
const rgblight_segment_t* const PROGMEM my_rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    layer0_rgb, layer1_rgb, layer2_rgb, layer3_rgb, layer4_rgb, layer5_rgb, layer6_rgb, layer7_rgb,
    layer8_rgb, layer9_rgb, layer10_rgb, layer11_rgb, layer12_rgb, layer13_rgb, layer14_rgb, layer15_rgb
);
// clang-format on

void keyboard_post_init_rgb(void) {
    rgblight_layers = my_rgb_layers;
}

void layer_state_set_rgb(layer_state_t state) {
    for (int layer = 0; layer <= _FIFTEEN; ++layer)
        rgblight_set_layer_state(layer, layer_state_cmp(state, layer));
}

static bool is_rgb_on = false;

void rgb_on(void) {
    if (!is_rgb_on) {
        rgblight_wakeup();
        is_rgb_on = true;
    }
}

void rgb_off(void) {
    if (is_rgb_on) {
        rgblight_suspend();
        is_rgb_on = false;
    }
}
