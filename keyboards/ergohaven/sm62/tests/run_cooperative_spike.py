#!/usr/bin/env python3
"""Throwaway strip-yield feasibility. All elapsed costs are SYNTHETIC, not measurements.
Production source is extracted by run_scan_timing.py; no firmware source is edited.
"""
import os
import json
import resource
import re
import subprocess
from pathlib import Path
HERE = Path(__file__).resolve().parent
ns = {'__file__':str(HERE/'run_scan_timing.py')}
exec(compile((HERE/'run_scan_timing.py').read_text().split("c=a.output/")[0],str(HERE/'run_scan_timing.py'),'exec'),ns)
a, ROOT, text = (ns[k] for k in ['a','ROOT','text'])
# Keep real matrix_task, keyboard_task, asym_eager_defer_pk, quantum_task,
# Vial callbacks, app dispatch, KEYMAP, LVGL and production painter callback.
text=text[:text.index('int main(int argc,char **argv)')]
text=text.replace('static bool fired, physical_down;', 'static bool fired, physical_down;\nstatic unsigned pulse2, yield_enabled, strip_rate, singleton, ready, yielding, yield_count, charged_pixels, charged_ms, cpu_cost;\nstatic bool bus_locked;')
text=text.replace('physical_down=now>=100 && now<release_at;', 'physical_down=(now>=100 && now<release_at) || (pulse2 && now>=200 && now<250);')
old='static void qp_pixdata(void *d,void *p,uint32_t n){(void)d;(void)p;(void)n;if(cost_due){cost_due=false;printf("modeled_SPI begin=%u cost=%u\\n",now,block_cost);now+=block_cost;}}'
assert old in text
text=text.replace(old,'''static void qp_pixdata(void *d,void *p,uint32_t n){
 (void)d;(void)p;assert(!bus_locked);bus_locked=true;
 if(cost_due){
  if(singleton){cost_due=false;now+=block_cost;charged_ms+=block_cost;}
  else {unsigned ms=(n*strip_rate+6719)/6720;now+=ms;charged_ms+=ms;charged_pixels+=n;}
 }
 bus_locked=false;
}
static void cooperative_probe(void){
 if(!yield_enabled || !ready || yielding)return;
 assert(!bus_locked);yielding=1;yield_count++;keyboard_task();yielding=0;
}''')
text=text.replace('qp_flush(selected_display);','qp_flush(selected_display);\n        cooperative_probe();')
text=text.replace('cost_due=true;lv_refr_now(NULL);','cost_due=true;now+=cpu_cost;lv_refr_now(NULL);cost_due=false;')
text+=(HERE/'cooperative_spike_cases.inc').read_text()
c=a.output/'cooperative_spike.c'; c.write_text(text)
fonts=[ROOT/'keyboards/ergohaven/src/display/fonts'/f'eh_font_iosevka_{n}.c' for n in [18,28]]+[ROOT/'keyboards/ergohaven/macropad'/f'sm62_font_zen_medium_{n}.c' for n in [16,17,20,28]]
exe=a.output/'cooperative_spike'
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-type-limits','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_ASSERT_MALLOC=1',*['-I'+str(p) for p in [ROOT/'lib/lvgl',ROOT/'quantum',ROOT/'quantum/sequencer',ROOT/'quantum/keymap_extras',ROOT/'tmk_core/common',ROOT/'keyboards/ergohaven']],str(c),*map(str,fonts),*sorted(str(f) for f in (ROOT/'lib/lvgl/src').rglob('*.c')),'-lm','-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);(a.output/'build.log').write_text(r.stdout+r.stderr)
if r.returncode: print(r.stdout+r.stderr);raise SystemExit(r.returncode)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
# off, yield, strip-ms-per-6720px, singleton, start, release, double, CPU, init
cases=[
 ('off-single',[1,0,38,0,110,150,0,0,1]),
 ('on-single-baseline',[0,0,20,0,110,150,0,0,1]),
 ('on-single-coop20',[0,1,20,0,110,150,0,0,1]),
 ('on-hold-coop20',[0,1,20,0,110,450,0,0,1]),
 ('off-double',[1,0,38,0,110,150,1,0,1]),
 ('on-double-baseline',[0,0,20,0,110,150,1,0,1]),
 ('on-double-coop20',[0,1,20,0,110,150,1,0,1]),
 ('on-double-coop38',[0,1,38,0,110,150,1,0,1]),
 ('singleton200-coop',[0,1,200,1,110,150,0,0,1]),
 ('cpu200-coop',[0,1,0,0,110,150,0,200,1]),
 ('singleton181-characterization',[0,1,181,1,110,150,0,0,1]),
 ('startup-guard',[0,1,20,0,110,150,0,0,0]),
 ('neighborhood-coop20',[0,1,20,0,90,150,0,0,1]),
]
results=[]
expected_actions=[(1,0,0),(0,1,0),(1,0,0),(0,1,0),(0,0,1),(0,1,0),(0,0,1),(1,0,0),(0,1,0),(0,1,0),(1,0,0),(0,1,0),(1,0,0)]
for index,(name,args) in enumerate(cases):
 r=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True)
 log=r.stdout+r.stderr;(a.output/(name+'.log')).write_text(log)
 print(name, 'exit', r.returncode,log.splitlines()[-1] if log else '')
 assert r.returncode==0,log
 values={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',log.split('RESULT ')[-1])}
 assert tuple(values[k] for k in ['tap','hold','double'])==expected_actions[index],(name,values)
 if 'coop20' in name:assert values['max_scan_gap']==20 and values['px']==67200 and values['modeled_ms']==200
 if name=='neighborhood-coop20':assert (values['recent_row'],values['recent_col'])==(0,0)
 results.append({'name':name,'args':args,'exit':r.returncode,'values':values,'log':log})
# Actual expected-behavior RED controls: no crash or compiler failure masquerades as RED.
red=[]
for name,args,expect in [
 ('RED-baseline-single',cases[1][1],'tap'),
 ('RED-coop-double38',cases[7][1],'double'),
 ('RED-uninterruptible200',cases[8][1],'tap'),
 ('RED-neighborhood',cases[12][1],'neighborhood'),
]:
 r=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,env=dict(os.environ,SPIKE_EXPECT=expect))
 (a.output/(name+'.log')).write_text(r.stdout+r.stderr)
 print(name,r.returncode)
 assert r.returncode==-6,(name,r.stdout,r.stderr)
 red.append({'name':name,'exit':r.returncode,'log':r.stdout+r.stderr})
(a.output/'results.json').write_text(json.dumps({'characterization':results,'expected_behavior_red':red},indent=2))
assert len(results)==13 and len(red)==4
print('13 characterizations; 4 behavioral RED controls. Strip-only candidate NOT qualified.')
