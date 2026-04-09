#pragma once

#define EH_DISPLAY_TIMEOUT_SPLASH_SCREEN (1500)
#define EH_DISPLAY_TIMEOUT_VOLUME_SCREEN (1500)
#define EH_DISPLAY_TIMEOUT_ACTIVITY (10 * 1000)

#ifdef EH_HAS_DISPLAY

#    include "qp.h"

extern lv_color_t accent_color_red;
extern lv_color_t accent_color_blue;

extern lv_style_t style_screen;
extern lv_style_t style_container;
extern lv_style_t style_button;
extern lv_style_t style_button_active;

bool display_init_kb(void);

__attribute__((weak)) void display_init_styles_kb(void);

__attribute__((weak)) void display_init_screens_kb(void);

void display_turn_on(void);

void display_turn_off(void);

bool is_display_enabled(void);

const char *get_layer_label(uint8_t layer);

const char *get_layout_label(uint8_t layer);

/* Common screens */

typedef struct {
    void (*init)(void);
    void (*load)(void);
    void (*housekeep)(void);
} eh_screen_t;

extern const eh_screen_t eh_screen_splash;
extern const eh_screen_t eh_screen_volume;
extern const eh_screen_t eh_screen_home;

#endif
