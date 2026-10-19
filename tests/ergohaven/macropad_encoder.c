/* Production bodies are inserted by run_macropad_bugs.py. */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "quantum/keycodes.h"
#include "quantum/quantum_keycodes.h"
#define EH_HAS_DISPLAY
#define ENCODER_ENABLE
#define ENCODER_MAP_ENABLE
#define EH_APP_LAYOUT_ENABLE
EVENT_TYPES
typedef struct {keyevent_t event; uint16_t keycode;} keyrecord_t;
static uint32_t last_user_activity_timer,screen_timer;
SCREEN_STATE
static uint32_t now=100;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static bool volume_live=true;
static bool is_hid_volume_active(void){return volume_live;}
typedef struct {void (*init)(void);void (*load)(void);void (*housekeep)(void);} eh_screen_t;
static unsigned volume_loads;
static void load_volume(void){volume_loads++;}
static void noop(void){}
static const eh_screen_t eh_screen_volume={noop,load_volume,noop},eh_screen_splash={noop,noop,noop},eh_screen_home={noop,noop,noop},eh_screen_layout={noop,noop,noop};
static eh_screen_t current_screen;
static void display_turn_on(void){}
static void display_turn_off(void){}
static unsigned cw_animations,ccw_animations;
static void screen_layout_process_encoder_event(uint8_t i,bool cw){assert(i==0);if(cw)cw_animations++;else ccw_animations++;}
static uint32_t layer_state=1,default_layer_state=1;
static uint8_t get_highest_layer(uint32_t state){return 31-__builtin_clz(state);}
static void layer_clear(void){layer_state=1;}
static uint16_t tapped;
static unsigned taps;
static void vial_keycode_tap(uint16_t kc){tapped=kc;taps++;}
static uint16_t down_code,up_code;
static void vial_keycode_down(uint16_t kc){down_code=kc;}
static void vial_keycode_up(uint16_t kc){up_code=kc;}
static void display_process_keyevent(uint8_t r,uint8_t c,bool pressed){(void)r;(void)c;(void)pressed;}
static bool pre_process_record_ruen(uint16_t k,keyrecord_t *r){(void)k;(void)r;return true;}
static bool pre_process_record_user(uint16_t k,keyrecord_t *r){(void)k;(void)r;return true;}
PRODUCTION
static void reset_screen(void){screen_state=change_screen_state=SCREEN_LAYOUT;}
int main(void){
    app_layout_active=true;app_layout_sync_time=now;
    app_layout_keycodes[0][14]=KC_M;
    reset_screen();
    keyrecord_t r={.event={.key={.row=KEYLOC_ENCODER_CW,.col=0},.pressed=true,.type=ENCODER_CW_EVENT}};
    pre_process_record_kb(KC_AUDIO_VOL_UP,&r);
    printf("BUG-002: active application M, underlying QMK volume: screen=%d expected=%d\n",screen_state,SCREEN_LAYOUT);fflush(stdout);
    assert(screen_state==SCREEN_LAYOUT);
    assert(volume_loads==0);
    assert(taps==1 && tapped==KC_M);
    unsigned cases=0;
    for(unsigned cw=0;cw<2;cw++){
        r.event.key.row=cw?KEYLOC_ENCODER_CW:KEYLOC_ENCODER_CCW;
        r.event.type=cw?ENCODER_CW_EVENT:ENCODER_CCW_EVENT;
        for(unsigned layer=0;layer<3;layer++){
            layer_state=1u<<layer;
            const uint16_t codes[]={KC_M,KC_NO,KC_AUDIO_VOL_UP,KC_AUDIO_VOL_DOWN,LCTL(KC_AUDIO_VOL_UP),RSFT(KC_AUDIO_VOL_DOWN),KC_KB_VOLUME_UP,KC_KB_VOLUME_DOWN};
            for(unsigned k=0;k<sizeof(codes)/sizeof(*codes);k++){
                memset(app_layout_keycodes,0,sizeof(app_layout_keycodes));
                app_layout_keycodes[0][cw?14:13]=codes[k];
                for(unsigned l=1;l<=layer;l++) app_layout_keycodes[l][cw?14:13]=KC_TRNS;
                reset_screen();tapped=0;app_layout_clear_events();
                assert(!pre_process_record_kb(KC_AUDIO_VOL_UP,&r));
                assert(screen_state==(k>=2?SCREEN_VOLUME:SCREEN_LAYOUT));
                assert(app_layout_event_count==1 && app_layout_events[0].control==(cw?14:13) && app_layout_events[0].layer==layer);
                if(codes[k]!=KC_NO)assert(tapped==codes[k]);
                unsigned before=taps;
                screen_state=change_screen_state=SCREEN_HOME;r.event.pressed=false;
                pre_process_record_kb(KC_AUDIO_VOL_UP,&r);assert(screen_state==SCREEN_HOME && taps==before);r.event.pressed=true;
                cases++;
            }
            app_layout_stack_counts[layer]=2;app_layout_stack_selected[layer]=1;
            app_layout_stack_keycodes[layer][1][cw?1:0]=KC_M;
            reset_screen();pre_process_record_kb(KC_AUDIO_VOL_UP,&r);assert(screen_state==SCREEN_LAYOUT && tapped==KC_M);
            app_layout_stack_keycodes[layer][1][cw?1:0]=KC_AUDIO_VOL_DOWN;
            pre_process_record_kb(KC_M,&r);assert(screen_state==SCREEN_VOLUME && tapped==KC_AUDIO_VOL_DOWN);
            app_layout_stack_counts[layer]=0;
        }
    }
    assert(cw_animations && ccw_animations);
    app_layout_active=false;reset_screen();
    assert(pre_process_record_kb(KC_AUDIO_VOL_UP,&r));assert(screen_state==SCREEN_VOLUME);
    const uint16_t fallback[]={KC_M,KC_NO,KC_AUDIO_VOL_UP,KC_AUDIO_VOL_DOWN,LCTL(KC_AUDIO_VOL_UP),RSFT(KC_AUDIO_VOL_DOWN)};
    for(unsigned k=0;k<sizeof(fallback)/sizeof(*fallback);k++){
        reset_screen();assert(pre_process_record_kb(fallback[k],&r));assert(screen_state==(k>=2?SCREEN_VOLUME:SCREEN_LAYOUT));
    }
    reset_screen();volume_live=false;pre_process_record_kb(KC_AUDIO_VOL_UP,&r);assert(screen_state==SCREEN_LAYOUT);
    volume_live=true;app_layout_active=true;app_layout_sync_time=now;layer_state=1;
    app_layout_keycodes[0][14]=EH_APP_LAYOUT_UNSET_KEYCODE;
    reset_screen();assert(pre_process_record_kb(KC_AUDIO_VOL_UP,&r));assert(screen_state==SCREEN_VOLUME);
    app_layout_keycodes[0][14]=KC_M;now+=EH_APP_LAYOUT_TIMEOUT_MS;
    reset_screen();assert(pre_process_record_kb(KC_AUDIO_VOL_UP,&r));assert(screen_state==SCREEN_VOLUME && !app_layout_active);
    app_layout_active=true;app_layout_sync_time=now;
    const uint16_t chords[]={LCTL(KC_M),RCTL(KC_M),RSFT(KC_M),MEH(KC_M)};
    for(unsigned k=0;k<sizeof(chords)/sizeof(*chords);k++){
        app_layout_keycodes[0][0]=chords[k];
        assert(hid_app_layout_process_keyevent(1,0,true));assert(down_code==chords[k]);
        assert(hid_app_layout_process_keyevent(1,0,false));assert(up_code==chords[k]);
    }
    printf("BUG-002 PASS: %u direction/layer/binding combinations plus stacks, releases, expiry, UNSET, offline controls; real HID dispatch and unchanged tap codes\n",cases);
}
