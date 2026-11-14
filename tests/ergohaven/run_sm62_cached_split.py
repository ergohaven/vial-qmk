#!/usr/bin/env python3
"""Production cached-image split regression; injected timing is not hardware FPS.
Relocated from the delivered cached-repair probe without changing its C controls.
Generated files require an external output directory; production is read-only.
"""
from pathlib import Path
from native_vial_firewall import firewall_source
import argparse, subprocess, json
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output',type=Path)
parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
a=parser.parse_args()
ROOT=root=a.source.resolve()
P=a.output.resolve()
if P==ROOT or ROOT in P.parents:
    parser.error('output must be outside repository source')
P.mkdir(parents=True,exist_ok=True)
def read(p):return (root/p).read_text()
def function(text,name):
    import re
    m=re.search(r'^(?:static )?(?:inline )?(?:const char \*|uint16_t |void \*|void |bool )'+name+r'\([^\n;]*\)\s*\{',text,re.M)
    if not m:raise ValueError(name)
    e=text.index('{',m.start())+1;d=1
    while d:d+=(text[e]=='{')-(text[e]=='}');e+=1
    return text[m.start():e]+'\n'
hid=read('keyboards/ergohaven/hid.c');prod=hid[hid.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):hid.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
nvm=read('quantum/nvm/eeprom/nvm_dynamic_keymap.c')
common=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define LAYER_LABEL_SIZE 16
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
static uint8_t eeprom[16*10*7*2];
#define DYNAMIC_KEYMAP_EEPROM_ADDR eeprom
#define KC_NO 0
#define KC_TRNS 1
#define RPC_SM62_APP_LAYOUT 5
extern uint64_t clock_us;
extern unsigned key_byte_us,rpc_us;
extern bool link_ok;
void sm62_fps_capture(uint32_t t){(void)t;}
static uint64_t reads, rpcs, captures, publications, crc_bytes, bytes_in,bytes_out;
static uint32_t layer_state=1,default_layer_state=1;
static char layer_names[16][LAYER_LABEL_SIZE];
static uint8_t eeprom_read_byte(const void *p){reads++;clock_us+=key_byte_us;return *(const uint8_t*)p;}
static bool is_keyboard_master(void){return IS_MASTER;}
static uint32_t timer_read32(void){return clock_us/1000;}
static uint32_t timer_elapsed32(uint32_t t){return timer_read32()-t;}
static uint8_t get_highest_layer(uint32_t s){return 31-__builtin_clz(s);}
static void layer_clear(void){layer_state=1;}
static void vial_keycode_down(uint16_t k){(void)k;}
static void vial_keycode_up(uint16_t k){(void)k;}
static void vial_keycode_tap(uint16_t k){(void)k;}
static void hid_app_layout_changed_kb(void){}
static void (*callback)(uint8_t,const void*,uint8_t,void*);
static void transaction_register_rpc(int id,void(*cb)(uint8_t,const void*,uint8_t,void*)){assert(id==5);callback=cb;}
extern void peer_receive(uint8_t,const void*,uint8_t,void*);
static bool transaction_rpc_exec(int id,uint8_t n,const void *in,uint8_t m,void *out){
 assert(IS_MASTER&&id==5&&n==40&&m==5);rpcs++;bytes_out+=n;clock_us+=rpc_us;
 if(!link_ok)return false;
 peer_receive(n,in,m,out);bytes_in+=m;return true;
}
'''
exports=r'''
/* Storage is not exercised by this split-only fixture. */
const uint8_t *sm62_cache_flash_record(unsigned r){static const uint8_t empty[8192];return r<SM62_CACHE_RECORDS?empty:NULL;}
bool sm62_cache_flash_erase(unsigned r){(void)r;return false;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t*p){(void)r;(void)off;(void)p;return false;}
void __cyg_profile_func_enter(void *fn, void *caller) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *fn, void *caller) __attribute__((no_instrument_function));
/* Weak per-object hooks are not used: compile-local hook names supplied by -D. */
void __cyg_profile_func_enter(void *fn,void *caller){
 (void)caller;
 if(fn==(void*)sm62_app_capture)captures++;
 if(fn==(void*)sm62_app_publish)publications++;
 if(fn==(void*)app_layout_crc16_byte)crc_bytes++;
}
void __cyg_profile_func_exit(void *fn,void *caller){(void)fn;(void)caller;}
void ROLE_init(void){hid_app_layout_split_init();}
void ROLE_task(void){hid_app_layout_split_task();}
void ROLE_receive(uint8_t n,const void *in,uint8_t m,void *out){bytes_in+=n;bytes_out+=m;callback(n,in,m,out);}
void ROLE_stats(uint64_t *v){v[0]=captures;v[1]=reads;v[2]=rpcs;v[3]=publications;v[4]=crc_bytes;v[5]=bytes_out;v[6]=bytes_in;v[7]=sizeof(sm62_app_image_t);v[8]=offsetof(sm62_app_image_t,crc);v[9]=sm62_app_tx_epoch;}
'''
# Production application state, timeout/session functions and entire split include
# are compiled unmodified. Only hardware/OS boundaries above are substituted.
body=common+function(read('keyboards/ergohaven/src/eh_settings.c'),'layer_name')+function(nvm,'dynamic_keymap_key_to_eeprom_address')+function(nvm,'nvm_dynamic_keymap_read_keycode')+function(read('quantum/dynamic_keymap.c'),'dynamic_keymap_get_keycode')+firewall_source(read)+prod+exports
def writer_function(text,name):
 import re
 m=re.search(r'^(?:__attribute__\(\(weak\)\) )?(?:static )?(?:inline )?(?:bool|void|int)\s+'+name+r'\([^\n;]*\)\s*\{',text,re.M);assert m,name
 e=text.index('{',m.start())+1;d=1
 while d:d+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[m.start():e]+'\n'
nvm=(ROOT/'quantum/nvm/eeprom/nvm_dynamic_keymap.c').read_text()
dyn=(ROOT/'quantum/dynamic_keymap.c').read_text()
settings=(ROOT/'keyboards/ergohaven/src/eh_settings.c').read_text()
writers='''
static void eeprom_update_byte(void *p,uint8_t v){*(uint8_t*)p=v;}
#define KB_SETTINGS_LAYER_LABELS_OFFSET 0
typedef struct {int qsid;} qmk_settings_proto_t;
static bool layer_name_updated;
static void eeconfig_update_kb_datablock(const void*p,unsigned o,size_t n){(void)p;(void)o;(void)n;}
#define dprintf(...) ((void)0)
'''+''.join(writer_function(nvm,n) for n in ['nvm_dynamic_keymap_update_keycode','nvm_dynamic_keymap_update_buffer'])+''.join(writer_function(dyn,n) for n in ['dynamic_keymap_set_keycode','dynamic_keymap_set_buffer'])+ '\n#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wformat-security"\n#pragma GCC diagnostic ignored "-Wunused-parameter"\n' +writer_function(settings,'layer_name_set')+'\n#pragma GCC diagnostic pop\n'
body+=writers
startup=(ROOT/'quantum/split_common/split_util.c').read_text()
startup_code=''.join(writer_function(startup,n) for n in ['usb_bus_detected','is_keyboard_master_impl','is_keyboard_left_impl','is_keyboard_left','is_keyboard_master','split_pre_init','split_post_init'])
startup_stubs='''
extern bool master_left;
#define SPLIT_HAND_PIN 29
#define SPLIT_USB_TIMEOUT 2000
#define SPLIT_USB_TIMEOUT_POLL 10
static bool usb=IS_MASTER,gpio,isLeftHand;
static struct {bool master,left;} split_config;
static bool usb_connected_state(void){return usb;}
static void wait_ms(unsigned n){(void)n;}
static void wait_us(unsigned n){(void)n;}
static void gpio_set_pin_input(unsigned p){assert(p==29);}
static bool gpio_read_pin(unsigned p){assert(p==29);return gpio;}
static void usb_disconnect(void){}
static void transport_master_init(void){}
static void transport_slave_init(void){}
'''
body=body.replace('static bool is_keyboard_master(void){return IS_MASTER;}',startup_stubs+startup_code)
body=body.replace('void ROLE_init(void){','void ROLE_init(void){gpio=IS_MASTER?master_left:!master_left;split_pre_init();split_post_init();assert(is_keyboard_master()==IS_MASTER&&is_keyboard_left()==gpio);')
body=body.replace('static bool transaction_rpc_exec(', 'extern unsigned fault;\nextern void peer_unpublished_controls(void);\nstatic bool transaction_rpc_exec(')
body=body.replace('peer_receive(n,in,m,out);bytes_in+=m;return true;', '''
 uint8_t packet[40];memcpy(packet,in,40);
 if(fault==1)packet[38]^=1;
 peer_receive(n,packet,m,out);bytes_in+=m;
 if(packet[6])peer_unpublished_controls();
 if(fault==2)return false;
 if(fault==3){peer_receive(n,packet,m,out);}
 if(fault==4)((uint8_t*)out)[1]^=1;
 return true;''')
extra=r'''
void ROLE_activate(void){app_layout_active=true;app_layout_revision=42;app_layout_sync_time=timer_read32();}
void ROLE_keepalive(void){uint8_t p[7]={EH_APP_LAYOUT_KEEPALIVE,EH_APP_LAYOUT_PROTOCOL_VERSION,1,42,0,0,0};assert(hid_app_layout_process_packet(p,7));}
bool ROLE_live(void){return app_layout_session_live();}
void ROLE_change(unsigned kind){
 uint32_t g=app_layout_visual_generation;
 if(kind==0)dynamic_keymap_set_keycode(15,9,3,0x1234);
 if(kind==1){uint8_t b[2]={0x56,0x78};dynamic_keymap_set_buffer((15*70+9*7+3)*2,2,b);}
 if(kind==2){qmk_settings_proto_t p={215};assert(layer_name_set(&p,"Fresh Default",16)==0);}
 if(kind<3)assert(g==app_layout_visual_generation);
 if(kind==3){app_layout_keycodes[15][61]=0x4321;strcpy(app_layout_name,"Overlay");strcpy(app_layout_layer_names[15],"Overlay layer");app_layout_visuals[15][61]=3;app_layout_execution_states[15][61]=2;app_layout_visual_generation++;}
 if(kind==4){uint8_t p[2]={EH_APP_LAYOUT_DEACTIVATE,EH_APP_LAYOUT_PROTOCOL_VERSION};assert(hid_app_layout_process_packet(p,2));}
 if(kind==5){
  // CRC16 collision generated over the real 6626-byte image prefix. Exact
  // content comparison must still send both changed Default keys.
  dynamic_keymap_set_keycode(15,9,2,dynamic_keymap_get_keycode(15,9,2)^1);
  dynamic_keymap_set_keycode(15,9,3,dynamic_keymap_get_keycode(15,9,3)^12595);
  sm62_app_capture();assert(sm62_app_tx.crc==sm62_app_acked.crc&&memcmp(&sm62_app_tx,&sm62_app_acked,sizeof(sm62_app_tx)));
 }
}
void ROLE_check(unsigned kind){
 if(kind==0)assert(hid_sm62_default_keycode(15,9,3)==0x1234);
 if(kind==1)assert(hid_sm62_default_keycode(15,9,3)==0x5678);
 if(kind==2)assert(!strcmp(hid_sm62_default_layer_name(15),"Fresh Default"));
 if(kind==3)assert(app_layout_keycodes[15][61]==0x4321&&!strcmp(app_layout_name,"Overlay")&&!strcmp(app_layout_layer_names[15],"Overlay layer")&&app_layout_visuals[15][61]==3&&app_layout_execution_states[15][61]==2);
 if(kind==4)assert(!app_layout_session_live());
 if(kind==5)assert(hid_sm62_default_keycode(15,9,2)==1&&hid_sm62_default_keycode(15,9,3)==(0x5678^12595));
}
void ROLE_reset(unsigned kind){
 if(IS_MASTER){sm62_app_tx_started=false;sm62_app_tx_offset=0;sm62_app_tx_epoch=0;sm62_app_acked_valid=false;if(kind){app_layout_wake_identity_seen=false;memset(app_layout_wake_identity,0,20);}}
 else {sm62_app_rx_seen=false;sm62_app_rx_pending=0;sm62_app_rx_retained=false;sm62_app_rx_published=false;sm62_app_rx_epoch=kind?0x1000:0;sm62_app_rx_offset=0;app_layout_active=false;if(kind)sm62_app_rx_seen=true;}
}
void ROLE_identity(void){memset(app_layout_wake_identity,42,20);app_layout_wake_identity_seen=true;app_layout_visual_generation++;}
void ROLE_unpublished_controls(void){
 if(sm62_app_rx_offset==sizeof(sm62_app_rx)&&sm62_app_rx_retained&&!sm62_app_rx_pending)return;
 uint8_t p[40]={0},ack[5];sm62_app_put32(p,sm62_app_rx_epoch);p[4]=p[5]=255;p[7]=sm62_app_rx.crc;p[8]=sm62_app_rx.crc>>8;sm62_app_put32(p+9,sm62_app_rx.revision);p[13]=sm62_app_rx.active;p[14]=sm62_app_rx.identity_seen;memcpy(p+15,sm62_app_rx.identity,20);
 uint16_t c=sm62_app_crc(p,38);p[38]=c;p[39]=c>>8;
 uint32_t t=sm62_app_rx_time,g=app_layout_visual_generation;uint8_t pending=sm62_app_rx_pending;
 callback(40,p,5,ack);assert(!ack[0]&&pending==sm62_app_rx_pending&&t==sm62_app_rx_time&&g==app_layout_visual_generation);
}
void ROLE_heartbeat_controls(bool delayed){
 assert(!IS_MASTER&&sm62_app_rx_retained);uint8_t p[40]={0},ack[5];
 sm62_app_put32(p,sm62_app_rx_epoch);p[4]=p[5]=255;p[7]=sm62_app_rx.crc;p[8]=sm62_app_rx.crc>>8;sm62_app_put32(p+9,sm62_app_rx.revision);p[13]=sm62_app_rx.active;p[14]=sm62_app_rx.identity_seen;memcpy(p+15,sm62_app_rx.identity,20);
 for(unsigned i=0;i<10;i++){
  uint8_t b[40];memcpy(b,p,40);
  switch(i){case 0:b[0]--;break;case 1:b[0]++;break;case 2:b[7]^=1;break;case 3:b[9]^=1;break;case 4:b[13]^=1;break;case 5:b[14]^=1;break;case 6:b[15]^=1;break;case 7:b[35]=1;break;case 8:b[4]=0;break;default:b[6]=32;break;}
  uint16_t c=sm62_app_crc(b,38);b[38]=c;b[39]=c>>8;uint32_t t=sm62_app_rx_time,g=app_layout_visual_generation;
  callback(40,b,5,ack);assert(!ack[0]&&!sm62_app_rx_pending);hid_app_layout_split_task();assert(t==sm62_app_rx_time&&g==app_layout_visual_generation);
 }
 uint16_t c=sm62_app_crc(p,38);p[38]=c;p[39]=c>>8;
 uint32_t t=sm62_app_rx_time,g=app_layout_visual_generation;
 callback(40,p,5,ack);assert(ack[0]==2&&sm62_app_rx_time==t&&app_layout_visual_generation==g);
 // Duplicate cannot renew or overwrite a staged mailbox; newer DATA also waits.
 callback(40,p,5,ack);assert(!ack[0]);hid_app_layout_split_task();assert(app_layout_visual_generation==g);
 // A staged heartbeat cannot cross a local disable/generation change.
 callback(40,p,5,ack);assert(ack[0]==2);
 if(delayed){clock_us+=2501000;assert(!app_layout_session_live());}
 else {app_layout_visual_generation++;app_layout_active=false;}
 hid_app_layout_split_task();assert(!sm62_app_rx_retained&&!app_layout_active);
 callback(40,p,5,ack);assert(!ack[0]);
}
'''
commands=[]
for role in ['master','peer']:
 src=P/(role+'.c');src.write_text(body.replace('ROLE_',role+'_')+extra.replace('ROLE_',role+'_'))
 obj=src.with_suffix('.o')
 cmd=['gcc','-include',str(ROOT/'quantum/sm62_trace.h'),'-std=gnu11','-O0','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-type-limits','-fsanitize=undefined','-fno-sanitize-recover=all','-finstrument-functions',f'-DIS_MASTER={int(role=="master")}','-DKEYBOARD_ergohaven_sm62','-DEH_SM62_SPLIT_RUNTIME_ENABLE','-I'+str(ROOT/'keyboards/ergohaven'),'-c',str(src),'-o',str(obj)]
 commands.append(cmd);subprocess.run(cmd,check=True)
 syms=subprocess.check_output(['nm','-g','--defined-only',str(obj)],text=True)
 local=[l.split()[-1] for l in syms.splitlines() if not l.split()[-1].startswith(role+'_')]
 subprocess.run(['objcopy',*['--localize-symbol='+n for n in local],str(obj)],check=True)
main=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
uint64_t clock_us;unsigned key_byte_us=2,rpc_us=2500,fault;bool link_ok=true,master_left;
void master_init(void);void peer_init(void);void master_task(void);void peer_task(void);void master_activate(void);void master_keepalive(void);bool peer_live(void);void master_stats(uint64_t*);void peer_stats(uint64_t*);
void master_change(unsigned);void peer_check(unsigned);void master_reset(unsigned);void peer_reset(unsigned);void peer_heartbeat_controls(bool);bool master_live(void);
void master_identity(void);
static void run_ms(unsigned ms,bool keep){uint64_t end=clock_us+ms*1000ULL;while(clock_us<end){clock_us+=28928;if(keep)master_keepalive();master_task();peer_task();peer_live();}}
int main(int argc,char**argv){
 assert(argc==3);bool active=argv[1][0]=='1';master_left=argv[2][0]=='1';master_init();peer_init();if(active)master_activate();
 bool seen=false;uint64_t first=0,warm[10]={0},wpeer[10]={0};unsigned expired=0;
 while(clock_us<16000000){clock_us+=28928;if(active)master_keepalive();master_task();peer_task();bool live=peer_live();if(live&&!seen){seen=true;first=clock_us;}
 if(active&&seen&&!live)expired++;
 if(clock_us>4000000&&!warm[0]){master_stats(warm);peer_stats(wpeer);}}
 uint64_t m[10],p[10];master_stats(m);peer_stats(p);
 printf("active=%u first_ms=%llu RPCs=%llu steady_RPCs=%llu captures=%llu steady_publications=%llu expired=%u\n",active,(unsigned long long)(first/1000),(unsigned long long)m[2],(unsigned long long)(m[2]-warm[2]),(unsigned long long)m[0],(unsigned long long)(p[3]-wpeer[3]),expired);fflush(stdout);
 assert(m[2]-warm[2]<30);assert(m[0]>5);assert(p[3]==1);assert(!active||(seen&&expired==0));
 puts("PASS unchanged full recapture retained, <=1 small RPC per 500ms, no republish, live peer across TTL windows");
 if(active){
  for(unsigned k=0;k<4;k++){master_change(k);run_ms(3500,true);peer_check(k);assert(peer_live());}
  puts("PASS real Default single/bulk/name producers detected without visual generation; overlay key/name/visual/state propagated");
  master_change(5);run_ms(3500,true);peer_check(5);puts("PASS equal whole-image CRC16 with different bytes still causes full propagation (exact memcmp)");
  peer_heartbeat_controls(false);run_ms(3500,true);assert(peer_live());
  peer_heartbeat_controls(true);master_activate();run_ms(3500,true);assert(peer_live());
  puts("PASS strict heartbeat context/CRC/session fences, callback stages only, no visual generation increment; rejected heartbeat recovers");
  for(unsigned k=0;k<2;k++){peer_reset(k);run_ms(3500,true);assert(peer_live());peer_check(3);}
  master_reset(0);run_ms(3500,true);assert(peer_live());
  puts("PASS peer reset/reconnect and retained newer epoch/master restart recovery");
  master_identity();run_ms(3500,true);master_reset(1);run_ms(3500,true);peer_stats(wpeer);run_ms(8000,true);peer_stats(p);assert(p[3]==wpeer[3]&&peer_live());
  puts("PASS reset sender's unknown identity preserves peer's retained semantic identity without repeated full publication");
  for(fault=1;fault<5;fault++){run_ms(1200,true);unsigned saved=fault;fault=0;run_ms(3500,true);assert(peer_live());fault=saved;}
  fault=0;link_ok=false;run_ms(3000,true);assert(!peer_live());link_ok=true;run_ms(3500,true);assert(peer_live());
  puts("PASS corrupted CRC/ACK epoch, ACK loss, duplicate frames, disconnect expiry and full resync");
  run_ms(6000,false);assert(!master_live()&&!peer_live());
  puts("PASS split keepalive cannot extend physical master host session; inactive changed image propagated");
  master_activate();run_ms(3500,true);assert(peer_live());master_change(4);run_ms(3500,false);peer_check(4);
  puts("PASS explicit host deactivation propagated");
 }
}
'''
(P/'main.c').write_text(main)
cmd=['gcc','-fsanitize=undefined','-fno-sanitize-recover=all',str(P/'main.c'),str(P/'master.o'),str(P/'peer.o'),'-o',str(P/'test_cached')];commands.append(cmd);subprocess.run(cmd,check=True)
for arg in [['0','0'],['0','1'],['1','0'],['1','1']]:
 cmd=[str(P/'test_cached'),*arg];commands.append(cmd);subprocess.run(cmd,check=True)
(P/'test-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
