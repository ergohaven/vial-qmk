#pragma once

#define EH_DISPLAY_TIMEOUT_SPLASH_SCREEN (1500)
#define EH_DISPLAY_TIMEOUT_VOLUME_SCREEN (1500)
#define EH_DISPLAY_TIMEOUT_ACTIVITY (10 * 1000)

#ifdef EH_HAS_DISPLAY

#    include "qp.h"

extern lv_color_t accent_color_red;
extern lv_color_t accent_color_blue;
extern lv_color_t display_background_color;

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
bool display_should_wake_on_usb_resume(void);

void display_process_keyevent(uint8_t row, uint8_t col, bool pressed);

void display_process_matrix_press(void);

void display_process_encoder_event(uint8_t index, bool clockwise, uint16_t keycode);

void display_apply_accent_color(uint8_t red, uint8_t green, uint8_t blue);

void display_apply_background_color(uint8_t red, uint8_t green, uint8_t blue);

void display_apply_brightness(void);

__attribute__((weak)) void display_accent_color_changed_kb(void);

__attribute__((weak)) void display_background_color_changed_kb(void);

void display_apply_button_style(uint8_t style);
void display_apply_clock_settings(void);

__attribute__((weak)) void display_button_style_changed_kb(uint8_t style);

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
