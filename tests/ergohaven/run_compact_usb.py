#!/usr/bin/env python3
"""Compile production parser and keyboard callback; retain VIA unlock gate/tail.
Only the unrelated VIA switch bodies and hardware boundaries are substituted.
"""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--ref');a=p.parse_args()
R=Path(__file__).resolve().parents[2];O=a.output;O.mkdir(parents=True,exist_ok=True)
import sys
sys.path.insert(0,str(R/'tests/ergohaven'))
from native_vial_firewall import firewall_source
def source(path):return subprocess.check_output(['git','show',f'{a.ref}:{path}'],cwd=R,text=True) if a.ref else (R/path).read_text()
h=source('keyboards/ergohaven/hid.c');v=source('quantum/via.c')
prod=h[h.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):h.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
prefix=(R/'tests/ergohaven/run_sm62_layout.py').read_text().split("text = r'''",1)[1].split('PRODUCTION',1)[0]
prefix=prefix.replace('static void hid_app_layout_changed_kb(void){}','static unsigned wakes; static void hid_app_layout_changed_kb(void){wakes++;}')
prefix += firewall_source(source)
callback=h[h.rindex('void raw_hid_receive_kb('):h.rindex('\nvoid keyboard_post_init_hid')]
gate=v[v.index('    /* When unlock is in progress'):v.index('\n#endif',v.index('    /* When unlock is in progress'))]
tail=v[v.index('    // Return the same buffer, optionally'):v.index('\n#if defined(VIA_QMK_BACKLIGHT_ENABLE)',v.index('    // Return the same buffer, optionally'))]
boundaries=r'''
#define EH_APP_LAYOUT_ENABLE
#define VIAL_HID_MAGIC 0xfeedfacecafebeefULL
static bool sm62_diag_raw(uint8_t*p,uint8_t n){(void)p;(void)n;return false;}
static bool process_via_custom_lighting(uint8_t*p,uint8_t n){(void)p;(void)n;return false;}
static bool process_raw_hid_data(uint8_t*p,uint8_t n){(void)p;(void)n;return false;}
static bool vial_unlock_in_progress;
enum {id_vial_prefix=0xfe, vial_get_keyboard_id=0, vial_get_size=1,vial_get_def=2,vial_get_unlock_status=5,vial_unlock_start=6,vial_unlock_poll=7};
static unsigned replies;static uint8_t response[32];
static void raw_hid_send(uint8_t*p,uint8_t n){assert(n==32);replies++;memcpy(response,p,32);}
'''
main=r'''
const uint8_t *sm62_cache_flash_record(unsigned r){static const uint8_t empty[8192];return r<SM62_CACHE_RECORDS?empty:NULL;}
bool sm62_cache_flash_erase(unsigned r){(void)r;return false;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t*p){(void)r;(void)off;(void)p;return false;}
static void receive(const uint8_t *in) {uint64_t aligned[4];memcpy(aligned,in,32);raw_hid_receive((uint8_t*)aligned,32);}
static void replay(const char *path, bool preserve, unsigned generation){
 FILE*f=fopen(path,"rb");assert(f);uint8_t p[32];while(fread(p,1,32,f)==32){receive(p);if(preserve)assert(app_layout_visual_generation==generation);}assert(feof(f));fclose(f);
}
static void capture(uint8_t *out) {
#define SAVE(x) memcpy(out,&(x),sizeof(x));out+=sizeof(x)
 SAVE(app_layout_active);SAVE(app_layout_revision);SAVE(app_layout_name);SAVE(app_layout_layer_names);SAVE(app_layout_keycodes);SAVE(app_layout_visuals);SAVE(app_layout_stack_counts);SAVE(app_layout_stack_keycodes);SAVE(app_layout_stack_names);SAVE(app_layout_wake_identity_seen);SAVE(app_layout_wake_identity);
#undef SAVE
}
int main(int argc,char**argv){
 if(argc==3&&!strcmp(argv[1],"--query")){
  assert(strlen(argv[2])==64);uint8_t q[32];for(unsigned i=0;i<32;i++){unsigned x;assert(sscanf(argv[2]+2*i,"%2x",&x)==1);q[i]=x;}
  receive(q);assert(replies==1);for(unsigned i=0;i<32;i++)printf("%02x",response[i]);puts("");return 0;
 }
 assert(argc==3||argc==4);replay(argv[1],false,0);
 uint8_t expected[6000]={0},actual[6000]={0};capture(expected);unsigned generation=app_layout_visual_generation;
 if(argc==4){
  bool held=hid_app_layout_process_keyevent(0,5,true);unsigned u=ups;
  replay(argv[2],true,generation);capture(actual);assert(!memcmp(expected,actual,sizeof(actual)));
  if(held){assert(ups==u);assert(hid_app_layout_process_keyevent(0,5,false));assert(ups==u+1);}
  puts("REJECT atomic + held-release");return 0;
 }
 bool held=hid_app_layout_process_keyevent(0,5,true);unsigned u=ups,w=wakes;
 replay(argv[2],false,0);capture(actual);assert(!memcmp(expected,actual,sizeof(actual)));
 assert(wakes==w);if(held){assert(ups==u);assert(hid_app_layout_process_keyevent(0,5,false));assert(ups==u+1);}
 uint8_t off[32]={0xea,8};receive(off);assert(!app_layout_active);
 FILE*f=fopen(argv[2],"rb");assert(f);uint8_t q[32];unsigned reports=0;
 while(fread(q,1,32,f)==32){unsigned opcode=q[0];receive(q);reports++;if(opcode!=0xee)assert(!app_layout_active);}
 fclose(f);capture(actual);for(unsigned i=0;i<sizeof(actual);i++)if(expected[i]!=actual[i])fprintf(stderr,"difference %u expected=%u actual=%u active=%u staging=%u crc=%04x\\n",i,expected[i],actual[i],app_layout_active,app_layout_staging.valid,app_layout_crc16(&app_layout_staging));assert(!memcmp(expected,actual,sizeof(actual)));
 assert(reports<210);printf("ACCEPT identical reports=%u compact_state=%zu\n",reports,sizeof(app_compact));
 // New capability reply must survive real keyboard callback + actual VIA tail.
 uint8_t cap[32]={0xeb,8,0xa5,1,1,2,3,4};replies=0;receive(cap);assert(replies==1&&response[2]==0x5a&&response[8]==3&&response[9]==0);
 vial_unlock_in_progress=true;replies=0;receive(cap);assert(replies==1&&!memcmp(response,cap,32));vial_unlock_in_progress=false;
 // Ordinary BEGIN and DEACTIVATE supersede a partial compact transaction.
 f=fopen(argv[2],"rb");assert(f);assert(fread(q,1,32,f)==32);fclose(f);receive(q);assert(app_compact.valid);
 uint8_t begin[32]={0xe1,8,1,42,0,0,0,62,16};receive(begin);assert(!app_compact.valid);
 receive(q);assert(app_compact.valid);receive(off);assert(!app_compact.valid);
 receive(q);now+=2500;hid_app_layout_task();assert(!app_compact.valid&&!app_layout_staging.valid);
 assert(!app_layout_active&&!hid_app_layout_process_keyevent(0,5,true));
 return 0;
}
'''
text=prefix+prod+boundaries+callback+'\nstatic void raw_hid_receive(uint8_t *data,uint8_t length) {\n'+gate+'\n raw_hid_receive_kb(data,length);\nskip:\n'+tail+main
src=O/'compact_receiver.c';src.write_text(text);exe=O/'compact_receiver'
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-DKEYBOARD_ergohaven_sm62','-DSM62_RAW_HID_FRAME_GATE','-I'+str(R/'keyboards/ergohaven'),str(src),'-o',str(exe)]
# Baseline mode compiles query-only to establish actual missing capability RED.
if a.ref:
 text=text[:text.index('int main(')]+r'''int main(void){uint8_t q[32]={0xeb,8,0xa5,1,1,2,3,4};receive(q);assert(replies==1&&response[2]==0x5a&&response[8]==3);return 0;}'''
 src.write_text(text);cmd.remove('-Werror');cmd.remove('-Wall');cmd.remove('-Wextra')
subprocess.run(cmd,check=True)
if a.ref:raise SystemExit(subprocess.run([str(exe)]).returncode)
print(exe)
