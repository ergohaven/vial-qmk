#!/usr/bin/env python3
"""Actual scan/debounce/native TD + KEYMAP/LVGL/painter/service.
Only physical matrix, timer/work costs, EEPROM and USB/SPI are fixtures.
Costs are synthetic budgets, never MCU measurements.
"""
import os,json,resource,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ns={'__file__':str(HERE/'run_scan_timing.py')}
exec(compile((HERE/'run_scan_timing.py').read_text().split('c=a.output/')[0],str(HERE/'run_scan_timing.py'),'exec'),ns)
a,ROOT,text=(ns[k] for k in ['a','ROOT','text'])
text=text[:text.index('int main(int argc,char **argv)')]
text=text.replace('static bool fired, physical_down;', '''static bool fired, physical_down;
static unsigned pulse2,yield_enabled,strip_rate,singleton,ready,yielding,yield_count,charged_pixels,charged_ms,cpu_cost,contact_glitch,transferred_pixels;
static bool bus_locked;''')
text=text.replace('static unsigned frames,flushes;', 'static unsigned frames,flushes;\nstatic lv_color_t panel_pixels[240*280];static unsigned vx,vy,vw,vh;')
text=text.replace('static void qp_viewport(void *d,int x1,int y1,int x2,int y2){(void)d;(void)x1;(void)y1;(void)x2;(void)y2;}', 'static void qp_viewport(void *d,int x1,int y1,int x2,int y2){(void)d;vx=x1;vy=y1;vw=x2-x1+1;vh=y2-y1+1;}')
text=text.replace('physical_down=now>=100 && now<release_at;', 'physical_down=((now>=100 && now<release_at) || (pulse2 && now>=200 && now<250)) && !(contact_glitch && now>=150 && now<150+contact_glitch);')
old='static void qp_pixdata(void *d,void *p,uint32_t n){(void)d;(void)p;(void)n;if(cost_due){cost_due=false;printf("modeled_SPI begin=%u cost=%u\\n",now,block_cost);now+=block_cost;}}'
assert old in text
text=text.replace(old,'''static void qp_pixdata(void *d,void *p,uint32_t n){
 (void)d;assert(!bus_locked);bus_locked=true;
 assert(n==vw*vh && vx+vw<=240 && vy+vh<=280);
 transferred_pixels+=n;
 for(unsigned y=0;y<vh;y++)memcpy(panel_pixels+(vy+y)*240+vx,(lv_color_t*)p+y*vw,vw*sizeof(lv_color_t));
 if(cost_due){
  unsigned ms=singleton ? block_cost : (n*strip_rate+6719)/6720;
  now+=ms;charged_ms+=ms;charged_pixels+=n;
  if(singleton)cost_due=false;
 }
 bus_locked=false;
}''')
text=text.replace('cost_due=true;lv_refr_now(NULL);','cost_due=true;now+=cpu_cost;lv_refr_now(NULL);cost_due=false;')
# Count actual service entries, without injecting an extra scan.
text=text.replace('    protocol_keyboard_task();\n}', '    assert(!bus_locked);yield_count++;protocol_keyboard_task();\n}')
cases=(HERE/'cooperative_spike_cases.inc').read_text()
cases=cases.replace('now=90;combo_disable();','contact_glitch=getenv("CONTACT_GLITCH")?atoi(getenv("CONTACT_GLITCH")):0;now=90;combo_disable();')
cases=cases.replace('lv_disp_draw_buf_init(&buf,pixels,NULL,240*28);','lv_disp_draw_buf_init(&buf,pixels,NULL,240*(getenv("REFERENCE28")?28:4));')
cases=cases.replace('lv_refr_now(NULL);flushes=0;yield_count=0;', '''lv_refr_now(NULL);flushes=0;yield_count=0;transferred_pixels=0;
    unsigned pixel_hash=2166136261u;for(unsigned i=0;i<sizeof(panel_pixels);i++)pixel_hash=(pixel_hash^((uint8_t*)panel_pixels)[i])*16777619u;
    printf("STARTUP_PIXEL_HASH=%u\\n",pixel_hash);''')
cases=cases.replace('if(!ready || !yield_enabled)assert(yield_count==0);','')
cases=cases.replace('debounce_free();return 0;', '''if(expect && !strcmp(expect,"hold"))assert(adds[KC_T]==1 && !adds[KC_5] && !adds[KC_R]);
    assert(charged_pixels==67200 || singleton || off);
    debounce_free();return 0;''')
cases=cases.replace('    printf("RESULT ', '    printf("TRANSFERRED_PIXELS=%u\\n",transferred_pixels);assert(transferred_pixels==(off?0:67200));\n    printf("RESULT ')
text+=cases
extra='''
    if (getenv("CAPTURE_CHECKS")) {
        for (unsigned side=0;side<2;side++) {
            side_left=side;uint8_t base=side ? 0 : 5;
            keyrecord_t r={.event={.key={.row=base+1,.col=2},.pressed=true,.type=KEY_EVENT,.time=now}};
            layer_state=1;
            uint16_t dirty=lv_disp_get_default()->inv_p;
            pre_process_record_kb(KC_A,&r);
            assert(captured_press.pending && captured_press.row==1 && captured_press.col==2 && captured_press.layer==0);
            assert(lv_disp_get_default()->inv_p==dirty);
            r.event.key.col=3;pre_process_record_kb(KC_A,&r);
            assert(captured_press.col==3);
            r.event.key.row=254;pre_process_record_kb(KC_A,&r);
            assert(captured_press.col==3 && captured_press.row==1);
            r.event.key.row=base+1;r.event.key.col=99;pre_process_record_kb(KC_A,&r);
            assert(captured_press.col==3);
            r.event.key.col=3;r.event.type=TICK_EVENT;pre_process_record_kb(KC_A,&r);
            assert(captured_press.col==3);
            r.event.type=KEY_EVENT;r.event.key.row=side ? 6 : 1;pre_process_record_kb(KC_A,&r);
            assert(captured_press.col==3 && captured_press.row==1);
            r.event.key.row=base+1;r.event.pressed=false;pre_process_record_kb(KC_A,&r);
            assert(captured_press.released && captured_press.released_at==now);
            layer_state=2;assert(captured_press.layer==0);
            assert(sm62_keymap_observe_matrix());
            assert(!captured_press.pending && recent_press_row==1 && recent_press_col==3);
        }
        puts("PASS capture both sides/latest/release/press-layer/magic/out-of-bounds/tick/other-side/no-LVGL-dirt");
    }
'''
text=text.replace('    debounce_free();return 0;',extra+'    debounce_free();return 0;')
c=a.output/'display_candidate.c';c.write_text(text)
fonts=[ROOT/'keyboards/ergohaven/src/display/fonts'/f'eh_font_iosevka_{n}.c' for n in [18,28]]+[ROOT/'keyboards/ergohaven/macropad'/f'sm62_font_zen_medium_{n}.c' for n in [16,17,20,28]]
exe=a.output/'display_candidate'
cmd=['gcc','-include','/home/agent/.hermes/profiles/ben/cache/scratch/sm62-test16-final/firmware/quantum/sm62_trace.h','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-type-limits','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_ASSERT_MALLOC=1',*['-I'+str(p) for p in [ROOT/'lib/lvgl',ROOT/'quantum',ROOT/'quantum/sequencer',ROOT/'quantum/keymap_extras',ROOT/'tmk_core/common',ROOT/'keyboards/ergohaven']],str(c),*map(str,fonts),*sorted(str(f) for f in (ROOT/'lib/lvgl/src').rglob('*.c')),'-lm','-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);(a.output/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
results=[]
def run(name,args,expect,status=0):
 r=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,env=dict(os.environ,SPIKE_EXPECT=expect))
 log=r.stdout+r.stderr;(a.output/(name+'.log')).write_text(log)
 values={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',log.split('RESULT ')[-1])}
 pixel_hash=int(re.search(r'STARTUP_PIXEL_HASH=(\d+)',log)[1])
 sent_pixels=int(re.search(r'TRANSFERRED_PIXELS=(\d+)',log)[1])
 results.append({'name':name,'args':args,'exit':r.returncode,'expect':expect,'values':values,'startup_pixel_hash':pixel_hash,'transferred_pixels':sent_pixels})
 print(name,r.returncode,log.split('RESULT ')[-1].strip())
 assert r.returncode==status,(name,log)
 return values
for name,release,double,expect,start in [('double',150,1,'double',110),('neighborhood',150,0,'neighborhood',90),('tap',150,0,'tap',110),('hold',450,0,'hold',110)]:
 v=run(name,[0,1,38,0,start,release,double,0,1],expect)
 assert v['max_scan_gap']==6 and v['px']==67200 and v['yields']==70
# Independent total strip budgets; NOT 181ms divided into pieces.
for budget in [1,2,4,6,8,10,12,16]:
 for start in [90,95,100,105,110,115]:
  run(f'budget{budget}-phase{start}',[0,1,budget*7,0,start,150,1,0,1],'double')
for glitch in range(1,10):
 os.environ['CONTACT_GLITCH']=str(glitch)
 for off in [1,0]:
  for rate in [38,112]:
   run(f'contact-glitch{glitch}-off{off}-rate{rate}',[off,1,rate,0,110,450,0,0,1],'hold')
os.environ.pop('CONTACT_GLITCH')
run('truehold320-single-SPI220',[0,1,220,1,110,320,0,0,1],'hold')
run('negative-SPI200',[0,1,200,1,110,150,0,0,1],'tap',-6)
run('negative-CPU200',[0,1,0,0,110,150,0,200,1],'tap',-6)
run('negative-unseen-entire-pulse',[0,1,0,0,90,150,0,200,1],'tap',-6)
os.environ['CAPTURE_CHECKS']='1'
run('capture-identity-and-invalid-events',[0,1,38,0,90,150,0,0,1],'neighborhood')
os.environ.pop('CAPTURE_CHECKS')
os.environ['REFERENCE28']='1'
run('pixel-reference28',[0,1,0,0,110,150,0,0,1],'tap')
assert len({r['startup_pixel_hash'] for r in results})==1
(a.output/'results.json').write_text(json.dumps(results,indent=2))
print('PASS',len(results),'executed (including 3 expected-negative unbounded stalls/unseen pulse); no hardware PASS')
