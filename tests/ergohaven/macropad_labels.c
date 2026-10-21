#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
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
#include "backspace_delete_icons.h"
LV_FONT_DECLARE(eh_font_montserrat_20);
LV_FONT_DECLARE(eh_font_montserrat_28);
static lv_color_t accent_color_blue,display_background_color;
#ifdef EH_APP_LAYOUT_ENABLE
static bool hid_app_layout_get_visual(uint8_t l,uint8_t c,uint8_t *v,uint8_t *s){(void)l;(void)c;(void)v;(void)s;return false;}
static void screen_layout_set_integration_icon(uint8_t i,uint8_t v,uint8_t s,uint8_t f){(void)i;(void)v;(void)s;(void)f;assert(false);}

static uint32_t timer_read32(void){return 0;}
#endif
PRODUCTION
static lv_color_t framebuffer[240*280];
static void flush(lv_disp_drv_t *drv,const lv_area_t *a,lv_color_t *colors){
    for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++){
        if(x>=0 && x<240 && y>=0 && y<280)framebuffer[y*240+x]=*colors;
        colors++;
    }
    lv_disp_flush_ready(drv);
}
static void check_pixels(void){
    unsigned ink=0;
    for(int y=0;y<280;y++)for(int x=0;x<240;x++)if(framebuffer[y*240+x].full){
        ink++;
        assert(x>=23 && x<=96 && y>=31 && y<=73);
    }
    assert(ink>0);
}
static void check_label(uint16_t kc, bool single_line){
    memset(label_text[1],0x5a,sizeof(label_text[1]));
    screen_layout_set_key_content(0,0,0,kc);
    assert(label_text[1][0]==0x5a);
    lv_obj_update_layout(lv_scr_act());
    const char *text=lv_label_get_text(key_labels[0]);
    printf("keycode=%04x label=[%s] size=%dx%d\n",kc,text,lv_obj_get_width(key_labels[0]),lv_obj_get_height(key_labels[0]));fflush(stdout);
    if(single_line){
        assert(!strchr(text,'\n'));
        assert(!strchr(text,'+'));
        assert(lv_label_get_long_mode(key_labels[0])==LV_LABEL_LONG_CLIP);
        assert(lv_obj_get_width(key_labels[0])>0);
        assert(lv_obj_get_height(key_labels[0])==eh_font_montserrat_20.line_height);
        unsigned zoom=lv_obj_get_style_transform_zoom(key_labels[0],0);
        assert(lv_obj_get_width(key_labels[0])*zoom<=66U*256U);
        assert(lv_obj_get_style_text_font(key_labels[0],0)==&eh_font_montserrat_20);
    }
    lv_refr_now(NULL);
    check_pixels();
    if(single_line){
        lv_color_t before[240*280];memcpy(before,framebuffer,sizeof(before));
        for(unsigned i=0;i<5;i++){
            lv_tick_inc(300);lv_timer_handler();lv_refr_now(NULL);check_pixels();
            assert(!memcmp(before,framebuffer,sizeof(before))); // no scrolling in either axis
        }
    }
}
int main(void){
    lv_init();
    accent_color_blue=lv_color_white();display_background_color=lv_color_black();
    static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*30];
    lv_disp_draw_buf_init(&buf,pixels,NULL,240*30);
    static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.flush_cb=flush;drv.draw_buf=&buf;lv_disp_t *disp=lv_disp_drv_register(&drv);
    lv_disp_set_theme(disp,NULL);lv_obj_remove_style_all(lv_scr_act());lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);lv_obj_set_style_bg_opa(lv_scr_act(),LV_OPA_COVER,0);
    lv_obj_t *cell=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(cell);lv_obj_set_pos(cell,23,31);lv_obj_set_size(cell,74,43);
    key_labels[0]=lv_label_create(cell);lv_obj_center(key_labels[0]);lv_obj_set_style_text_font(key_labels[0],&eh_font_montserrat_20,0);lv_obj_set_style_text_align(key_labels[0],LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(key_labels[0],lv_color_white(),0);
    key_icons[0]=lv_img_create(cell);lv_obj_center(key_icons[0]);
    const uint16_t delete_keys[]={KC_BACKSPACE,KC_DELETE};
    const uint8_t *expected_icons[]={expected_backspace,expected_delete};
    for (unsigned i=0;i<2;i++) {
        const uint8_t *actual=screen_layout_builtin_key_icon(delete_keys[i]);
        assert(actual && "Backspace/Delete must use a built-in bitmap, not a font glyph");
        assert(!memcmp(actual,expected_icons[i],154));
        screen_layout_set_key_content(0,0,0,delete_keys[i]);
        assert(lv_obj_has_flag(key_labels[0],LV_OBJ_FLAG_HIDDEN));
        assert(!lv_obj_has_flag(key_icons[0],LV_OBJ_FLAG_HIDDEN));
        assert(key_icon_dsc[0].header.w==EH_ICON_RENDER_SIZE && key_icon_dsc[0].header.h==EH_ICON_RENDER_SIZE);
        uint8_t expected_render[EH_ICON_RENDER_BYTES];
        eh_render_pictogram(expected_icons[i],35,expected_render);
        assert(!memcmp(key_icon_bits[0],expected_render,sizeof(expected_render)));
    }
    assert(memcmp(expected_backspace,expected_delete,154));
    puts("BACKSPACE/DELETE PASS: unchanged Backspace, exact approved mirrored Delete; same native canvas and production setter");
#ifndef EH_APP_LAYOUT_ENABLE
    // Default rev3 keymaps retain the shared formatter's modifier/newline/key.
    const uint16_t multiline[]={LCTL(KC_M),RCTL(KC_M),LCTL(KC_Z),LGUI(KC_M)};
    for(unsigned i=0;i<sizeof(multiline)/sizeof(*multiline);i++){
        check_label(multiline[i],false);
        assert(strchr(lv_label_get_text(key_labels[0]),'\n'));
        assert(lv_obj_get_height(key_labels[0])>=2*eh_font_montserrat_20.line_height);
        assert(lv_label_get_long_mode(key_labels[0])==LV_LABEL_LONG_WRAP);
        assert(lv_obj_get_style_transform_zoom(key_labels[0],0)==256);
        unsigned lower_ink=0;
        for(int y=53;y<=73;y++)for(int x=23;x<=96;x++)lower_ink+=framebuffer[y*240+x].full!=0;
        assert(lower_ink>0 && "base key must remain visible on the second row");
    }
    check_label(KC_1,false);
    assert(lv_obj_get_style_text_font(key_labels[0],0)==&eh_font_montserrat_28);
    puts("DEFAULT LABEL PASS: multiline modifier and base key visible without application-layout feature");
#else
    // Ctrl+Z has a special arrow caption, unlike the basic KC_UNDO glyph.
    // It must pass the same one-line, clipped/static pixel assertions.
    check_label(LCTL(KC_Z),true);
    assert(strstr(lv_label_get_text(key_labels[0])," Z"));
    check_label(LCTL(KC_M),true);
    assert(strstr(lv_label_get_text(key_labels[0])," M"));
    const uint16_t combos[]={RCTL(KC_M),LSFT(KC_M),RSFT(KC_M),LALT(KC_M),RALT(KC_M),LGUI(KC_M),RGUI(KC_M),LCTL(LSFT(KC_M)),MEH(KC_M),HYPR(KC_M),LCTL(KC_PAGE_DOWN),LCTL(KC_AUDIO_VOL_UP),MEH(KC_ALTERNATE_ERASE)};
    for(unsigned i=0;i<sizeof(combos)/sizeof(*combos);i++)check_label(combos[i],true);
    test_os=OS_MACOS;check_label(LGUI(KC_M),true);assert(!strcmp(lv_label_get_text(key_labels[0]),"CMD M"));
    check_label(LALT(KC_M),true);assert(!strcmp(lv_label_get_text(key_labels[0]),"OPT M"));
    check_label(LGUI(LALT(LSFT(KC_M))),true);
    test_os=OS_LINUX;
    char left[64],right[64];get_keycode_str(left,LSFT(KC_M));get_keycode_str(right,RSFT(KC_M));assert(!strcmp(left,right));
    get_keycode_str(left,LCTL(KC_M));get_keycode_str(right,RCTL(KC_M));assert(!strcmp(left,right));
    get_keycode_str(left,KC_M);assert(!strcmp(left,"M"));check_label(KC_M,false);
    assert(lv_obj_get_width(key_labels[0])<74);
    assert(lv_obj_get_style_transform_zoom(key_labels[0],0)==256);
    get_keycode_str(left,LCTL_T(KC_M));assert(strchr(left,'\n'));
    get_keycode_str(left,OSM(MOD_LSFT));assert(strchr(left,'\n'));
    check_label(KC_1,false);assert(lv_obj_get_style_text_font(key_labels[0],0)==&eh_font_montserrat_28);
    screen_layout_set_key_content(0,0,0,LCTL(KC_C));assert(lv_obj_has_flag(key_labels[0],LV_OBJ_FLAG_HIDDEN));
    check_label(LCTL(KC_Z),true);assert(!lv_obj_has_flag(key_labels[0],LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_style_transform_zoom(key_labels[0],0)==256);
    test_os=OS_MACOS;check_label(LGUI(KC_Z),true);
    assert(!strcmp(lv_label_get_text(key_labels[0]),"CMD Z"));
    puts("SPACING/UNDO PASS: modifier gap, complete arrow/control/Z single line, no clipped second row");
    puts("BUG-004 PASS: static single-line shortcuts without plus separators; identical frames over time, scaled glyph bounds inside 74x43 tile, Linux/Mac and ordinary/icon controls");
#endif
}
