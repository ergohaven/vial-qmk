#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "quantum_keycodes.h"
#define KEYBOARD_ergohaven_sm62
#define EH_APP_LAYOUT_ENABLE
#define EH_HAS_DISPLAY
#define COMBO_ENABLE
#define VIA_ENABLE
#define VIAL_ENABLE
#define KEY_OVERRIDE_ENABLE
#define TAP_DANCE_ENABLE
#define NO_ACTION_ONESHOT
#define NO_ACTION_LAYER
#define NO_ACTION_TAPPING
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define LAYER_LABEL_SIZE 16
#define TAPPING_TERM 200
#define TAP_CODE_DELAY 0
#define GET_TAPPING_TERM(k,r) TAPPING_TERM
#define RPC_SM62_APP_LAYOUT 1
#define MOD_BIT(k) (1u << ((k) & 7))
#define PROGMEM
#define pgm_read_word(p) (*(p))
typedef uint32_t layer_state_t;
typedef struct { uint8_t col, row; } keypos_t;
typedef struct { keypos_t key; bool pressed; uint16_t time; uint8_t type; } keyevent_t;
typedef struct keyrecord_t { keyevent_t event; uint16_t keycode; uint32_t app_layout_revision; uint8_t app_layout_layer; } keyrecord_t;
#define TICK_EVENT 0
#define KEY_EVENT 1
#define COMBO_EVENT 4
#define IS_KEYEVENT(e) ((e).type == KEY_EVENT)
#define IS_NOEVENT(e) ((e).type == TICK_EVENT)
#define MAKE_KEYPOS(r,c) ((keypos_t){.row=(r),.col=(c)})
#define MAKE_COMBOEVENT(p) ((keyevent_t){.pressed=(p),.type=COMBO_EVENT})
static uint32_t now=100, layer_state=1, default_layer_state=1;
static uint16_t defaults[16][10][7];
static unsigned direct_downs, direct_ups, via_calls, vial_calls, user_calls;
static unsigned adds[256], dels[256];
static bool keys[256];
static uint8_t mods, weak_mods;
static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t t) { return now-t; }
static uint16_t timer_read(void) { return now; }
static uint16_t timer_elapsed(uint16_t t) { return (uint16_t)(now-t); }
static uint8_t get_highest_layer(uint32_t s) { return s ? 31-__builtin_clz(s) : 0; }
static void layer_clear(void) { layer_state=1; }
static void layer_move(uint8_t l) { layer_state=1u<<l; }
static void vial_keycode_down(uint16_t k) { (void)k; direct_downs++; }
static void vial_keycode_up(uint16_t k) { (void)k; direct_ups++; }
static void vial_keycode_tap(uint16_t k) { (void)k; }
static void hid_app_layout_changed_kb(void) {}
static void add_key(uint16_t k) { if(k<256){keys[k]=true;adds[k]++;} }
static void del_key(uint16_t k) { if(k<256){keys[k]=false;dels[k]++;} }
static void register_code(uint16_t k) { add_key(k); }
static void unregister_code(uint16_t k) { del_key(k); }
static void register_code16(uint16_t k) { add_key(k); }
static void unregister_code16(uint16_t k) { del_key(k); }
static void send_keyboard_report(void) {}
static void wait_ms(uint16_t ms) { now+=ms; }
static uint8_t get_mods(void) { return mods; }
static uint8_t get_weak_mods(void) { return weak_mods; }
static void add_weak_mods(uint8_t m) { weak_mods|=m; }
static void del_weak_mods(uint8_t m) { weak_mods&=~m; }
static void clear_weak_mods(void) { weak_mods=0; }
uint8_t extract_mod_bits(uint16_t k) { return k>>8; }
void set_weak_override_mods(uint8_t m) { (void)m; }
void clear_weak_override_mods(void) {}
void set_suppressed_override_mods(uint8_t m) { (void)m; }
void clear_suppressed_override_mods(void) {}
static uint8_t read_source_layers_cache(keypos_t p) { (void)p; return get_highest_layer(layer_state); }
static uint16_t get_event_keycode(keyevent_t e, bool update) { (void)update; return defaults[get_highest_layer(layer_state)][e.key.row][e.key.col]; }
static void display_process_keyevent(uint8_t r,uint8_t c,bool p) { (void)r;(void)c;(void)p; }
static bool pre_process_record_ruen(uint16_t k,keyrecord_t *r) { (void)k;(void)r;return true; }
static bool pre_process_record_user(uint16_t k,keyrecord_t *r) { (void)k;(void)r;return true; }
static bool pre_process_record_modules(uint16_t k,keyrecord_t *r) { (void)k;(void)r;return true; }
static bool process_record_modules(uint16_t k,keyrecord_t *r) { (void)k;(void)r;return true; }
static bool process_record_user(uint16_t k,keyrecord_t *r) { (void)k;(void)r;user_calls++;return true; }
static bool process_record_via(uint16_t k,keyrecord_t *r) { (void)k;(void)r;via_calls++;return true; }
static bool process_record_vial(uint16_t k,keyrecord_t *r) { (void)k;(void)r;vial_calls++;return true; }
static void process_record(keyrecord_t *r);
static uint16_t keymap_key_to_keycode(uint8_t l, keypos_t p) { return defaults[l][p.row][p.col]; }
