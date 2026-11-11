#!/usr/bin/env python3
"""Actual pinned LVGL first-pass/resume/list-restart ownership regressions.
--timer-source permits an explicit pre-repair control, not a new baseline claim.
"""
from pathlib import Path
import argparse,json,subprocess
from macropad_native_support import validate_lvgl
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
p.add_argument('--timer-source',type=Path)
a=p.parse_args();F=a.source.resolve();O=a.output.resolve()
if F==O or F in O.parents:p.error('output must be outside firmware')
O.mkdir(parents=True,exist_ok=True);validate_lvgl(F/'lib/lvgl', profile='cooperative')
sources=sorted((F/'lib/lvgl/src').rglob('*.c'))
if a.timer_source:sources=[a.timer_source.resolve() if x.name=='lv_timer.c' else x for x in sources]
cmd=['gcc','-std=gnu11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_COOPERATIVE_REFR=1','-I'+str(F/'lib/lvgl'),'-I'+str(F/'lib/lvgl/src/misc'),str(Path(__file__).with_name('sm62_timer_ownership.c')),*map(str,sources),'-lm','-o',str(O/'timer')]
b=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(b.stdout+b.stderr)
assert b.returncode==0,b.stderr
results=[]
for mode in range(3):
 r=subprocess.run(['stdbuf','-o0',str(O/'timer'),str(mode)],capture_output=True,text=True)
 results.append(dict(mode=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr));print(results[-1])
(O/'results.json').write_text(json.dumps(dict(command=cmd,results=results),indent=2))
raise SystemExit(any(x['exit'] for x in results))
