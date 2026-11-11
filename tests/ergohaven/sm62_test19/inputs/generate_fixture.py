def strip(s):return '\n'.join(x for x in s.splitlines() if not re.match(r'\s*#\s*(include|pragma once)',x))+'\n'
def read(p):return (F/p).read_text()
def body(s,sig):
 a=s.index(sig);b=s.index('{',a)+1;d=1;e=b
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]+'\n'
h=read('keyboards/ergohaven/hid.h');h=h[h.index('typedef struct {'):h.index('hid_data_t* get_hid_data')]
common='''#pragma once
#include "lvgl.h"
#include "quantum_keycodes.h"
#include "src/display/eh_symbols.h"
#include "src/display/application_icons.h"
#include "deferred_exec.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#define KEYBOARD_ergohaven_sm62
#define SM62_DUALCORE_RENDER 1
#define SM62_RENDER_NATIVE_TEST 1
#define SM62_FAST_RETURN 1
#define SM62_EFFECT 0
#define EH_HAS_DISPLAY
#define EH_APP_LAYOUT_ENABLE
#define EH_VERSION_STR "native"
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define LANG_EN 0
#define LANG_RU 1
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define dprint(...) ((void)0)
#define qp_dprintf(...) ((void)0)
#define LCD_CS_PIN 6
#define LCD_DC_PIN 5
#define LCD_RST_PIN 4
#define QP_ROTATION_0 0
#define QP_ROTATION_180 2
#define QP_ROTATION QP_ROTATION_0
#define SM62_DISPLAY_SYNC_DELAY_MS 2500U
#include "production_geometry.h"

#define QP_LVGL_TASK_PERIOD 5
#define QP_LVGL_SPI_TRANSACTION
#define QUANTUM_PAINTER_TASK_THROTTLE 8
#define QUANTUM_PAINTER_LVGL_INTEGRATION_ENABLE
#define QUANTUM_PAINTER_DISPLAY_TIMEOUT 0
#define QP_NUM_DEVICES 1
#define TIMER_DIFF_32(a,b) ((uint32_t)((a)-(b)))
#define MOD_MASK_CTRL 0x11
#define MOD_MASK_SHIFT 0x22
#define MOD_MASK_ALT 0x44
#define MOD_MASK_GUI 0x88
typedef struct {uint16_t qsid;} qmk_settings_proto_t;
typedef void *painter_device_t;
typedef struct {bool validate_ok;uint16_t panel_width,panel_height;} painter_driver_t;
'''+h+strip(read('keyboards/ergohaven/sm62/sm62_settings.h'))+'\n#include "'+str(S/'render_exchange.h')+'"\n'+strip((S/'render_owner.h').read_text())+strip(read('keyboards/ergohaven/src/display/eh_display.h'))+'''
#include "'''+str(S/'flash_guard.h')+'''"
#include "'''+str(S/'dualcore_trace.h')+'''"
#include "'''+str(F/'quantum/sm62_trace.h')+'''"
extern painter_device_t qp_devices[1];
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t);
uint8_t get_current_layer(void),get_cur_lang(void);
bool is_keyboard_left(void),is_keyboard_master(void),is_hid_active(void),is_hid_layout_valid(void),is_hid_time_active(void),is_hid_volume_active(void);
hid_data_t *get_hid_data(void);
uint8_t matrix_get_row(uint8_t);
bool matrix_is_on(uint8_t,uint8_t);
uint16_t hid_sm62_default_keycode(uint8_t,uint8_t,uint8_t),sm62_effective_keycode(uint8_t,uint8_t,uint8_t);
const char *sm62_effective_layer_name(uint8_t),*layer_name(uint8_t);
bool hid_app_layout_get_visual(uint8_t,uint8_t,uint8_t*,uint8_t*);
uint32_t hid_app_layout_visual_generation(void),last_matrix_activity_time(void),last_matrix_activity_elapsed(void),sm62_display_boot_elapsed(void);
uint32_t get_split_lcd_timeout_ms(void);
uint8_t get_split_lcd_brightness(void);
uint8_t get_display_accent_red(void),get_display_accent_green(void),get_display_accent_blue(void),get_display_background_red(void),get_display_background_green(void),get_display_background_blue(void);
void backlight_init(void),backlight_level_noeeprom(uint8_t);
void sm62_diag_stage(uint8_t),sm62_diag_star(void);
void display_housekeeping_task(void),display_init_screens_kb(void);
uint8_t sm62_diag_screen(void);
extern bool layer_name_updated;
const char *basic_keycode_to_str(uint16_t);
void get_keycode_str(char *,uint16_t);
bool eh_keycode_str_uses_mac_modifiers(void);
painter_device_t qp_st7789_make_spi_device(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
void qp_set_viewport_offsets(painter_device_t,unsigned,unsigned);
bool qp_init(painter_device_t,unsigned),qp_power(painter_device_t,bool),qp_lvgl_attach(painter_device_t);
void qp_lvgl_detach(void),qp_lvgl_internal_tick(void),qp_internal_task(void),qp_internal_animation_tick(void);
bool qp_lvgl_refr_pending(void),qp_comms_spi_transaction_begin(painter_device_t),qp_comms_spi_transaction_end(painter_device_t);
bool qp_viewport(painter_device_t,int,int,int,int),qp_pixdata(painter_device_t,const void *,uint32_t),qp_flush(painter_device_t);
void qp_get_geometry(painter_device_t,uint16_t*,uint16_t*,void*,uint16_t*,uint16_t*);
void qp_lvgl_flush(lv_disp_drv_t*,const lv_area_t*,lv_color_t*);
void display_frame_completed(void);
void raw_hid_send(uint8_t*,uint8_t);
'''
# Actual custom keycode declarations, not a synthetic formatter alphabet.
for path,start in [('keyboards/ergohaven/ergohaven.h','enum custom_keycodes'),('keyboards/ergohaven/src/eh_ruen.h','enum ruen_custom_keycodes'),('keyboards/ergohaven/src/eh_pointing.h','enum {')]:
 t=read(path);i=t.index(start) if 'eh_pointing' not in path else t.index('enum {\n    EH_L_SNP');common+=t[i:t.index('};',i)+2]+'\n'
(O/'common.h').write_text(common)
alias=strip((S/'render_alias.h').read_text())
# Production owner/transport/diagnostics + supported native boundary hooks.
for name in ['render_owner','dualcore_trace']:
 source=strip((S/(name+'.c')).read_text())
 (O/(name+'.c')).write_text('#include "common.h"\n'+source)
# Actual renderer and policy code in distinct TUs (same static-name isolation).
for name,path in [('clock','keyboards/ergohaven/sm62/screen_clock.c'),('player','keyboards/ergohaven/sm62/screen_player.c'),('keymap','keyboards/ergohaven/sm62/screen_keymap.c'),('orbit','keyboards/ergohaven/macropad/screen_orbit.c'),('display','keyboards/ergohaven/src/display/eh_display.c'),('policy','keyboards/ergohaven/sm62/display_modes.c')]:
 helpers=''
 if name in ['clock','player']:
  helpers+='#include "keyboards/ergohaven/macropad/sm62_starfield.h"\n#include "player_fonts.h"\n'+strip(read('keyboards/ergohaven/sm62/sm62_starfield_control.h'))
 if name=='clock':helpers+=strip(read('keyboards/ergohaven/sm62/sm62_date.h'))
 if name=='keymap':helpers+='#include "application_geometry.h"\n'
 text='#include "common.h"\n'+alias+helpers+strip(read(path))
 if name=='player':text+='\nint diag_title_style(void){return lv_obj_get_style_x(label_title,0); }\nint diag_title_coord(void){return lv_obj_get_x(label_title); }\n'
 if name=='keymap':text+='\nbool native_release_visible(unsigned r,unsigned c){return released_fading[r][c];}\n'
 (O/(name+'.c')).write_text(text)
(O/'formatter.c').write_text('#include "common.h"\n'+strip(read('keyboards/ergohaven/src/display/eh_keycode_str.c')))
(O/'stars.c').write_text('#include "common.h"\n#include "keyboards/ergohaven/macropad/sm62_starfield.h"\n'+strip(read('keyboards/ergohaven/macropad/sm62_starfield.c')))
(O/'qp.c').write_text('#include "common.h"\n'+strip(read('quantum/painter/lvgl/qp_lvgl.c'))+body(read('quantum/painter/qp_internal.c'),'void qp_internal_task('))
(O/'deferred.c').write_text('#include "common.h"\n'+strip(read('quantum/deferred_exec.c')))
(O/'main.c').write_text('#include "common.h"\n'+(INPUTS/'main.c').read_text())
