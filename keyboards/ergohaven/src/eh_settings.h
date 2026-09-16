#pragma once

#include <stdint.h>
#include <stdbool.h>

void kb_settings_init(void);

/* should override if needed */
const char *default_layer_label(uint8_t layer);

const char *layer_name(uint8_t layer);

extern bool layer_name_updated;

void kb_settings_lcd_init(void);
void kb_settings_lcd_reset(void);

uint8_t get_lcd_brightness(void);
void set_lcd_brightness(uint8_t brightness);
uint8_t get_lcd_timeout_mins(void);
void set_lcd_timeout_mins(uint8_t timeout_mins);
uint32_t get_lcd_timeout_ms(void);

uint8_t get_display_accent_red(void);
uint8_t get_display_accent_green(void);
uint8_t get_display_accent_blue(void);
void set_display_accent_red(uint8_t red);
void set_display_accent_green(uint8_t green);
void set_display_accent_blue(uint8_t blue);
uint8_t get_display_background_red(void);
uint8_t get_display_background_green(void);
uint8_t get_display_background_blue(void);
void set_display_background_red(uint8_t red);
void set_display_background_green(uint8_t green);
void set_display_background_blue(uint8_t blue);
uint8_t get_display_button_style(void);
void set_display_button_style(uint8_t style);
uint8_t get_clock_text_red(void);
uint8_t get_clock_text_green(void);
uint8_t get_clock_text_blue(void);
void set_clock_text_red(uint8_t red);
void set_clock_text_green(uint8_t green);
void set_clock_text_blue(uint8_t blue);
uint8_t get_clock_background_red(void);
uint8_t get_clock_background_green(void);
uint8_t get_clock_background_blue(void);
void set_clock_background_red(uint8_t red);
void set_clock_background_green(uint8_t green);
void set_clock_background_blue(uint8_t blue);
uint8_t get_clock_style(void);
void set_clock_style(uint8_t style);
uint8_t get_clock_size(void);
void set_clock_size(uint8_t size);
uint8_t get_clock_alignment(void);
void set_clock_alignment(uint8_t alignment);
uint8_t get_clock_delay_variant(void);
void set_clock_delay_variant(uint8_t variant);
uint32_t get_clock_delay_ms(void);
bool get_clock_colon_blink(void);
void set_clock_colon_blink(bool enabled);
uint8_t get_clock_info_red(void);
uint8_t get_clock_info_green(void);
uint8_t get_clock_info_blue(void);
void set_clock_info_red(uint8_t red);
void set_clock_info_green(uint8_t green);
void set_clock_info_blue(uint8_t blue);
bool get_clock_visible(void);
void set_clock_visible(bool visible);
uint8_t get_clock_opacity(void);
void set_clock_opacity(uint8_t opacity);
bool get_clock_info_visible(void);
void set_clock_info_visible(bool visible);
uint8_t get_clock_info_opacity(void);
void set_clock_info_opacity(uint8_t opacity);
bool get_clock_modifiers_visible(void);
void set_clock_modifiers_visible(bool visible);
uint8_t get_clock_modifiers_red(void);
uint8_t get_clock_modifiers_green(void);
uint8_t get_clock_modifiers_blue(void);
void set_clock_modifiers_red(uint8_t red);
void set_clock_modifiers_green(uint8_t green);
void set_clock_modifiers_blue(uint8_t blue);
uint8_t get_clock_modifiers_opacity(void);
void set_clock_modifiers_opacity(uint8_t opacity);
uint8_t get_clock_background_dim(void);
void set_clock_background_dim(uint8_t dim);

uint8_t get_split_lcd_brightness(void);
uint32_t get_split_lcd_timeout_ms(void);
