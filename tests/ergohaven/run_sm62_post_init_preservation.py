#!/usr/bin/env python3
"""Exercise actual SM62 post-init, migrator and native name-cache reload.
Hardware registration/timers are boundaries; persistence writes are recorded.
"""
from pathlib import Path
from native_layer_label_support import layer_label_source
import argparse,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def function(path,name):
 s=path.read_text();m=re.search(r'^(?:static )?[^\n;]+\b'+name+r'\([^;\n]*\)\s*\{',s,re.M);assert m,name
 e=s.index('{',m.start())+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
board=ROOT/'keyboards/ergohaven/sm62/sm62.c'
source=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "quantum/keycodes.h"
#include "keyboards/ergohaven/sm62/config.h"
_Static_assert(SM62_RESERVED_TD_BYTES == 40U && VIAL_COMBO_ENTRIES == 32 && VIAL_KEY_OVERRIDE_ENTRIES == 32, "exercise actual legacy geometry with TD disabled");
#ifdef VIAL_TAP_DANCE_ENABLE
#error "post-init preservation must exercise the normal TD-disabled configuration"
#endif
#undef SM62_PRESERVE_LEGACY_DEFAULT
#define SM62_PRESERVE_LEGACY_DEFAULT
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define KEYBOARD_ergohaven_sm62
#define MATRIX_ROWS 10
#define MATRIX_COLS 7
#define LAYER_LABEL_SIZE 16
#define KB_SETTINGS_LAYER_LABELS_OFFSET 64
#define TG(n) (QK_TOGGLE_LAYER | (n))
#define MO(n) (QK_MOMENTARY | (n))
#define RPC_SM62_DIAG 1
static uint8_t eeprom[1024];
static uint16_t keys[16][10][7],factory[10][7];
static char layer_names[16][16];
static bool master,layer_name_updated,diag_started;
static unsigned writes,cache_reads,display_init_calls;
static uint32_t diag_epoch;
static void eeconfig_read_kb_datablock(void *p,unsigned o,unsigned n){assert(o+n<=sizeof(eeprom));memcpy(p,eeprom+o,n);cache_reads++;}
static void eeconfig_update_kb_datablock(const void *p,unsigned o,unsigned n){assert(o+n<=sizeof(eeprom));memcpy(eeprom+o,p,n);writes++;}
static uint16_t dynamic_keymap_get_keycode(unsigned l,unsigned r,unsigned c){return keys[l][r][c];}
static void dynamic_keymap_set_keycode(unsigned l,unsigned r,unsigned c,uint16_t k){keys[l][r][c]=k;writes++;}
static uint16_t keycode_at_keymap_location_raw(unsigned l,unsigned r,unsigned c){(void)l;return factory[r][c];}
static const char *layer_name(unsigned l){return layer_names[l];}

static bool is_keyboard_master(void){return master;}
static uint32_t timer_read32(void){return 42;}
static void sm62_diag_stage(unsigned n){(void)n;}
static void display_init_kb(void){display_init_calls++;}
static void sm62_diag_rpc(void){}
static void transaction_register_rpc(unsigned id,void (*fn)(void)){(void)id;(void)fn;}
static void sm62_settings_register_rpc(void){}
static void hid_app_layout_split_init(void){}
'''+layer_label_source(ROOT)+function(board,'sm62_migrate_factory_defaults')+function(board,'keyboard_post_init_user')+r'''
int main(void){unsigned checks=0,generated_checks=0;
 static const char *const expected[16]={"Base","Num + Nav","Symbols","Adjust","4","5","6","7","8","9","10","11","12","13","14","15"};
 for(unsigned generated=0;generated<2;generated++)for(unsigned role=0;role<2;role++)for(unsigned pattern=0;pattern<4;pattern++)for(unsigned cached=0;cached<2;cached++){
  master=role;memset(eeprom,0xa5,sizeof(eeprom));
  for(unsigned l=0;l<16;l++){char name[16]={0};snprintf(name,16,"LAYER %u",l);memcpy(eeprom+64+l*16,name,16);}
  // A saved custom label opts the profile out of the intentional generated-
  // factory-label migration. Keep every legacy-like key signature below.
  if(!generated)memcpy(eeprom+64,"My saved base",14);
  for(unsigned r=0;r<10;r++)for(unsigned c=0;c<7;c++)factory[r][c]=0x100+r*7+c;
  for(unsigned l=0;l<16;l++)memcpy(keys[l],factory,sizeof(factory));
  if(pattern==0){keys[0][5][6]=KC_NO;keys[0][6][6]=KC_EQL;keys[0][7][6]=KC_QUOT;keys[0][8][6]=KC_BSLS;}
  if(pattern==1){keys[0][7][5]=KC_RBRC;keys[0][6][6]=KC_QUOT;}
  if(pattern==2){keys[0][9][0]=TG(1);keys[0][9][1]=KC_LGUI;keys[0][9][2]=KC_RALT;keys[0][9][3]=KC_ENT;}
  if(pattern==3)keys[0][4][0]=TG(1);
  uint8_t before[sizeof(eeprom)];uint16_t oldkeys[16][10][7];memcpy(before,eeprom,sizeof(before));memcpy(oldkeys,keys,sizeof(keys));
  for(unsigned boot=0;boot<2;boot++){
   if(cached)memcpy(layer_names,eeprom+64,sizeof(layer_names));else memset(layer_names,0,sizeof(layer_names));layer_name_updated=false;writes=cache_reads=display_init_calls=0;diag_started=false;
   keyboard_post_init_user();
   unsigned expected_writes=generated&&boot==0?16:0;
   if(writes!=expected_writes)fprintf(stderr,"unexpected stored Default writes=%u role=%u pattern=%u boot=%u\n",writes,role,pattern,boot);
   assert(writes==expected_writes);assert(memcmp(oldkeys,keys,sizeof(keys))==0);
   if(!generated)assert(writes==0); // Custom saved profiles never migrate.
   if(expected_writes){
    // Only the intentional label-owner migration may persist. Keymaps and
    // every EEPROM byte outside the native-name block remain untouched.
    assert(memcmp(before,eeprom,64)==0&&memcmp(before+320,eeprom+320,sizeof(eeprom)-320)==0);
    for(unsigned l=0;l<16;l++)assert(strcmp((char *)eeprom+64+l*16,expected[l])==0);
    memcpy(before,eeprom,sizeof(before));
   }else assert(memcmp(before,eeprom,sizeof(before))==0);
   assert(cache_reads>0&&layer_name_updated&&display_init_calls==1&&diag_started);
   for(unsigned l=0;l<16;l++)assert(memcmp(layer_names[l],eeprom+64+l*16,16)==0);
   if(generated)generated_checks++;else checks++;
  }
 }
 printf("PASS %u actual post-init boots: two roles, four legacy-like key signatures, warm/cold name cache, zero persistence writes; native names reloaded\n",checks);
 printf("PASS %u generated-profile boots: same roles, signatures and caches; factory labels migrate once without rewriting keys or unrelated EEPROM\n",generated_checks);
}
'''
src=a.output/'post_init.c';src.write_text(source);exe=a.output/'post_init'
subprocess.run(['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT),str(src),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(a.output/'results.log').write_text(r.stdout);print(r.stdout,end='')
if r.returncode: raise SystemExit(r.returncode)
# A disabled preservation guard must still expose the original mutating path.
negative=a.output/'without_guard.c';negative.write_text(source.replace('#define SM62_PRESERVE_LEGACY_DEFAULT\n',''))
negexe=a.output/'without_guard'
subprocess.run(['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT),str(negative),'-o',str(negexe)],check=True)
bad=subprocess.run([str(negexe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(a.output/'negative-control.log').write_text(bad.stdout)
assert bad.returncode != 0 and 'unexpected stored Default writes=14' in bad.stdout,bad.stdout
print('PASS guard-disabled negative control detects original automatic persistence writes')
