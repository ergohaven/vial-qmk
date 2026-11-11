#!/usr/bin/env python3
"""Actual SM62 tile/header rendering with pinned LVGL; panel/storage clocks injected."""
from pathlib import Path
from native_vial_firewall import firewall_source
import argparse,re,subprocess
from macropad_native_support import validate_lvgl,write_stubs
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--lvgl',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);validate_lvgl(a.lvgl)
stubs=a.output/'stubs';write_stubs(stubs)
def function(src,name):
 m=re.search(r'^(?:static )?[^\n;]+\b'+name+r'\([^;\n]*\)\s*\{',src,re.M);assert m,name
 e=src.index('{',m.start())+1;depth=1
 while depth: depth+=(src[e]=='{')-(src[e]=='}');e+=1
 return src[m.start():e]+'\n'
h=(ROOT/'keyboards/ergohaven/hid.c').read_text();h=h[h.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):h.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
k=(ROOT/'keyboards/ergohaven/sm62/screen_keymap.c').read_text();c=(ROOT/'keyboards/ergohaven/sm62/screen_clock.c').read_text()
helper=ROOT/'keyboards/ergohaven/sm62/application_display.c'
helper=re.sub(r'^#include[^\n]*','',helper.read_text(),flags=re.M) if helper.exists() else ''
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "quantum/keycodes.h"
#include "quantum/quantum_keycodes.h"
#include "lvgl.h"
#include "src/display/eh_symbols.h"
#include "src/display/application_icons.h"
#define KEYMAP_TEXT 0xffffff
static uint32_t now=100,layer_state=1,default_layer_state=1;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static uint8_t get_current_layer(void){return get_highest_layer(layer_state|default_layer_state);}
static void layer_clear(void){layer_state=1;}
static void vial_keycode_down(uint16_t k){(void)k;}
static void vial_keycode_up(uint16_t k){(void)k;}
static void vial_keycode_tap(uint16_t k){(void)k;}
static void hid_app_layout_changed_kb(void){}
static bool raw_transparent;
static uint16_t dynamic_keymap_get_keycode(uint8_t l,uint8_t r,uint8_t c){(void)r;(void)c;return raw_transparent&&l>0?KC_TRNS:KC_A;}
static const char *layer_name(uint8_t l){(void)l;return "Persistent Default";}
static const char *hid_sm62_default_layer_name(uint8_t l){return layer_name(l);}
static uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c){return dynamic_keymap_get_keycode(l,r,c);}
'''
body=r'''
LV_FONT_DECLARE(eh_font_iosevka_18);LV_FONT_DECLARE(sm62_font_zen_medium_17);LV_FONT_DECLARE(sm62_font_zen_medium_28);
typedef struct {lv_obj_t *box,*primary,*secondary,*shifted;char primary_text[64];} key_widgets_t;
static struct {uint8_t cyrillic_enabled;} settings;
static const typeof(settings) *sm62_local_display_settings(void){return &settings;}
static uint8_t global_row(bool left,uint8_t row){return row+(left?0:5);}
static bool physical_key_exists(bool l,uint8_t r,uint8_t c){(void)l;return r<5&&c<7;}
static const char *secondary_legend(bool l,uint8_t r,uint8_t c,uint16_t k){(void)l;(void)r;(void)c;(void)k;return "Cyr";}
static const char *shifted_legend(uint16_t k){(void)k;return "";}
static bool keycode_is_layer_switch(uint16_t k){return IS_QK_MOMENTARY(k);}
static void get_keycode_str(char *s,uint16_t k){snprintf(s,64,"%c",'A'+k-KC_A);}
static lv_obj_t *label_layer_number,*label_layer_name;
static uint8_t displayed_layer=255;
static char displayed_layer_name[64];
'''
body=body.replace('typedef struct {lv_obj_t *box,*primary,*secondary,*shifted;char primary_text[64];} key_widgets_t;',k[k.index('typedef struct {'):k.index('} key_widgets_t;')+len('} key_widgets_t;')])
body+=''.join(function(k,n) for n in ['text_has_icon','text_codepoints','display_keycode_text','update_widget_content'])+function(c,'update_layer')
test=r'''
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){(void)a;(void)p;lv_disp_flush_ready(d);}
int main(void){
 lv_init();static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*30];lv_disp_draw_buf_init(&buf,pixels,0,240*30);
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.draw_buf=&buf;drv.flush_cb=flush;lv_disp_drv_register(&drv);
 lv_obj_t *screen=lv_obj_create(NULL);lv_scr_load(screen);
 key_widgets_t w={0};w.box=lv_obj_create(screen);lv_obj_set_size(w.box,75,75);w.primary=lv_label_create(w.box);w.secondary=lv_label_create(w.box);w.shifted=lv_label_create(w.box);
 label_layer_number=lv_label_create(screen);label_layer_name=lv_label_create(screen);
 app_layout_active=true;app_layout_sync_time=now;strcpy(app_layout_layer_names[15],"App Last Layer");layer_state=1u<<15;
 for(unsigned l=0;l<16;l++)for(unsigned n=0;n<62;n++)app_layout_keycodes[l][n]=KC_B;
 unsigned checks=0;
 for(unsigned side=0;side<2;side++)for(unsigned usb=0;usb<2;usb++){
  (void)usb;update_widget_content(&w,side,15,4,3);assert(strcmp(lv_label_get_text(w.primary),"B")==0);checks++;
 }
 update_layer(false);assert(strcmp(lv_label_get_text(label_layer_name),"App Last Layer")==0);checks++;
 app_layout_keycodes[15][61]=KC_TRNS;app_layout_keycodes[14][61]=KC_C;
 update_widget_content(&w,false,15,4,3);assert(strcmp(lv_label_get_text(w.primary),"C")==0);checks++;
 app_layout_keycodes[14][61]=0xffff;update_widget_content(&w,false,15,4,3);assert(strcmp(lv_label_get_text(w.primary),"A")==0);checks++;
 app_layout_keycodes[15][61]=KC_NO;app_layout_visuals[15][61]=1;
 update_widget_content(&w,false,15,4,3);assert(lv_obj_has_flag(w.primary,LV_OBJ_FLAG_HIDDEN));checks++;
 assert(w.icon&&!lv_obj_has_flag(w.icon,LV_OBJ_FLAG_HIDDEN));
 uint8_t empty[EH_ICON_RENDER_BYTES]={0},before[EH_ICON_RENDER_BYTES];assert(memcmp(empty,w.icon_bits,sizeof(empty))!=0);checks++;
 app_layout_execution_states[15][61]=1;update_widget_content(&w,false,15,4,3);memcpy(before,w.icon_bits,sizeof(before));now+=120;update_widget_content(&w,false,15,4,3);assert(memcmp(before,w.icon_bits,sizeof(before))!=0);checks++;
 app_layout_execution_states[15][61]=2;update_widget_content(&w,false,15,4,3);memcpy(before,w.icon_bits,sizeof(before));app_layout_execution_states[15][61]=3;update_widget_content(&w,false,15,4,3);assert(memcmp(before,w.icon_bits,sizeof(before))!=0);checks++;
 app_layout_visuals[15][61]=0;app_layout_keycodes[15][61]=KC_B;update_widget_content(&w,false,15,4,3);assert(!lv_obj_has_flag(w.primary,LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(w.icon,LV_OBJ_FLAG_HIDDEN)&&strcmp(lv_label_get_text(w.primary),"B")==0);checks++;
 now+=2501;hid_app_layout_task();update_layer(false);assert(strcmp(lv_label_get_text(label_layer_name),"Persistent Default")==0);checks++;
 raw_transparent=true;settings.cyrillic_enabled=1;update_widget_content(&w,false,15,4,3);assert(strcmp(lv_label_get_text(w.secondary),"Cyr")==0);checks++;
 lv_refr_now(NULL);printf("PASS %u actual SM62 tile/header LVGL assertions (panel/storage boundaries injected)\n",checks);
}
'''
src=a.output/'screen.c';src.write_text(pre+firewall_source(lambda path:(ROOT/path).read_text(),keycodes_included=True)+h+helper+body+test)
fonts=[ROOT/'keyboards/ergohaven/src/display/fonts/eh_font_iosevka_18.c',ROOT/'keyboards/ergohaven/macropad/sm62_font_zen_medium_17.c',ROOT/'keyboards/ergohaven/macropad/sm62_font_zen_medium_28.c']
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_LOG=1','-DLV_USE_ASSERT_MALLOC=1','-DKEYBOARD_ergohaven_sm62','-DEH_APP_LAYOUT_ENABLE',*['-I'+str(x) for x in [stubs,a.lvgl,ROOT,ROOT/'quantum',ROOT/'quantum/keymap_extras',ROOT/'quantum/sequencer',ROOT/'keyboards/ergohaven']],str(src),*map(str,fonts),*sorted(str(f) for f in (a.lvgl/'src').rglob('*.c')),'-lm','-o',str(a.output/'screen')]
subprocess.run(cmd,check=True);r=subprocess.run([str(a.output/'screen')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(a.output/'results.log').write_text(r.stdout);print(r.stdout,end='');raise SystemExit(r.returncode)
