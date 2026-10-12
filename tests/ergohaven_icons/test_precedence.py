#!/usr/bin/env python3
"""Compile the unmodified production setter; stub only storage/LVGL boundaries."""
from pathlib import Path
import subprocess
import tempfile
import sys
ROOT = Path(__file__).resolve().parents[2]
source = (subprocess.check_output(['git', 'show', sys.argv[1] + ':keyboards/ergohaven/macropad/screen_layout.c'], cwd=ROOT).decode() if len(sys.argv)>1 else (ROOT / 'keyboards/ergohaven/macropad/screen_layout.c').read_text())
setter = source.split('static void screen_layout_set_key_content',1)[1].split('/* A hidden screen',1)[0]
harness = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#define EH_APP_LAYOUT_ENABLE
#define LAYER_PREV 100
#define LAYER_NEXT 101
#define KC_1 1
#define KC_0 10
#define LV_OBJ_FLAG_HIDDEN 1
#define LV_PART_MAIN 0
#define EH_ICON_RENDER_SIZE 70
#define EH_ICON_RENDER_BYTES 630
#define LV_IMG_CF_ALPHA_1BIT 1
typedef struct { struct {int always_zero,w,h,cf;} header; int data_size; const uint8_t *data;} lv_img_dsc_t;
static uint8_t label_visual[1],label_execution_state[1],label_animation_frame[1],key_icon_bits[1][630];
static lv_img_dsc_t key_icon_dsc[1];
static int key_icon_colors[1],accent_color_blue,display_background_color,eh_font_montserrat_28,eh_font_montserrat_20;
static bool key_pressed[1];
static void *key_icons[1],*key_labels[1];
static char label_text[1][32];
static const uint8_t layer_prev_icon[154]={1},layer_next_icon[154]={2},custom[154]={3},ordinary[154]={4};
static const uint8_t *stored,*rendered;
static uint8_t width,render_width;
static bool integrated,integration_drawn;
static bool hid_app_layout_get_visual(uint8_t l,uint8_t c,uint8_t *v,uint8_t *s){(void)l;(void)c;*v=5;*s=1;return integrated;}
static uint32_t timer_read32(void){return 240;}
static void screen_layout_set_integration_icon(uint8_t i,uint8_t v,uint8_t s,uint8_t f){(void)i;(void)v;(void)s;(void)f;integration_drawn=true;}
static const uint8_t *eh_pictogram_for_keycode(uint16_t k){(void)k;return stored;}
static uint8_t eh_pictogram_stored_width(void){return width;}
static const uint8_t *screen_layout_builtin_key_icon(uint16_t k){return k==200?ordinary:NULL;}
static void eh_render_pictogram(const uint8_t *b,uint8_t w,uint8_t *out){(void)out;rendered=b;render_width=w;}
#define lv_obj_add_flag(a,b) ((void)(a),(void)(b))
#define lv_obj_clear_flag(a,b) ((void)(a),(void)(b))
#define lv_obj_set_style_text_font(a,b,c) ((void)(a),(void)(b),(void)(c))
#define get_keycode_str(a,b) ((void)(a),(void)(b))
#define lv_label_set_text_static(a,b) ((void)(a),(void)(b))
#define lv_img_cache_invalidate_src(a) ((void)(a))
#define lv_img_set_src(a,b) ((void)(a),(void)(b))
#define lv_obj_invalidate(a) ((void)(a))
#define lv_obj_set_style_img_recolor(a,b,c) ((void)(a),(void)(b),(void)(c))
'''
checks = r'''
int main(void){
 for(uint16_t k=LAYER_PREV;k<=LAYER_NEXT;k++){
  stored=custom; width=35; rendered=NULL; screen_layout_set_key_content(0,0,0,k); assert(rendered==custom && render_width==35);
  width=32; rendered=NULL; screen_layout_set_key_content(0,0,0,k); assert(rendered==custom && render_width==32);
  stored=NULL; rendered=NULL; screen_layout_set_key_content(0,0,0,k); assert(rendered==(k==LAYER_PREV?layer_prev_icon:layer_next_icon) && render_width==35);
  stored=custom;integrated=true;rendered=NULL;integration_drawn=false;screen_layout_set_key_content(0,0,0,k);assert(integration_drawn && rendered==NULL && label_visual[0]==5);integrated=false;
 }
 stored=NULL;rendered=NULL;screen_layout_set_key_content(0,0,0,200);assert(rendered==ordinary && render_width==35);
 stored=custom;rendered=NULL;screen_layout_set_key_content(0,0,0,200);assert(rendered==custom);
 stored=NULL;rendered=NULL;screen_layout_set_key_content(0,0,0,1);assert(rendered==NULL);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='icon-priority-') as tmp:
    p=Path(tmp); (p/'test.c').write_text(harness+'\nstatic void screen_layout_set_key_content'+setter+checks)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('production setter: saved 35/32px > built-in; integration > saved; fallback and text PASS')
