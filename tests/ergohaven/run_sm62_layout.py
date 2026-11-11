#!/usr/bin/env python3
"""Exercise shared production HID engine, substituting only QMK boundaries."""
import argparse
from pathlib import Path
import subprocess
from native_vial_firewall import firewall_source
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('output', type=Path)
p.add_argument('--ref')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
path = 'keyboards/ergohaven/hid.c'
src = subprocess.check_output(['git','show',f'{a.ref}:{path}'],cwd=ROOT,text=True) if a.ref else (ROOT/path).read_text()
def read_source(path):
    return subprocess.check_output(['git','show',f'{a.ref}:{path}'],cwd=ROOT,text=True) if a.ref else (ROOT/path).read_text()
src = src[src.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):src.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
text = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define KC_NO 0
#define KC_TRNS 1
static uint32_t now=100,layer_state=1,default_layer_state=1;
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static void layer_clear(void){layer_state=1;}
static unsigned downs,ups,taps;
static uint16_t down_code,up_code;
static void vial_keycode_down(uint16_t k){downs++;down_code=k;}
static void vial_keycode_up(uint16_t k){ups++;up_code=k;}
static void vial_keycode_tap(uint16_t k){(void)k;taps++;}
static void hid_app_layout_changed_kb(void){}
PRODUCTION
const uint8_t *sm62_cache_flash_record(unsigned r){static const uint8_t empty[8192];return r<SM62_CACHE_RECORDS?empty:NULL;}
bool sm62_cache_flash_erase(unsigned r){(void)r;return false;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t*p){(void)r;(void)off;(void)p;return false;}
int main(void){
 assert(EH_APP_LAYOUT_CONTROL_COUNT==62);
 assert(EH_APP_LAYOUT_KEY_COUNT==62);
 assert(EH_APP_LAYOUT_PROTOCOL_VERSION==8);
 uint8_t begin[32]={0xe1,8,1,42,0,0,0,62,16};
 assert(hid_app_layout_process_packet(begin,32));assert(app_layout_staging.valid);
 now+=2501;hid_app_layout_task();assert(!app_layout_staging.valid);
 assert(hid_app_layout_process_packet(begin,32));
 uint8_t malformed[32];memcpy(malformed,begin,32);malformed[7]=61;
 assert(hid_app_layout_process_packet(malformed,32));assert(!app_layout_staging.valid);
 assert(hid_app_layout_process_packet(begin,32));
 for(unsigned l=0;l<16;l++){
  for(unsigned s=0;s<62;s+=13){
   unsigned n=62-s<13?62-s:13;uint8_t packet[32]={0xe2,8,l,s,n};
   for(unsigned k=0;k<n;k++){uint16_t code=4+l*62+s+k;packet[5+k*2]=code;packet[6+k*2]=code>>8;}
   assert(hid_app_layout_process_packet(packet,32));
  }
  for(unsigned s=0;s<62;s+=27){
   unsigned n=62-s<27?62-s:27;uint8_t packet[32]={0xe7,8,l,s,n};
   memset(packet+5,1,n);assert(hid_app_layout_process_packet(packet,32));
  }
  uint8_t name[32]={0xe4,8,l,1,'A'};assert(hid_app_layout_process_packet(name,32));
  for(unsigned s=0;s<4;s++){uint8_t stack[32]={0xe9,8,l,s};assert(hid_app_layout_process_packet(stack,32));}
 }
 uint16_t crc=app_layout_crc16(&app_layout_staging);
 uint8_t commit[32]={0xe3,8,1,42,0,0,0,crc,crc>>8};
 assert(hid_app_layout_process_packet(commit,32));assert(app_layout_active);
 unsigned checks=3;
 const uint8_t positions[62][2]=POSITIONS;
 for(unsigned l=0;l<16;l++){
  layer_state=1u<<l;
  for(unsigned k=0;k<62;k++){
   uint8_t row=positions[k][0],col=positions[k][1];uint16_t code=4+l*62+k,combo=0;
   assert(hid_app_layout_get_combo_keycode(row,col,&combo)&&combo==code);
   unsigned d=downs,u=ups;
   assert(hid_app_layout_process_keyevent(row,col,true));assert(downs==d+1&&down_code==code);
   layer_state=1u<<((l+1)%16);
   assert(hid_app_layout_process_keyevent(row,col,false));assert(ups==u+1&&up_code==code);
   layer_state=1u<<l;checks++;
  }
 }
 assert(!hid_app_layout_process_keyevent(4,6,true));assert(!hid_app_layout_process_encoder_event(0,true,true));
 assert(hid_app_layout_process_keyevent(0,5,true));unsigned u=ups;
 uint32_t generation=hid_app_layout_visual_generation();
 now+=2501;hid_app_layout_task();assert(!app_layout_active&&ups==u+1);
 assert(hid_app_layout_visual_generation()!=generation);
 assert(!hid_app_layout_process_keyevent(0,5,true));assert(taps==0);
 printf("PASS %u geometry/protocol/held-release/combo assertions; timeout and invalid controls rejected\n",checks+4);
}
'''
import json
positions=json.loads((ROOT/'keyboards/ergohaven/sm62/keyboard.json').read_text())['layouts']['LAYOUT']['layout']
assert len(positions)==62
text=text.replace('PRODUCTION',firewall_source(read_source)+src).replace('POSITIONS','{'+','.join('{'+','.join(map(str,k['matrix']))+'}' for k in positions)+'}')
source=a.output/'sm62_layout.c';source.write_text(text)
exe=a.output/'sm62_layout'
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-DKEYBOARD_ergohaven_sm62','-I'+str(ROOT/'keyboards/ergohaven'),str(source),'-o',str(exe)]
subprocess.run(cmd,check=True)
result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
(a.output/'results.log').write_text(result.stdout)
print(result.stdout, end='',flush=True)
raise SystemExit(result.returncode)
