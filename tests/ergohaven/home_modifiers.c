#include "quantum.h"
#include <stdio.h>
#include <assert.h>
uint32_t mock_now;
typedef union { uint8_t raw; struct { uint8_t num_lock:1,caps_lock:1,scroll_lock:1,reserved:5; }; } led_t;
typedef struct { bool visible,pressed; const char *text; } lv_obj_t;
#define LV_STATE_PRESSED 1
#define MOD_MASK_SHIFT 0x22
#define MOD_MASK_CTRL 0x11
#define MOD_MASK_ALT 0x44
#define MOD_MASK_GUI 0x88
#include "../head/keyboards/ergohaven/src/display/eh_display.h"
_Static_assert(EH_DISPLAY_TIMEOUT_ACTIVITY == 10000, "production home activity timeout");
static lv_obj_t objects[9];
static lv_obj_t *label_shift=&objects[0], *label_ctrl=&objects[1], *label_alt=&objects[2], *label_gui=&objects[3];
static lv_obj_t *label_num=&objects[4], *label_caps=&objects[5], *label_scroll=&objects[6];
static lv_obj_t *screen_home_mods=&objects[7], *screen_home_media=&objects[8];
static led_t led_state;
static uint8_t modifiers,oneshot;
static bool caps_word,mac,hid;
static unsigned updates;
led_t host_keyboard_led_state(void) { return led_state; }
bool split_get_caps_word(void) { return caps_word; }
uint8_t get_mods(void) { return modifiers; }
uint8_t get_oneshot_mods(void) { return oneshot; }
bool split_get_mac(void) { return mac; }
bool is_hid_active(void) { return hid; }
void toggle_hidden(lv_obj_t *obj, bool visible) { obj->visible=visible; }
void toggle_state(lv_obj_t *obj, unsigned state, bool pressed) { assert(state==LV_STATE_PRESSED); obj->pressed=pressed; updates++; }
void lv_label_set_text(lv_obj_t *obj, const char *text) { obj->text=text; updates++; }
/* The runner inserts the verbatim production function here. */
FUNCTION
int main(void) {
    mock_now=10000; screen_home_update_modifiers(); assert(screen_home_mods->visible && !screen_home_media->visible);
    modifiers=0xff; led_state.raw=7; screen_home_update_modifiers();
    for(unsigned i=0;i<7;i++) assert(objects[i].pressed);
    unsigned before=updates; screen_home_update_modifiers(); assert(updates==before);
    modifiers=0; oneshot=MOD_MASK_SHIFT; led_state.raw=0; caps_word=true; mac=true; screen_home_update_modifiers();
    assert(label_shift->pressed && label_caps->pressed && !label_num->pressed && !label_scroll->pressed);
    assert(!strcmp(label_gui->text,"CMD") && !strcmp(label_alt->text,"OPT"));
    oneshot=0; caps_word=false; mac=false; screen_home_update_modifiers();
    for(unsigned i=0;i<7;i++) assert(!objects[i].pressed);
    assert(!strcmp(label_gui->text,"GUI") && !strcmp(label_alt->text,"ALT"));
    hid=true; mock_now+=EH_DISPLAY_TIMEOUT_ACTIVITY; screen_home_update_modifiers(); assert(!screen_home_mods->visible && screen_home_media->visible);
    modifiers=MOD_MASK_CTRL; screen_home_update_modifiers(); assert(screen_home_mods->visible && !screen_home_media->visible);
    modifiers=0; screen_home_update_modifiers(); mock_now+=EH_DISPLAY_TIMEOUT_ACTIVITY-1; screen_home_update_modifiers(); assert(screen_home_mods->visible);
    mock_now++; screen_home_update_modifiers(); assert(!screen_home_mods->visible && screen_home_media->visible);
    hid=false; screen_home_update_modifiers(); assert(screen_home_mods->visible && !screen_home_media->visible);
    puts("shared home: all seven indicators, oneshot/caps-word, Mac labels, production 10000ms media timeout/offline and unchanged-state redraw suppression: PASS");
}
