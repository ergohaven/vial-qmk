#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "quantum/keycodes.h"
#include "quantum/quantum_keycodes.h"
#include "lvgl.h"
#include "quantum.h"
#include "ergohaven.h"
#include "src/eh_ruen.h"
#include "src/eh_pointing.h"
#include "src/display/eh_symbols.h"
#include "pictogram_render.h"
#include "layer_navigation_icons.h"
#include "action_icons.h"
#include "builtin_key_icons.h"
LV_FONT_DECLARE(eh_font_montserrat_20);
LV_FONT_DECLARE(eh_font_montserrat_28);
static lv_color_t accent_color_blue,display_background_color;
#define MAX(a,b) ((a)>(b)?(a):(b))
#define EH_DISPLAY_TIMEOUT_SPLASH_SCREEN 100
#define EH_DISPLAY_TIMEOUT_VOLUME_SCREEN 300
static uint32_t now=100, matrix_time;
static bool enabled=true,volume_live=true;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint32_t layer_state=1,default_layer_state=1;
static uint8_t get_highest_layer(uint32_t state){return 31-__builtin_clz(state);}
uint8_t get_current_layer(void){return get_highest_layer(layer_state|default_layer_state);}
static void layer_clear(void){}
static void vial_keycode_down(uint16_t k){(void)k;}
static void vial_keycode_up(uint16_t k){(void)k;}
static void vial_keycode_tap(uint16_t k){(void)k;}
void hid_app_layout_changed_kb(void);
HID_PRODUCTION
static bool is_display_enabled(void){return enabled;}
static bool is_hid_volume_active(void){return volume_live;}
static void last_matrix_activity_trigger(void){matrix_time=now;}
static uint32_t last_matrix_activity_time(void){return matrix_time;}
static uint32_t physical_time, lcd_timeout, clock_delay=500;
static uint32_t last_input_activity_elapsed(void){return now-physical_time;}
static uint32_t get_clock_delay_ms(void){return clock_delay;}
static uint32_t get_lcd_timeout_ms(void){return lcd_timeout;}
static bool screen_layout_has_active_key_animation(void){return false;}
static void screen_layout_process_encoder_event(uint8_t i,bool c){(void)i;(void)c;}
static void screen_layout_process_keyevent(uint8_t r,uint8_t c,bool p){(void)r;(void)c;(void)p;}
typedef struct {bool volume_changed;} hid_data_t;
static hid_data_t data;
static hid_data_t *get_hid_data(void){return &data;}
typedef struct {void (*load)(void);void (*housekeep)(void);} eh_screen_t;
static bool layer_name_updated;
static uint16_t dynamic_keymap_get_keycode(uint8_t l,uint8_t r,uint8_t c){(void)l;(void)r;(void)c;return KC_1;}
static uint16_t dynamic_keymap_get_encoder(uint8_t l,uint8_t e,bool c){(void)l;(void)e;(void)c;return KC_M;}
static const char *layer_name(uint8_t l){(void)l;return "firmware Default";}
static void display_apply_brightness(void){}
// Fixtures use no integration visuals. Fail rather than emulate that renderer.
static void screen_layout_set_integration_icon(uint8_t i,uint8_t v,uint8_t s,uint8_t f){(void)i;(void)v;(void)s;(void)f;assert(false);}
LAYOUT_PRODUCTION
static void noop(void){}
static const eh_screen_t eh_screen_layout={screen_layout_load,screen_layout_housekeep},eh_screen_home={noop,noop},eh_screen_splash={noop,noop},eh_screen_volume={noop,noop};
static void display_turn_on(void){}
static void display_turn_off(void){}
SCREEN_STATE
DISPLAY_PRODUCTION
static void flush(lv_disp_drv_t *drv,const lv_area_t *area,lv_color_t *pixels){(void)area;(void)pixels;lv_disp_flush_ready(drv);}
int main(void){
 lv_init();accent_color_blue=lv_color_white();display_background_color=lv_color_black();
 static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*30];
 lv_disp_draw_buf_init(&buf,pixels,NULL,240*30);
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.flush_cb=flush;drv.draw_buf=&buf;lv_disp_drv_register(&drv);
 screen_layout=lv_obj_create(NULL);
 label_layer=lv_label_create(screen_layout);label_layer_icon=lv_label_create(screen_layout);
 for(unsigned i=0;i<NLABELS;i++){
  key_cells[i]=lv_obj_create(screen_layout);key_labels[i]=lv_label_create(key_cells[i]);key_icons[i]=lv_img_create(key_cells[i]);
 }
 screen_state=change_screen_state=SCREEN_LAYOUT;
 char cmd;unsigned checks=0;uint32_t saved=0;
 while(scanf(" %c",&cmd)==1){
  if(cmd=='P'){
   uint8_t packet[32];unsigned x;
   for(unsigned i=0;i<32;i++){assert(scanf("%2x",&x)==1);packet[i]=x;}
   assert(hid_app_layout_process_packet(packet,32));
  }else if(cmd=='S'){
   now+=600;display_housekeeping_task();assert(screen_state==SCREEN_HOME);saved=last_user_activity_timer;
  }else if(cmd=='W'){
   display_housekeeping_task();
   printf("wake check: screen=%d expected=%d name=%s\n",screen_state,SCREEN_LAYOUT,lv_label_get_text(label_layer));fflush(stdout);
   assert(screen_state==SCREEN_LAYOUT);assert(last_user_activity_timer==now);
   const char *expected=app_layout_active?(app_layout_layer_names[0][0]?app_layout_layer_names[0]:app_layout_name):"firmware Default";
   assert(strcmp(lv_label_get_text(label_layer),expected)==0);
   assert(lv_scr_act()==screen_layout);
   for(unsigned i=0;i<NLABELS;i++){
    uint8_t control=screen_layout_control_for_label(i);
    if(app_layout_active && app_layout_keycodes[0][control]!=EH_APP_LAYOUT_UNSET_KEYCODE && app_layout_keycodes[0][control]!=KC_TRNS)assert(label_kc[i]==app_layout_keycodes[0][control]);
   }
   if(app_layout_active && app_layout_keycodes[0][0]==KC_M)assert(strcmp(lv_label_get_text(key_labels[0]),"M")==0);
   lv_refr_now(NULL);checks++;
  }else if(cmd=='N'){
   display_housekeeping_task();assert(screen_state==SCREEN_HOME);assert(last_user_activity_timer==saved);checks++;
  }else if(cmd=='D'){enabled=false;
  }else if(cmd=='E'){enabled=true;
  }else if(cmd=='U'){
   uint8_t packet[32]={0xea,7};assert(hid_app_layout_process_packet(packet,32));
   display_housekeeping_task();assert(!app_layout_active);assert(screen_state==SCREEN_HOME);assert(last_user_activity_timer==saved);checks++;
  }else if(cmd=='X'){
   now+=2501;hid_app_layout_task();display_housekeeping_task();assert(!app_layout_active);assert(screen_state==SCREEN_HOME);checks++;
  }else if(cmd=='K'){
   display_process_keyevent(1,0,true);display_housekeeping_task();assert(screen_state==SCREEN_LAYOUT);checks++;
  }else if(cmd=='R'){
   display_process_encoder_event(0,false,KC_M);assert(screen_state==SCREEN_HOME);checks++;
  }else if(cmd=='V'){
   display_process_encoder_event(0,true,KC_AUDIO_VOL_UP);assert(screen_state==SCREEN_VOLUME);
   now+=301;display_housekeeping_task();assert(screen_state==SCREEN_HOME);checks++;
  }else if(cmd=='G'){
   display_process_encoder_event(0,true,KC_AUDIO_VOL_UP);assert(screen_state==SCREEN_VOLUME);saved=last_user_activity_timer;
  }else if(cmd=='Z'){
   display_housekeeping_task();assert(screen_state==SCREEN_VOLUME);assert(last_user_activity_timer==saved);checks++;
   now+=301;display_housekeeping_task();assert(screen_state==SCREEN_LAYOUT);
   now+=600;display_housekeeping_task();assert(screen_state==SCREEN_HOME);saved=last_user_activity_timer;
  }else if(cmd=='T'){
   assert(screen_state==SCREEN_HOME);lcd_timeout=3000;clock_delay=1000;physical_time=now-2990;
  }else if(cmd=='Y'){
   uint32_t physical=physical_time,matrix=matrix_time;
   now+=501;display_housekeeping_task();
   printf("LCD near-timeout: screen=%d expected=%d physical_elapsed=%u\n",screen_state,SCREEN_LAYOUT,last_input_activity_elapsed());fflush(stdout);
   assert(screen_state==SCREEN_LAYOUT);assert(physical_time==physical);assert(matrix_time==matrix);checks++;
   now+=500;display_housekeeping_task();assert(screen_state==SCREEN_HOME);checks++;
   now+=2000;display_housekeeping_task();assert(screen_state==SCREEN_OFF);checks++;
  }else if(cmd=='O'){
   screen_state=change_screen_state=SCREEN_OFF;
  }else if(cmd=='F'){
   display_housekeeping_task();assert(screen_state==SCREEN_OFF);checks++;
  }else{assert(!"unknown command");}
 }
 if(checks==0){fputs("FAIL: no wake checks executed\n",stderr);return 1;}
 printf("PASS %u screen/standby/disabled/manual/disconnect assertions\n",checks);
 return 0;
}
