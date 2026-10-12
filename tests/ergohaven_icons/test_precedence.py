#!/usr/bin/env python3
"""Characterize real pictogram storage/getter and the production display setter."""
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
static const uint8_t ordinary[154]={4};
static const uint8_t *rendered;
static uint8_t render_width;
static bool integrated,integration_drawn;
static bool hid_app_layout_get_visual(uint8_t l,uint8_t c,uint8_t *v,uint8_t *s){(void)l;(void)c;*v=5;*s=1;return integrated;}
static void screen_layout_set_integration_icon(uint8_t i,uint8_t v,uint8_t s,uint8_t f){(void)i;(void)v;(void)s;(void)f;integration_drawn=true;}
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
static void packet(uint8_t *p) { assert(eh_pictograms_process_hid(p,32)); assert(p[1]==0); }
static void upload(uint8_t kind,uint16_t slot,const uint8_t *art) {
 uint8_t record[EH_PICTOGRAM_RECORD_SIZE]={0}; memcpy(record+4,art,EH_PICTOGRAM_BYTES);
 uint8_t p[32]={0xc7};p[1]=kind;write_u16(p+2,slot);p[4]=1;write_u32(p+5,crc32(record,sizeof(record)));packet(p);
 for(unsigned offset=0,seq=0;offset<sizeof(record);offset+=29,seq++) {
  memset(p,0,32);p[0]=0xc8;write_u16(p+1,seq);memcpy(p+3,record+offset,MIN(29,sizeof(record)-offset));packet(p);
 }
 memset(p,0,32);p[0]=0xc9;packet(p);
 eh_pictograms_init();assert(valid);
}
static void draw(uint16_t k,const uint8_t *expected,uint8_t width) {
 rendered=NULL;screen_layout_set_key_content(0,0,0,k);assert(rendered==expected);
 if(expected) assert(render_width==width);
}
int main(void){
 assert(!memcmp(layer_prev_icon,approved_prev,154));assert(!memcmp(layer_next_icon,approved_next,154));
 memset(mock_flash,255,sizeof(mock_flash));eh_pictograms_init();
 assert(!eh_pictogram_for_keycode(QK_MACRO));assert(!eh_pictogram_for_keycode(QK_TAP_DANCE));
 for(unsigned kind=0;kind<2;kind++) for(unsigned edge=0;edge<2;edge++) {
  uint16_t base=kind?QK_TAP_DANCE:QK_MACRO,max=kind?QK_TAP_DANCE_MAX:QK_MACRO_MAX;
  uint16_t slot=edge?max-base:0,k=base+slot;
  const uint8_t *art=edge?layer_next_icon:layer_prev_icon;
  upload(kind,slot,art);const uint8_t *saved=eh_pictogram_for_keycode(k);
  assert(saved && !memcmp(saved,art,154));assert(eh_pictogram_stored_width()==35);draw(k,saved,35);
  integrated=true;integration_drawn=false;draw(k,NULL,0);assert(integration_drawn && label_visual[0]==5);integrated=false;
 }
 upload(0,255,layer_next_icon);assert(!eh_pictogram_for_keycode(QK_MACRO+255));
 for(unsigned kind=0;kind<3;kind++) {
  uint8_t bad[32]={0xc7};bad[1]=kind;write_u16(bad+2,kind==2?0:256);bad[4]=1;
  assert(eh_pictograms_process_hid(bad,32));assert(bad[1]==EH_PICTOGRAM_STATUS_BAD_FORMAT);
 }
 assert(!eh_pictogram_for_keycode(QK_MACRO+1));assert(!eh_pictogram_for_keycode(QK_TAP_DANCE+254));
 const uint16_t outside[]={QK_MACRO-1,QK_MACRO_MAX+1,QK_TAP_DANCE-1,QK_TAP_DANCE_MAX+1,LAYER_PREV,LAYER_NEXT,200,KC_1};
 for(unsigned i=0;i<sizeof(outside)/sizeof(outside[0]);i++) assert(!eh_pictogram_for_keycode(outside[i]));
 draw(LAYER_PREV,layer_prev_icon,35);draw(LAYER_NEXT,layer_next_icon,35);draw(200,ordinary,35);draw(KC_1,NULL,0);
 integrated=true;integration_drawn=false;draw(LAYER_NEXT,NULL,0);assert(integration_drawn);integrated=false;
 /* Legacy v3 is readable, not slot-writable. Seed the historical package at
  * the hardware boundary; do not pretend that the v4 upload accepts 32px. */
 memset(mock_flash,255,sizeof(mock_flash));
 eh_pictogram_header_t *h=(void*)(mock_flash+EH_PICTOGRAM_FLASH_OFFSET);memset(h,0,sizeof(*h));
 h->magic=EH_PICTOGRAM_MAGIC;h->version=3;h->width=h->height=32;h->bytes_per_icon=128;h->total_size=256+512*132;
 for(unsigned kind=0;kind<2;kind++) for(unsigned edge=0;edge<2;edge++) {
  unsigned slot=edge?(unsigned)((kind?QK_TAP_DANCE_MAX:QK_MACRO_MAX)-(kind?QK_TAP_DANCE:QK_MACRO)):0;uint8_t *bits=kind?h->tap_dance_valid:h->macro_valid;bits[slot/8]|=1u<<(slot%8);
  uint8_t *record=mock_flash+EH_PICTOGRAM_FLASH_OFFSET+256+(kind*256+slot)*132;
  memcpy(record+4,edge?legacy_next:legacy_prev,128);
 }
 h->data_crc32=crc32(stored_payload,h->total_size-256);h->header_crc32=header_crc((const uint8_t*)h);eh_pictograms_init();assert(valid);
 for(unsigned kind=0;kind<2;kind++) for(unsigned edge=0;edge<2;edge++) {
  uint16_t k=edge?(kind?QK_TAP_DANCE_MAX:QK_MACRO_MAX):(kind?QK_TAP_DANCE:QK_MACRO);const uint8_t *saved=eh_pictogram_for_keycode(k);
  assert(saved && !memcmp(saved,edge?legacy_next:legacy_prev,128));draw(k,saved,32);
 }
 uint8_t p[32]={0xc7};p[4]=1;assert(eh_pictograms_process_hid(p,32));assert(p[1]==EH_PICTOGRAM_STATUS_BAD_FORMAT);
 for(unsigned i=0;i<sizeof(outside)/sizeof(outside[0]);i++) assert(!eh_pictogram_for_keycode(outside[i]));
 draw(LAYER_PREV,layer_prev_icon,35);draw(LAYER_NEXT,layer_next_icon,35);
 puts("real v4 upload/getter: Macro 0/127, TD 0/255 approved artwork; v3 32px read-only; layer NULL/builtin; integration priority: PASS");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='icon-priority-') as tmp:
    p = Path(tmp)
    # Reuse the native runner's flash fault model and hardware header setup,
    # stopping before its test dispatch (no native suites executed by exec).
    runner = (ROOT / 'tests/ergohaven/run_native.py').read_text()
    setup, dispatch = runner.split('out=[]', 1)
    assert dispatch.startswith('\n# Entire unchanged')
    args = sys.argv
    try:
        sys.argv = [str(ROOT / 'tests/ergohaven/run_native.py'), str(p)]
        native: dict = {'__file__': str(ROOT / 'tests/ergohaven/run_native.py')}
        exec(compile(setup, 'native-hardware-setup', 'exec'), native)
    finally:
        sys.argv = args
    storage_path = 'keyboards/ergohaven/src/display/eh_pictograms.c'
    storage = (subprocess.check_output(['git','show',args[1]+':'+storage_path],cwd=ROOT).decode()
               if len(args)>1 else (ROOT / storage_path).read_text())
    # Replace includes only: every storage/getter/cache/helper body stays real.
    storage = storage.replace('#include "eh_pictograms.h"', '#include "' + str(ROOT / 'keyboards/ergohaven/src/display/eh_pictograms.h') + '"')
    constants = '\n#undef QK_MACRO\n#undef QK_MACRO_MAX\n#undef QK_TAP_DANCE\n#undef QK_TAP_DANCE_MAX\n'
    constants += '#include "' + str(ROOT / 'quantum/keycodes.h') + '"\n'
    constants += '#include "' + str(ROOT / 'keyboards/ergohaven/ergohaven.h') + '"\n'
    def array(name, data):
        return 'static const uint8_t '+name+'[]={'+','.join(map(str,data))+'};\n'
    art = '#include "' + str(ROOT / 'keyboards/ergohaven/macropad/layer_navigation_icons.h') + '"\n'
    for direction in ('prev','next'):
        bits = (Path(__file__).parent / 'golden' / f'layer_{direction}_icon.bin').read_bytes()
        art += array(f'approved_{direction}', bits)
        # Deterministic top-left 32x32 crop, not a historical approved 32px master.
        legacy = bytearray(128)
        for y in range(32):
            for x in range(32):
                if bits[(y*35+x)//8] & (0x80 >> ((y*35+x)%8)):
                    legacy[(y*32+x)//8] |= 0x80 >> ((y*32+x)%8)
        art += array(f'legacy_{direction}', legacy)
    text = native['common'] + constants + storage + art + harness
    text += '\nstatic void screen_layout_set_key_content' + setter + checks
    native['run']('precedence', text, ['-DPICO_FLASH_SIZE_BYTES=4194304','-DEH_PICTOGRAM_ENABLE'])
