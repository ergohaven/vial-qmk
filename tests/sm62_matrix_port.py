#!/usr/bin/env python3
"""Source-backed GPIO and complete logical geometry checks; no hardware I/O.
Run: python3 tests/sm62_matrix_port.py --source ROOT --output ABS_CACHE_DIR
The native fixture compiles QMK's generated production LAYOUT macro.
"""
import argparse,json,pathlib,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--output',required=True);a=p.parse_args()
s=pathlib.Path(a.source).resolve();o=pathlib.Path(a.output)
assert o.is_absolute() and not o.resolve().is_relative_to(s), 'output must be absolute and outside source'
o.mkdir(parents=True,exist_ok=True)
k=json.loads((s/'keyboards/ergohaven/sm62/keyboard.json').read_text())
cols=['GP21','GP22','GP23','GP24','GP25','GP26','GP27'];rows=['GP28','GP19','GP18','GP17','GP16']
assert k['matrix_pins']['cols']==cols, f"current-board column GPIO mismatch: {k['matrix_pins']['cols']} != {cols}"
assert k['matrix_pins']['rows']==rows
assert k['diode_direction']=='COL2ROW'
cfg=(s/'keyboards/ergohaven/sm62/config.h').read_text()
assert '#define SPLIT_HAND_PIN GP29' in cfg
layout=k['layouts']['LAYOUT']['layout'];coords=[tuple(x['matrix']) for x in layout]
assert len(coords)==62 and len(set(coords))==62
assert all(0<=r<10 and 0<=c<7 for r,c in coords)
# Use QMK's real generator, not a reimplementation of the shipping macro.
env=__import__('os').environ.copy();env['PYTHONPATH']=str(s/'lib/python')+':/home/agent/.local/lib/python3.12/site-packages';env['PYTHONDONTWRITEBYTECODE']='1';env['QMK_HOME']=str(s)
r=subprocess.run(['qmk','generate-keyboard-h','-kb','ergohaven/sm62'],cwd=s,env=env,capture_output=True,text=True)
assert r.returncode==0,r.stderr
(o/'keyboard.h').write_text(r.stdout)
# Each source JSON key carries an independent unique sentinel, holes remain zero.
expected=[[0]*7 for _ in range(10)]
for i,(row,col) in enumerate(coords):expected[row][col]=i+1
c='#include <assert.h>\n#include <stdint.h>\n#define KC_NO 0\n#define ___ KC_NO\n#include "keyboard.h"\nint main(void){\nuint16_t actual[10][7]=LAYOUT('+','.join(str(i+1) for i in range(62))+');\n'
c+='const uint16_t expected[10][7]='+json.dumps(expected).replace('[','{').replace(']','}')+';\n'
c+='for(unsigned r=0;r<10;r++){for(unsigned c=0;c<7;c++){assert(actual[r][c]==expected[r][c]);}}\nreturn 0;}\n'
(o/'geometry.c').write_text(c)
# Generated header includes quantum.h, provide an empty hardware boundary only.
(o/'quantum.h').write_text('/* Native harness hardware boundary. */\n')
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(o),str(o/'geometry.c'),'-o',str(o/'geometry')],check=True)
subprocess.run([str(o/'geometry')],check=True)
print('PASS current GPIO; rows/diode/GP29; native production LAYOUT: 62 unique keys, 70 cells including 8 holes, both halves')
