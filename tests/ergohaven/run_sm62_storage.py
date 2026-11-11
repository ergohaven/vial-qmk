#!/usr/bin/env python3
"""Actual Default-name handlers + EEPROM byte boundary; no app overlay writes."""
from pathlib import Path
import argparse,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=(ROOT/'keyboards/ergohaven/src/eh_settings.c').read_text()
def function(name):
 m=re.search(r'^(?:static )?[^\n;]+\b'+name+r'\([^;\n]*\)\s*\{',s,re.M);assert m,name
 e=s.index('{',m.start())+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
source=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define LAYER_LABEL_SIZE 16
#define KB_SETTINGS_LAYER_LABELS_OFFSET 64
#define dprintf(...) ((void)0)
typedef struct {uint16_t qsid;} qmk_settings_proto_t;
static char layer_names[16][16];static uint8_t eeprom[1024];static unsigned writes;
static bool layer_name_updated;
static void eeconfig_update_kb_datablock(const void *p,unsigned o,unsigned n){assert(o+n<=sizeof(eeprom));memcpy(eeprom+o,p,n);writes++;}
static void eeconfig_read_kb_datablock(void *p,unsigned o,unsigned n){assert(o+n<=sizeof(eeprom));memcpy(p,eeprom+o,n);}
'''+''.join(function(n) for n in ['kb_settings_layer_labels_init','layer_name_get','layer_name_set'])+r'''
int main(void){
 unsigned checks=0;
 for(unsigned l=0;l<16;l++){
  char name[16],readback[32]={0};snprintf(name,sizeof(name),"Default %u",l);
  qmk_settings_proto_t p={200+l};assert(layer_name_set(&p,name,sizeof(name))==0);
  assert(layer_name_updated);assert(layer_name_get(&p,readback,sizeof(readback))==0&&strcmp(name,readback)==0);checks++;
 }
 assert(writes==16);memset(layer_names,0,sizeof(layer_names));kb_settings_layer_labels_init();
 for(unsigned l=0;l<16;l++){char expected[16];snprintf(expected,sizeof(expected),"Default %u",l);assert(strcmp(layer_names[l],expected)==0);checks++;}
 qmk_settings_proto_t bad={216};char text[32];assert(layer_name_set(&bad,"invalid",8)==-1);assert(layer_name_get(&bad,text,sizeof(text))==-1);assert(writes==16);
 printf("PASS %u actual Default layer-name SET/GET/persistent reload assertions; qsid216 rejected, EEPROM layout unchanged\n",checks);
}
'''
src=a.output/'storage.c';src.write_text(source);exe=a.output/'storage'
subprocess.run(['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-format-security','-fsanitize=undefined','-fno-sanitize-recover=all',str(src),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(a.output/'results.log').write_text(r.stdout);print(r.stdout,end='');raise SystemExit(r.returncode)
