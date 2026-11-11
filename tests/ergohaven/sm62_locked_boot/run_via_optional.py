#!/usr/bin/env python3
"""Focused optional-Vial compile/runtime control; actual VIA case and firewall.
Only the final NVM setter is a recorder. This is not the complete VIA dispatcher.
"""
import argparse, hashlib, json, re, subprocess, sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',required=True,type=Path);p.add_argument('--output-dir',required=True,type=Path)
a=p.parse_args();s=a.source_root.resolve();o=a.output_dir.resolve();r=Path(__file__).resolve().parent
if sys.flags.optimize:p.error('Python assertions must remain enabled')
protected={s,r}
for base in [s,r]:
 for ancestor in [base,*base.parents]:
  if (ancestor/'.git').exists():protected.add(ancestor);break
if not a.output_dir.is_absolute() or o.exists() or a.output_dir.is_symlink() or not o.parent.is_dir() or any(o==x or x in o.parents or o in x.parents for x in protected):p.error('fresh absolute external output required')
via=(s/'quantum/via.c').read_text();vial=(s/'quantum/vial.c').read_text();keys=(s/'quantum/keycodes.h').read_text()
def body(text,start):
 m=re.search(start,text,re.M);assert m,start
 i=text.index('{',m.start());j=i+1;depth=1
 while depth:depth+=(text[j]=='{')-(text[j]=='}');j+=1
 return text[m.start():j]
case=body(via,r'        case id_dynamic_keymap_set_keycode:\s*\{')
firewall=body(vial,r'^(?:__attribute__\(\(unused\)\) static )?uint16_t vial_keycode_firewall\(')
decls=[]
for name in ['QK_BOOTLOADER','QK_BOOT']:
 matches=re.findall(r'^\s*'+name+r'\s*=\s*[^,\n]+,',keys,re.M);assert len(matches)==1;decls+=matches
source='#include <stdint.h>\n#include <stdbool.h>\n#include <assert.h>\nenum {\n'+ '\n'.join(decls)+'\n};\n'
source+='bool vial_unlocked; static uint16_t observed;\nstatic void dynamic_keymap_set_keycode(unsigned l,unsigned r,unsigned c,uint16_t k){assert(l==0&&r==0&&c==5);observed=k;}\n'
source+='#ifdef VIAL_ENABLE\n'+firewall+'\n#endif\nenum {id_dynamic_keymap_set_keycode=5};\nstatic void ingress(uint8_t *command_data){switch(5){\n'+case+'\n}}\n'
source+='int main(void){uint8_t q[5]={0,0,5,0,37};ingress(q);assert(observed==37);q[3]=QK_BOOT>>8;q[4]=QK_BOOT&255;vial_unlocked=false;ingress(q);\n#ifdef VIAL_ENABLE\nassert(observed==0);\n#else\nassert(observed==QK_BOOT);\n#endif\nvial_unlocked=true;ingress(q);assert(observed==QK_BOOT);return 0;}\n'
o.mkdir();(o/'case.c').write_text(source);results=[]
for enabled in [False,True]:
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror',*(['-DVIAL_ENABLE'] if enabled else []),str(o/'case.c'),'-o',str(o/('case-'+str(int(enabled))))]
 c=subprocess.run(cmd,capture_output=True,text=True);(o/('compile-'+str(int(enabled))+'.log')).write_text(c.stdout+c.stderr)
 code=c.returncode
 if not code:code=subprocess.run([cmd[-1]]).returncode
 results.append(dict(vial_enabled=enabled,exit=code,command=cmd));print(results[-1],c.stderr)
(o/'result.json').write_text(json.dumps(dict(via_sha256=hashlib.sha256(via.encode()).hexdigest(),vial_sha256=hashlib.sha256(vial.encode()).hexdigest(),cases=results),indent=2))
raise SystemExit(0 if all(x['exit']==0 for x in results) else 1)
