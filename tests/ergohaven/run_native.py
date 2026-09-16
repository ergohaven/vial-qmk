#!/usr/bin/env python3
"""Native firmware regressions using real source and mocked flash/timers/LVGL.
Usage: python3 tests/ergohaven/run_native.py /absolute/scratch-output [--only-unlock]
"""
from pathlib import Path
import subprocess, re, sys
ROOT=Path(__file__).resolve().parents[2]
ONLY_UNLOCK='--only-unlock' in sys.argv[2:]
P=Path(sys.argv[1]).resolve(); S=P/'stubs'; S.mkdir(parents=True,exist_ok=True)
def write(path,s):
 p=S/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s)
write('quantum.h',r'''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define QK_MACRO 0x7700
#define QK_MACRO_MAX 0x77ff
#define QK_TAP_DANCE 0x5700
#define QK_TAP_DANCE_MAX 0x57ff
extern uint32_t mock_now;
static inline uint32_t timer_read32(void){return mock_now;}
static inline uint32_t timer_elapsed32(uint32_t t){return mock_now-t;}
''')
write('via.h','#pragma once\n#include "quantum.h"\n#define VIAL_HID_MAGIC 0xfeedfacecafebeefULL\n')
write('hardware/flash.h',r'''#pragma once
#include <stdint.h>
#include <stddef.h>
#define FLASH_PAGE_SIZE 256u
#define FLASH_SECTOR_SIZE 4096u
void flash_range_erase(uint32_t offset,size_t size);
void flash_range_program(uint32_t offset,const uint8_t *data,size_t size);
''')
write('hardware/sync.h','#pragma once\nstatic inline uint32_t save_and_disable_interrupts(void){return 0;}\nstatic inline void restore_interrupts(uint32_t t){(void)t;}\n')
write('hardware/regs/addressmap.h','#pragma once\nextern uint8_t mock_flash[4194304];\n#define XIP_BASE ((uintptr_t)mock_flash)\n')
write('lvgl.h',r'''#pragma once
#include "quantum.h"
#define LV_UNUSED(x) (void)(x)
#define LV_IMG_SRC_VARIABLE 0
#define LV_IMG_CF_RAW_ALPHA 1
#define LV_RES_INV 1
#define LV_RES_OK 0
typedef int lv_res_t;
typedef int16_t lv_coord_t;
typedef uint16_t lv_color_t;
typedef struct {uint32_t cf,w,h;} lv_img_header_t;
typedef struct {lv_img_header_t header; uint32_t data_size; const uint8_t *data;} lv_img_dsc_t;
typedef struct {int dummy;} lv_img_decoder_t;
typedef struct {const void *src;const uint8_t *img_data;void *user_data;} lv_img_decoder_dsc_t;
static inline int lv_img_src_get_type(const void *p){(void)p;return LV_IMG_SRC_VARIABLE;}
static inline lv_color_t lv_color_make(uint8_t r,uint8_t g,uint8_t b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static inline lv_img_decoder_t *lv_img_decoder_create(void){static lv_img_decoder_t d;return &d;}
#define lv_img_decoder_set_info_cb(d,fn) ((void)(d),(void)(fn))
#define lv_img_decoder_set_open_cb(d,fn) ((void)(d),(void)(fn))
#define lv_img_decoder_set_read_line_cb(d,fn) ((void)(d),(void)(fn))
''')
common=r'''
#include "quantum.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
uint32_t mock_now=100;
uint8_t mock_flash[4194304];
/* Silent program failure is independent of power-cut/unwind injection. */
static uint32_t silent_program_address = UINT32_MAX;
static unsigned silent_program_after, silent_program_hits;
static bool silent_program_persistent;
static unsigned flash_operations, cut_after, cut_mode; static bool cut_live; static jmp_buf power_cut;
static bool cutting(uint32_t offset) {
    flash_operations++;
    return (cut_after && flash_operations==cut_after) || (cut_live && offset>=0xE0000 && offset<0xF4000);
}
void flash_range_erase(uint32_t offset,size_t size){
    assert(offset%4096==0 && size%4096==0 && offset+size<=PICO_FLASH_SIZE_BYTES);
    bool fail=cutting(offset);
    if(fail && cut_mode==1) longjmp(power_cut,1);
    memset(mock_flash+offset,255,fail && cut_mode==2?size/2:size);
    if(fail) longjmp(power_cut,1);
}
void flash_range_program(uint32_t offset,const uint8_t *data,size_t size){
    assert(offset%256==0 && size%256==0 && offset+size<=PICO_FLASH_SIZE_BYTES);
    bool fail=cutting(offset);
    if(fail && cut_mode==1) longjmp(power_cut,1);
    if (offset == silent_program_address) {
        if (silent_program_after) {
            silent_program_after--;
        } else {
            silent_program_hits++;
            if (!silent_program_persistent) silent_program_address = UINT32_MAX;
            return;
        }
    }
    size_t amount=fail && cut_mode==2?size/2:size;
    for(size_t i=0;i<amount;i++){assert((mock_flash[offset+i]&data[i])==data[i]);mock_flash[offset+i]&=data[i];}
    if(fail) longjmp(power_cut,1);
}
'''
def run(name,text,defs=()):
 if ONLY_UNLOCK and name not in {'unlock','unlock_insecure','unlock_reset_flags'}:
  return ''
 text=text.replace('../head/',str(ROOT)+'/')
 path=P/(name+'.c'); path.write_text(text)
 cmd=['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(S),*defs,str(path),'-o',str(P/name)]
 subprocess.run(cmd,check=True)
 r=subprocess.run([str(P/name)],text=True,capture_output=True)
 if r.stderr:
  print(r.stderr,file=sys.stderr,end='')
 r.check_returncode()
 print(r.stdout,end='')
 return r.stdout
out=[]
# Entire unchanged date/settings translation unit with flash and timer substitutions.
date=common+'\n#include "../head/keyboards/ergohaven/src/display/eh_date_settings.c"\n'+r'''
static void reboot(void){initialized=false;dirty=false;sequence=0;newest=-1;cut_after=0;}
int main(void){
memset(mock_flash,255,sizeof(mock_flash));assert(eh_date_get(10)==100);assert(!eh_date_set(0,2));assert(!eh_date_set(15,0));assert(eh_date_set(0,1));mock_now+=349;eh_extra_settings_housekeep();assert(newest==-1);mock_now++;eh_extra_settings_housekeep();assert(newest==0);reboot();assert(eh_date_get(0)==1);puts("settings defaults, validation, debounce, reboot persistence: PASS (4 cases)");
for(unsigned i=1;i<80;i++){assert(eh_date_set(1,i));mock_now+=350;eh_extra_settings_housekeep();reboot();assert(eh_date_get(1)==i);}puts("settings 79 saves across repeated two-sector rollover: PASS");
// Legacy journal had no standby-brightness/media bytes; preserve older choices.
record_t *legacy=(void*)(mock_flash+EXTRA_BASE+newest*FLASH_PAGE_SIZE);
memset(legacy->data+10,255,5); legacy->crc=checksum(legacy); reboot();
assert(eh_date_get(1)==79 && eh_date_get(10)==100 && eh_date_get(11)==1);
for(unsigned i=12;i<15;i++)assert(eh_date_get(i)==255);
puts("settings legacy erased fields recover defaults without losing existing fields: PASS");
uint8_t before=eh_date_get(1);eh_date_set(1,80);mock_now+=350;cut_after=flash_operations+1;if(!setjmp(power_cut))eh_extra_settings_housekeep();reboot();uint8_t after=eh_date_get(1);assert(after==before||after==80);puts("settings power cut after next flash operation recovers old/new value: PASS");
}
'''
out.append(run('settings',date,['-DPICO_FLASH_SIZE_BYTES=4194304']))
background=common+'\n#include "../head/keyboards/ergohaven/src/display/eh_background.c"\n'+r'''
static void packet(uint8_t *p){assert(eh_background_process_hid(p,32));}
static void preserve_check(void){for(unsigned i=0;i<4096;i++)assert(mock_flash[0x1fb000+i]==0x5a);for(unsigned i=0;i<16384;i++)assert(mock_flash[0x1fc000+i]==0xa5);}
static void valid_transfer(unsigned n,bool image,unsigned length){
 unsigned frame_size=image?EH_BACKGROUND_IMAGE_FRAME_SIZE:EH_BACKGROUND_ANIMATION_FRAME_SIZE;
 unsigned size=256+n*frame_size;uint8_t *pkg=malloc(size);assert(pkg);memset(pkg,0x11,size);eh_background_header_t *h=(void*)pkg;memset(h,0,256);
 h->magic=EH_BACKGROUND_MAGIC;h->version=4;h->kind=image?1:2;h->frame_count=n;h->width=240;h->height=280;h->frame_size=frame_size;h->total_size=size;
 h->data_crc32=crc32_update(0xffffffff,pkg+256,size-256)^0xffffffff;h->header_crc32=header_crc(pkg);
 uint8_t p[64]={0xb1};write_u32(p+1,size);write_u32(p+5,crc32_update(0xffffffff,pkg,size)^0xffffffff);assert(eh_background_process_hid(p,length));assert(!p[1]);
 unsigned seq=0;for(unsigned offset=0;offset<size;offset+=length-3,seq++){memset(p,0,sizeof(p));p[0]=0xb2;write_u16(p+1,seq);memcpy(p+3,pkg+offset,MIN(length-3,size-offset));assert(eh_background_process_hid(p,length));assert(!p[1]);}
 memset(p,0,sizeof(p));p[0]=0xb3;assert(eh_background_process_hid(p,length));assert(!p[1] && valid);eh_background_init();assert(valid && eh_background_frame_count()==n);for(unsigned i=0;i<n;i++){const uint8_t *f=eh_background_frame_data(i);assert(!memcmp(f,pkg+256+i*frame_size,frame_size));}preserve_check();printf("background full %s %u frame(s), %u-byte HID: CRC commit, reboot, exact frame readback, reserved sentinels PASS (%u packets)\n",image?"image":"animation",n,length,seq);free(pkg);
}
int main(void){
memset(mock_flash,255,sizeof(mock_flash));eh_background_init();assert(!valid);uint8_t p[32]={0xb0};packet(p);assert(p[13]==EH_BACKGROUND_MAX_FRAMES);puts("background erased storage and revision capability query: PASS");memset(mock_flash+0x1fb000,0x5a,4096);memset(mock_flash+0x1fc000,0xa5,16384);
for(uint32_t size=FLASH_PAGE_SIZE+EH_BACKGROUND_ANIMATION_FRAME_SIZE;size<=EH_BACKGROUND_MAX_PACKAGE_SIZE;size+=EH_BACKGROUND_ANIMATION_FRAME_SIZE){
start_upload(size,0);uint8_t chunk[61]={0};while(upload_received<size){consume_upload_bytes(chunk,MIN(61,size-upload_received));}if(upload_page_used)program_upload_page();assert(upload_received==size);preserve_check();}
printf("background flash writes stay aligned/bounded and preserve speed/EEPROM at all %u frame counts: PASS\n",EH_BACKGROUND_MAX_FRAMES);
memset(p,0,32);p[0]=0xb1;write_u32(p+1,EH_BACKGROUND_MAX_PACKAGE_SIZE+1);packet(p);assert(p[1]==2);memset(p,0,32);p[0]=0xb2;write_u16(p+1,1);uint32_t before=upload_received;packet(p);assert(p[1]==3 && upload_received==before);puts("background oversized begin and out-of-sequence packet rejected: PASS (2 cases)");
config_read_active=false;eh_background_note_config_read(2,3);assert(!eh_background_animation_paused());eh_background_note_config_read(0xfe,5);assert(!eh_background_animation_paused());eh_background_note_config_read(4,0);assert(eh_background_animation_paused());mock_now+=250;assert(!eh_background_animation_paused());puts("background matrix/unlock polling unpaused; config read 250ms pause: PASS (4 cases)");
valid_transfer(1,true,32);valid_transfer(20,false,64);if(EH_BACKGROUND_MAX_FRAMES>20)valid_transfer(21,false,64);valid_transfer(EH_BACKGROUND_MAX_FRAMES,false,32);
}
'''
for rev,size in [('rev2',2097152),('rev3',4194304)]:
 out.append(run('background_'+rev,background,['-DPICO_FLASH_SIZE_BYTES='+str(size),'-DWEAR_LEVELING_RP2040_FLASH_BASE=0x1fc000','-DEH_FAST_UPLOAD_ENABLE']))
# Extract whole function bodies verbatim; mock only LVGL and peripheral calls.
def function(path,name):
 src=(ROOT/path).read_text();m=re.search(r'^(?:static )?[a-zA-Z_][^\n;]*\b'+name+r'\([^;\n]*\)\s*\{',src,re.M)
 assert m,name
 start=m.start();pos=src.index('{',start);depth=1;end=pos+1
 while depth:
  if src[end]=='{':depth+=1
  if src[end]=='}':depth-=1
  end+=1
 return src[start:end]
brightness=r'''
#include "quantum.h"
#include <assert.h>
#include <stdio.h>
#define BACKLIGHT_LEVELS 100
#define VOLUME_SCALE 100
static void *home=(void*)1,*screen_volume=(void*)2,*current;
static bool display_enabled=true,is_display_on=true;
static struct {uint8_t brightness;} display_settings={100};
static uint8_t standby_brightness,backlight_level,previous_volume;
static struct {uint8_t volume;} hid={50};
static void *arc_volume;
bool screen_home_is_active(void){return current==home;}
uint8_t eh_date_get(uint8_t i){assert(i==10);return standby_brightness;}
void backlight_level_noeeprom(uint8_t n){backlight_level=n;}
void *display;
void qp_power(void *p,bool on){(void)p;(void)on;}
static bool volume_active=true;
bool is_hid_volume_active(void){return volume_active;}
#define get_hid_data() (&hid)
static int32_t shown_volume;
void set_displayed_volume(void *p,int32_t n){(void)p;shown_volume=n;}
void lv_anim_del(void *p,void (*cb)(void *,int32_t)){(void)p;(void)cb;}
void lv_scr_load(void *p){current=p;}
'''
for path,name in [('keyboards/ergohaven/macropad/rev3/rev3.c','get_split_lcd_brightness'),('keyboards/ergohaven/src/display/eh_display.c','display_apply_brightness'),('keyboards/ergohaven/src/display/eh_display.c','display_turn_on'),('keyboards/ergohaven/src/display/eh_screen_volume.c','screen_volume_load')]:
 brightness+='\n'+function(path,name)+'\n'
brightness+=r'''
int main(void){
for(unsigned b=0;b<=100;b+=10){current=home;standby_brightness=b;display_apply_brightness();assert(backlight_level==b);display_turn_on();screen_volume_load();assert(current==screen_volume);assert(get_split_lcd_brightness()==100);assert(backlight_level==100);printf("volume from standby %u%%: applied=%u%%, current-screen setting=%u%%\n",b,backlight_level,get_split_lcd_brightness());}
puts("volume brightness reapplied after load: PASS at all 11 standby levels, including 0%");
for(unsigned main=0;main<=100;main+=10)for(unsigned volume=0;volume<=100;volume+=10) {
 current=home; standby_brightness=0; display_settings.brightness=main; hid.volume=volume;
 display_apply_brightness(); screen_volume_load(); assert(backlight_level==main && shown_volume==(int32_t)(volume*100));
}
current=home; volume_active=false; display_apply_brightness(); screen_volume_load(); assert(current==home && backlight_level==0);
puts("volume: 121 active-brightness/host-volume pairs including zero; inactive-volume guard: PASS");
}
'''
out.append(run('volume_brightness',brightness))
# Real shared reset call chain plus real new journal: LCD reset writes only bytes 16+.
reset=common+'\n#include "../head/keyboards/ergohaven/src/display/eh_date_settings.c"\n'+r'''
#define EH_DISPLAY_SETTINGS_FLASH
#define EH_DATE_SETTINGS_ENABLE
void kb_settings_lcd_reset(void);void kb_settings_lcd_init(void);
void display_apply_accent_color(uint8_t r,uint8_t g,uint8_t b){(void)r;(void)g;(void)b;}
void display_apply_background_color(uint8_t r,uint8_t g,uint8_t b){(void)r;(void)g;(void)b;}
void display_apply_button_style(uint8_t s){(void)s;}
void display_apply_brightness(void){}void display_apply_clock_settings(void){}
void kb_settings_ruen_reset(void){} void kb_settings_layer_labels_reset(void){} void kb_settings_pointing_reset(void){} void kb_settings_split_pointing_reset(void){} void kb_settings_led_colors_reset(void){}
void kb_settings_init(void){kb_settings_lcd_init();}
'''
# Keep the actual reset body and type/constant definitions from rev3, without unrelated getters.
rev3=(ROOT/'keyboards/ergohaven/macropad/rev3/rev3.c').read_text()
reset+='\n'+rev3[rev3.index('#define MACROPAD_RGB_TIMEOUT_DEFAULT_MINS'):rev3.index('#ifndef EH_DISPLAY_SETTINGS_FLASH')]
reset+='\nstatic macropad_display_settings_t display_settings;\n'
for name in ['persist_display_settings','lcd_timeout_mins_is_valid','set_clock_info_defaults','set_clock_background_defaults','set_clock_element_defaults','set_clock_defaults','kb_settings_lcd_init','kb_settings_lcd_reset']:
 reset+='\n'+function('keyboards/ergohaven/macropad/rev3/rev3.c',name)+'\n'
reset+='\n'+function('keyboards/ergohaven/src/eh_settings.c','kb_settings_reset')
reset+=r'''
int main(void){memset(mock_flash,0xa5,sizeof(mock_flash));memset(mock_flash+EXTRA_BASE,255,2*FLASH_SECTOR_SIZE);assert(eh_date_set(0,1));assert(eh_date_set(9,0));assert(eh_date_set(10,0));mock_now+=350;eh_extra_settings_housekeep();kb_settings_reset();assert(!dirty);initialized=false;dirty=false;newest=-1;sequence=0;assert(eh_date_get(0)==0 && eh_date_get(9)==7 && eh_date_get(10)==100);for(unsigned i=0;i<15;i++)assert(eh_date_get(i)==defaults[i]);for(unsigned i=0;i<sizeof(mock_flash);i++)if(i<EXTRA_BASE||i>=EXTRA_BASE+2*FLASH_SECTOR_SIZE)assert(mock_flash[i]==0xa5);puts("keyboard-settings reset immediately persists all 15 date/standby defaults; reboot and all external assets/EEPROM preserved: PASS");}
'''
out.append(run('settings_reset',reset,['-DPICO_FLASH_SIZE_BYTES=4194304']))
# Full source storage tests run at both physical chip sizes.
for size in [2097152,4194304]:
 text=common+'\n#include "../head/keyboards/ergohaven/src/display/eh_pictograms.c"\n'+(ROOT/'tests/ergohaven/pictograms.c').read_text()
 out.append(run('pictograms_'+str(size),text,['-DPICO_FLASH_SIZE_BYTES='+str(size),'-DEH_PICTOGRAM_ENABLE']))

src=(ROOT/'quantum/vial.c').read_text()
defs='\n'.join(re.findall(r'^#define VIAL_UNLOCK_[^\n]+',src,re.M))
a=src.index('        case vial_unlock_start:'); b=src.index('        case vial_qmk_settings_query:',a)
unlock = r'''
#include "quantum.h"
#include <stdio.h>
#include <assert.h>
uint32_t mock_now;
bool holding;
bool vial_unlock_combo_active(void) { return holding; }
enum {vial_unlock_start, vial_unlock_poll, vial_lock};
'''+defs+'\n'+'\n'.join(re.findall(r'^extern bool vial_unlock(?:ed|_in_progress);$',(ROOT/'quantum/vial.h').read_text(),re.M))+'\n'+src[src.index('#ifdef VIAL_INSECURE'):src.index('#ifndef VIAL_INSECURE\nstatic uint8_t vial_unlock_combo_rows')]+'\n'+function('quantum/vial.c','vial_unlock_task')+'\nvoid command(int id) { uint8_t msg[32]={0}; switch(id) { '+src[a:b]+' } (void)msg; }\n'
# Compile real public declarations with their real definitions; assert byte flags.
assert len(re.findall(r'^extern bool vial_unlock(?:ed|_in_progress);$',(ROOT/'quantum/vial.h').read_text(),re.M))==2
unlock+='\n_Static_assert(sizeof(vial_unlocked)==sizeof(bool) && sizeof(vial_unlock_in_progress)==sizeof(bool), "shared unlock flag types");\n'
out.append(run('unlock',unlock+(ROOT/'tests/ergohaven/unlock.c').read_text()))
assert '    matrix_scan();\n#ifdef VIAL_ENABLE\n    vial_unlock_task();\n#endif' in (ROOT/'quantum/keyboard.c').read_text()
assert 'kb_settings_reset();' in function('quantum/qmk_settings.c','qmk_settings_reset')
assert 'kb_settings_reset();' in function('keyboards/ergohaven/src/eh_settings.c','eeconfig_init_kb')
out.append(run('unlock_insecure',unlock+'int main(void) { vial_unlocked=1; command(vial_unlock_start); vial_unlock_task(); command(vial_lock); assert(vial_unlocked); puts("unlock insecure build unchanged: PASS"); }',['-DVIAL_INSECURE']))
reset_flags=unlock+r"""
#define VIAL_ENABLE
#define DYNAMIC_KEYMAP_LAYER_COUNT 2
#define MATRIX_ROWS 2
#define MATRIX_COLS 2
static unsigned writes,erases;
void nvm_dynamic_keymap_erase(void) { assert(vial_unlocked); erases++; }
uint16_t keycode_at_keymap_location_raw(int layer,int row,int column) { return layer*4+row*2+column; }
void dynamic_keymap_set_keycode(int layer,int row,int column,uint16_t keycode) {
    assert(vial_unlocked && keycode==layer*4+row*2+column); writes++;
}
"""+function('quantum/dynamic_keymap.c','dynamic_keymap_reset')+r"""
int main(void) {
    for(unsigned unlocked=0;unlocked<2;unlocked++) for(unsigned active=0;active<2;active++) {
        vial_unlocked=unlocked; vial_unlock_in_progress=active;
        writes=erases=0; dynamic_keymap_reset();
        assert(vial_unlocked==unlocked && vial_unlock_in_progress==active && writes==8 && erases==1);
    }
    puts("unlock flags: actual public bool declarations/definitions; reset temporarily unlocks and restores both initial states without changing progress: PASS");
}
"""
out.append(run('unlock_reset_flags',reset_flags))
home_path='keyboards/ergohaven/src/display/eh_screen_home.c'
home=(ROOT/'tests/ergohaven/home_modifiers.c').read_text().replace('FUNCTION',function(home_path,'screen_home_update_modifiers'))
out.append(run('home_modifiers',home))
home_source=(ROOT/home_path).read_text()
assert '#ifndef EH_HOME_HIDE_MODIFIERS\n    screen_home_update_modifiers();\n#endif' in home_source
assert '#ifndef EH_HOME_HIDE_MODIFIERS\n    screen_home_mods = lv_obj_create(content);' in home_source
for rev in ['rev2','rev3']:
 assert '#define EH_HOME_HIDE_MODIFIERS' in (ROOT/f'keyboards/ergohaven/macropad/{rev}/config.h').read_text()
for consumer in ['k03pro/rules.mk','planeta/rev2/rules.mk','hpd/keymaps/v2_lcd_ball/rules.mk']:
 assert 'SRC += '+home_path in (ROOT/'keyboards/ergohaven'/consumer).read_text()
(P/'results.txt').write_text(''.join(out))
