#!/usr/bin/env python3
"""Actual production split sender/receiver, ELF-derived factory map. No hardware timing claim."""
from pathlib import Path
import sys, subprocess, json, hashlib
# Reuse the maintained production native boundary generator, not sender logic.
runner=Path(__file__).with_name('run_sm62_delta_split.py')
text=runner.read_text().split('commands=[]')[0]
exec(compile(text,str(runner),'exec'))
fixture=Path(__file__).with_name('sm62_factory_keymaps.bin').read_bytes()
assert len(fixture)==2240 and hashlib.sha256(fixture).hexdigest()=='c81f5d2a6bc5ee6c1f3274ebfcbcbcf000aa7333e8fee1cd504b3aa03bf15507'
seed=r'''
void ROLE_seed(const uint16_t *k, bool noise) {
 uint32_t rng=17;
 for(unsigned l=0;l<16;l++){
  snprintf(layer_names[l],LAYER_LABEL_SIZE,"Native %u",l);
  snprintf(app_layout_layer_names[l],23,"Overlay %u",l);
  for(unsigned r=0;r<10;r++)for(unsigned c=0;c<7;c++)dynamic_keymap_set_keycode(l,r,c,k[l*70+r*7+c]);
  for(unsigned c=0;c<62;c++)app_layout_keycodes[l][c]=k[l*70+sm62_application_positions[c][0]*7+sm62_application_positions[c][1]];
 }
 strcpy(app_layout_name,"Synthetic browser");
 memset(app_layout_wake_identity,42,20);app_layout_wake_identity_seen=true;
 if(noise)for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++){
  rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
  app_layout_keycodes[l][c]=rng;app_layout_visuals[l][c]=rng>>16;app_layout_execution_states[l][c]=rng>>24&3;
  dynamic_keymap_set_keycode(l,sm62_application_positions[c][0],sm62_application_positions[c][1],rng>>8);
 }
 if(noise){
  for(unsigned l=0;l<16;l++){
   for(unsigned c=0;c<22;c++)app_layout_layer_names[l][c]=33+(l*23+c)%90;
   for(unsigned c=0;c<LAYER_LABEL_SIZE-1;c++)layer_names[l][c]=33+(l*17+c)%90;
  }
  for(unsigned c=0;c<22;c++)app_layout_name[c]=33+c;
 }
}
void ROLE_snapshot(void *p){memcpy(p,&sm62_app_tx,sizeof(sm62_app_tx));}
static sm62_app_image_t expected;
static bool watching;
void ROLE_watch(void){
 memset(&expected,0,sizeof(expected));
 memcpy(expected.keycodes,app_layout_keycodes,sizeof(app_layout_keycodes));
 memcpy(expected.visuals,app_layout_visuals,sizeof(app_layout_visuals));
 memcpy(expected.states,app_layout_execution_states,sizeof(app_layout_execution_states));
 memcpy(expected.name,app_layout_name,sizeof(app_layout_name));
 memcpy(expected.layer_names,app_layout_layer_names,sizeof(app_layout_layer_names));
 memcpy(expected.defaults,sm62_app_defaults,sizeof(sm62_app_defaults));
 memcpy(expected.default_keycodes,sm62_app_default_keycodes,sizeof(sm62_app_default_keycodes));watching=true;
}
void ROLE_atomic_all(void){if(!watching)return;
 assert(!memcmp(expected.keycodes,app_layout_keycodes,sizeof(app_layout_keycodes)));
 assert(!memcmp(expected.visuals,app_layout_visuals,sizeof(app_layout_visuals)));
 assert(!memcmp(expected.states,app_layout_execution_states,sizeof(app_layout_execution_states)));
 assert(!memcmp(expected.name,app_layout_name,sizeof(app_layout_name)));
 assert(!memcmp(expected.layer_names,app_layout_layer_names,sizeof(app_layout_layer_names)));
 assert(!memcmp(expected.defaults,sm62_app_defaults,sizeof(sm62_app_defaults)));
 assert(!memcmp(expected.default_keycodes,sm62_app_default_keycodes,sizeof(sm62_app_default_keycodes)));
}
void ROLE_check_snapshot(const void *p){
 const sm62_app_image_t *s=p;assert(sm62_app_rx_published&&!memcmp(s,&sm62_app_rx,sizeof(*s)));
 assert(!memcmp(s->keycodes,app_layout_keycodes,sizeof(app_layout_keycodes)));
 assert(!memcmp(s->visuals,app_layout_visuals,sizeof(app_layout_visuals)));
 assert(!memcmp(s->states,app_layout_execution_states,sizeof(app_layout_execution_states)));
 assert(!memcmp(s->name,app_layout_name,sizeof(app_layout_name)));
 assert(!memcmp(s->layer_names,app_layout_layer_names,sizeof(app_layout_layer_names)));
 assert(!memcmp(s->defaults,sm62_app_defaults,sizeof(sm62_app_defaults)));
 assert(!memcmp(s->default_keycodes,sm62_app_default_keycodes,sizeof(sm62_app_default_keycodes)));
 assert(app_layout_revision==s->revision && app_layout_wake_identity_seen==s->identity_seen&&!memcmp(s->identity,app_layout_wake_identity,20));watching=false;
}
'''
body=body.replace('extern void peer_unpublished_controls(void);','extern void peer_unpublished_controls(void); extern void peer_atomic_all(void);')
body=body.replace('if(packet[6])peer_unpublished_controls();','if(packet[6]){peer_unpublished_controls();} peer_atomic_all();')
# Explicit old-peer rejection of the new BEGIN, without weakening DATA checks.
body=body.replace('if(fault==2)return false;', 'if(fault==2){fault=0;return false;} if(fault==10 && packet[6] && packet[6]<31){fault=0;return false;}')
body=body.replace('if(fault==7 && !packet[6] && packet[4]==0xfe)', 'if(fault==7 && !packet[6] && (packet[4]==0xfe || packet[4]==0xfd))')
commands=[]
has_rle='static uint16_t sm62_app_encode(' in read('keyboards/ergohaven/sm62/application_split.inc')
if has_rle:seed+=Path(__file__).with_name('sm62_rle_controls.inc').read_text()
for role in ['master','peer']:
 src=P/(role+'.c');src.write_text(body.replace('ROLE_',role+'_')+extra.replace('ROLE_',role+'_')+seed.replace('ROLE_',role+'_'))
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
#include <stdlib.h>
#include <assert.h>
uint64_t clock_us;unsigned key_byte_us=2,rpc_us=2500,fault;bool link_ok=true,master_left;
void master_init(void);void peer_init(void);void master_task(void);void peer_task(void);void master_activate(void);void master_keepalive(void);void master_seed(const uint16_t*,bool);void master_stats(uint64_t*);void peer_stats(uint64_t*);void master_snapshot(void*);void peer_check_snapshot(const void*);void peer_watch(void);
void master_variant(unsigned);bool peer_variant_visible(unsigned);void master_reset(unsigned);void peer_reset(unsigned);bool peer_live(void);
unsigned master_encoded(void*);void peer_receiver_controls(const void*,unsigned,const void*);
void master_bad_format_controls(void*,unsigned*,uint16_t*);void peer_reject_format(const void*,unsigned,uint16_t);
static void tick(void){uint64_t before[10],after[10];master_stats(before);clock_us+=28928;master_keepalive();master_task();master_stats(after);assert(after[2]-before[2]<=4);peer_task();}
int main(int argc,char **argv){assert(argc==4);master_left=atoi(argv[1]);bool noise=atoi(argv[2]);fault=atoi(argv[3]);
 const uint16_t keys[1120]={KEYS};
 master_init();peer_init();master_seed(keys,noise);master_activate();peer_watch();
 uint64_t m[10],p[10];unsigned loops=0;
 do{tick();master_stats(m);peer_stats(p);assert(++loops<300);}while(!p[3]);
 uint8_t snapshot[10000];assert(m[7]<=sizeof(snapshot));master_snapshot(snapshot);peer_check_snapshot(snapshot);
 printf("cold noise=%u fault=%u bytes=%llu RPCs=%llu publication=%llu model_ms=%llu\n",noise,fault,(unsigned long long)m[7],(unsigned long long)m[2],(unsigned long long)p[3],(unsigned long long)(clock_us/1000));fflush(stdout);
 if(!noise&&!fault)assert(m[2]<=50 && "compressible factory-shaped cold image must reduce 214 DATA RPCs");
 if(noise||fault==7)assert(m[2]>=214);
 puts("PASS byte-identical final publication; no early partial fields; factory cold packet bound");
#if HAS_RLE
 if(!noise){uint8_t encoded[10000];unsigned length=master_encoded(encoded);assert(length);peer_receiver_controls(encoded,length,snapshot);uint16_t bad_crc;master_bad_format_controls(encoded,&length,&bad_crc);peer_reject_format(encoded,length,bad_crc);}
#endif
}
'''.replace('KEYS',','.join(str(int.from_bytes(fixture[i:i+2],'little')) for i in range(0,len(fixture),2)))
(P/'main.c').write_text(main)
cmd=['gcc',f'-DHAS_RLE={int(has_rle)}','-fsanitize=undefined','-fno-sanitize-recover=all',str(P/'main.c'),str(P/'master.o'),str(P/'peer.o'),'-o',str(P/'test_rle')];commands.append(cmd);subprocess.run(cmd,check=True)
for args in [['0','0','0'],['1','0','0'],['1','1','0'],['1','0','2'],['1','0','3'],['1','0','7'],['1','0','10']]:
 # Each scenario starts with a new pair of native production instances.
 cmd=[str(P/'test_rle'),*args];commands.append(cmd);subprocess.run(cmd,check=True)
(P/'test-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
