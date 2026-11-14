#!/usr/bin/env python3
"""Source-backed legacy full-fallback split; cached/delta codecs have separate runners."""
from pathlib import Path
from native_vial_firewall import firewall_source
import argparse, subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=(ROOT/'keyboards/ergohaven/hid.c').read_text();s=s[s.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):s.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
roles=(ROOT/'quantum/split_common/split_util.c').read_text()
import re
def function(name):
 m=re.search(r'^(?:__attribute__\(\(weak\)\) )?(?:static )?(?:inline )?(?:bool|void)\s+'+name+r'\([^;\n]*\)\s*\{',roles,re.M);assert m,name
 e=roles.index('{',m.start())+1;d=1
 while d:d+=(roles[e]=='{')-(roles[e]=='}');e+=1
 return roles[m.start():e]+'\n'
startup=''.join(function(n) for n in ['usb_bus_detected','is_keyboard_master_impl','is_keyboard_left_impl','is_keyboard_left','is_keyboard_master','split_pre_init','split_post_init'])
text=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define LAYER_LABEL_SIZE 16
static const char *layer_name(uint8_t l){(void)l;return "Default";}
static uint16_t dynamic_keymap_get_keycode(uint8_t l,uint8_t r,uint8_t c){return 4+l*70+r*7+c;}
#define KC_NO 0
static bool stale_peer_map;
#define DEFAULT_MAP(l,r,c) (4+(l)*70+(r)*7+(c))
#define dynamic_keymap_get_keycode(l,r,c) (stale_peer_map ? 4 : DEFAULT_MAP(l,r,c))
#define KC_TRNS 1
#define RPC_SM62_APP_LAYOUT 5
static uint32_t now=100,layer_state=1,default_layer_state=1;
#define SPLIT_HAND_PIN 29
#define SPLIT_USB_TIMEOUT 2000
#define SPLIT_USB_TIMEOUT_POLL 10
static bool usb=true,gpio=true,isLeftHand;
static struct {bool master,left;} split_config;
static bool usb_connected_state(void){return usb;}
static void wait_ms(unsigned n){(void)n;}
static void wait_us(unsigned n){(void)n;}
static void gpio_set_pin_input(unsigned p){assert(p==29);}
static bool gpio_read_pin(unsigned p){assert(p==29);return gpio;}
static void usb_disconnect(void){}
static void transport_master_init(void){}
static void transport_slave_init(void){}
STARTUP
static uint32_t timer_read32(void){return now;}
static uint32_t timer_elapsed32(uint32_t t){return now-t;}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static void layer_clear(void){layer_state=1;}
static void vial_keycode_down(uint16_t k){(void)k;}
static void vial_keycode_up(uint16_t k){(void)k;}
static void vial_keycode_tap(uint16_t k){(void)k;}
static void hid_app_layout_changed_kb(void){}
void sm62_fps_capture(uint32_t start){(void)start;}

static void (*callback)(uint8_t,const void*,uint8_t,void*);
static uint8_t frames[2000][40];static unsigned count;
static void transaction_register_rpc(int id,void (*cb)(uint8_t,const void*,uint8_t,void*)){(void)id;callback=cb;}
static bool transaction_rpc_exec(int id,uint8_t n,const void *in,uint8_t m,void *out){
 (void)id;assert(n==40&&m==5);
 uint8_t *ack=out;memset(ack,0,m);memcpy(ack+1,in,4);
 // Model an explicitly legacy peer: it rejects compressed/delta BEGIN and
 // retained-image heartbeat. Record only the resulting ordinary DATA stream,
 // so frames[1] is a genuinely reordered DATA segment, not fallback offset 0.
 if(!((const uint8_t *)in)[6])return true;
 assert(count<2000);memcpy(frames[count++],in,n);ack[0]=1;return true;
}
PRODUCTION
const uint8_t *sm62_cache_flash_record(unsigned r){static const uint8_t empty[8192];return r<SM62_CACHE_RECORDS?empty:NULL;}
bool sm62_cache_flash_erase(unsigned r){(void)r;return false;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t*p){(void)r;(void)off;(void)p;return false;}
#ifndef EH_SM62_SPLIT_RUNTIME_IMPLEMENTED
static void hid_app_layout_split_init(void){}
static void hid_app_layout_split_task(void){}
#endif
#ifndef EH_SM62_SPLIT_DEFAULT_BINDINGS
static uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c){return dynamic_keymap_get_keycode(l,r,c);}
#endif
int main(int argc,char **argv){
 assert(argc==3);gpio=argv[1][0]=='1';usb=argv[2][0]=='1';
 bool expected_left=gpio,expected_master=usb;
 split_pre_init();split_post_init();
 assert(is_keyboard_left()==expected_left&&is_keyboard_master()==expected_master);
 gpio=!gpio;assert(is_keyboard_left()==expected_left);gpio=expected_left;
 usb=true;split_pre_init();split_post_init();
 hid_app_layout_split_init();assert(callback&&"production split RPC receiver must be registered");
 uint8_t begin[32]={0xe1,8,1,42,0,0,0,62,16,6,'E','d','i','t','o','r'};
 assert(hid_app_layout_process_packet(begin,32));
 for(unsigned l=0;l<16;l++){
  for(unsigned s=0;s<62;s+=13){
   unsigned n=62-s<13?62-s:13;uint8_t packet[32]={0xe2,8,l,s,n};
   if(l==15&&s==52){packet[5+9*2]=0x34;packet[6+9*2]=0x12;}
   assert(hid_app_layout_process_packet(packet,32));
  }
  for(unsigned s=0;s<62;s+=27){unsigned n=62-s<27?62-s:27;uint8_t packet[32]={0xe7,8,l,s,n};if(l==15&&s==54)packet[5+7]=3;assert(hid_app_layout_process_packet(packet,32));}
  uint8_t name[32]={0xe4,8,l,0};if(l==15){name[3]=10;memcpy(name+4,"Last layer",10);}assert(hid_app_layout_process_packet(name,32));
  for(unsigned s=0;s<4;s++){uint8_t stack[32]={0xe9,8,l,s};assert(hid_app_layout_process_packet(stack,32));}
 }
 uint16_t wire_crc=app_layout_crc16(&app_layout_staging);uint8_t commit[32]={0xe3,8,1,42,0,0,0,wire_crc,wire_crc>>8};assert(hid_app_layout_process_packet(commit,32));assert(app_layout_active);
 uint8_t state[32]={0xe8,8,42,0,0,0,15,61,2};assert(hid_app_layout_process_packet(state,32));
 for(unsigned i=0;i<1000;i++){hid_app_layout_split_task();now++;}
 assert(count>1);
 usb=false;gpio=!expected_left;split_pre_init();split_post_init();assert(!is_keyboard_master()&&is_keyboard_left()!=expected_left);app_layout_active=false;app_layout_keycodes[15][61]=0;app_layout_name[0]=0;
 uint8_t ack[5];
 // CRC damage and reordered segment cannot publish or refresh a snapshot.
 uint8_t bad[40];memcpy(bad,frames[0],40);bad[9]^=1;callback(40,bad,5,ack);assert(!ack[0]&&!app_layout_active);
 callback(40,frames[1],5,ack);assert(!ack[0]&&!app_layout_active);
 unsigned committed=0;
 for(unsigned i=0;i<count;i++){
  callback(40,frames[i],5,ack);assert(ack[0]);
  assert(!app_layout_active&&"serial callback must not publish into concurrently rendering main loop");
  hid_app_layout_split_task();
  if(app_layout_active){committed++;break;}
  assert(app_layout_keycodes[15][61]==0&&app_layout_name[0]==0);
 }
 assert(committed==1);assert(app_layout_keycodes[15][61]==0x1234);
 stale_peer_map=true;
 assert(hid_sm62_default_keycode(15,9,3)==DEFAULT_MAP(15,9,3));
 assert(strcmp(hid_sm62_default_layer_name(15),"Default")==0);
 assert(app_layout_visuals[15][61]==3&&app_layout_execution_states[15][61]==2);
 assert(strcmp(app_layout_layer_names[15],"Last layer")==0&&strcmp(app_layout_name,"Editor")==0);
 // Completed old epochs and duplicate segments cannot roll back active data.
 uint32_t gen=app_layout_visual_generation;
 callback(40,frames[0],5,ack);assert(ack[0]);assert(app_layout_visual_generation==gen);
 now+=2501;hid_app_layout_task();assert(!app_layout_active);
 callback(40,frames[0],5,ack);assert(!app_layout_active);
 // Expired in-progress image must never revive the old application.
 uint32_t next_epoch=sm62_app_rx_epoch+1;
 for(unsigned i=0;i<count;i++){
  uint8_t packet[40];memcpy(packet,frames[i],40);
  if(app_layout_read_u32(frames[i])!=app_layout_read_u32(frames[0]))break;
  sm62_app_put32(packet,next_epoch);uint16_t crc=sm62_app_crc(packet,38);packet[38]=crc;packet[39]=crc>>8;
  callback(40,packet,5,ack);
  if(i==0)now+=2501;
 }
 assert(!app_layout_active&&"incomplete expired split snapshot must not publish");
 for(unsigned round=0;round<2;round++){
  ++next_epoch;
  for(unsigned i=0;i<count;i++){
   if(app_layout_read_u32(frames[i])!=app_layout_read_u32(frames[0]))break;
   uint8_t packet[40];memcpy(packet,frames[i],40);sm62_app_put32(packet,next_epoch);
   if(round==0&&i==0)packet[9]^=1; // per-frame CRC valid; whole-image CRC wrong
   uint16_t crc=sm62_app_crc(packet,38);packet[38]=crc;packet[39]=crc>>8;
   callback(40,packet,5,ack);hid_app_layout_split_task();
   if(round==1)now+=20; // continuous progress may outlast a whole-session TTL
  }
  assert(app_layout_active==(round==1));
 }
 callback(40,frames[0],5,ack);assert(!ack[0]&&app_layout_active);
 puts("PASS split: registration, actual sender/receiver, atomic last-control/layer names+visuals, CRC/reordering/duplicate/timeout");
}
'''.replace('PRODUCTION',firewall_source(lambda path:(ROOT/path).read_text())+s).replace('STARTUP',startup)
src=a.output/'split.c';src.write_text(text);exe=a.output/'split'
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-DKEYBOARD_ergohaven_sm62','-DEH_SM62_SPLIT_RUNTIME_ENABLE','-I'+str(ROOT/'keyboards/ergohaven'),'-include',str(ROOT/'quantum/sm62_trace.h'),str(src),'-o',str(exe)]
subprocess.run(cmd,check=True)
logs=[]
for gpio in ['0','1']:
 for usb in ['0','1']:
  result=subprocess.run([str(exe),gpio,usb],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  logs.append(f'GP29={gpio} USB={usb}: '+result.stdout)
  (a.output/'results.log').write_text(''.join(logs));print(logs[-1],end='');result.check_returncode()
print('PASS 4 actual QMK startup/handedness × USB-role combinations')
