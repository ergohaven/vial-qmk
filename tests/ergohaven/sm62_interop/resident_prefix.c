
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define KC_NO 0
#define KC_TRNS 1
extern uint32_t bridge_now;
#define now bridge_now
static uint32_t layer_state=1,default_layer_state=1;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static void layer_clear(void){layer_state=1;}
static unsigned downs,ups,taps;
static uint16_t down_code,up_code;
static void vial_keycode_down(uint16_t k){downs++;down_code=k;}
static void vial_keycode_up(uint16_t k){ups++;up_code=k;}
static void vial_keycode_tap(uint16_t k){(void)k;taps++;}
static void hid_app_layout_changed_kb(void){}

#define VIAL_ENABLE
extern bool vial_unlocked;
uint16_t vial_keycode_firewall(uint16_t);
