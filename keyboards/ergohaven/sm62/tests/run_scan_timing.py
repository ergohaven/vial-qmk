#!/usr/bin/env python3
"""Native scan/debounce/Vial-TD + actual KEYMAP/LVGL. Injected costs are NOT MCU measurements."""
import sys
import json
import hashlib
import subprocess
from pathlib import Path
HERE = Path(__file__).resolve().parent
# Reuse only the reviewed test generator, never retired production.
ns = {'__file__': str(HERE/'run_repeat_display.py')}
prefix = (HERE/'run_repeat_display.py').read_text().split("cases = (HERE/")[0]
exec(compile(prefix, str(HERE/'run_repeat_display.py'), 'exec'), ns)
a, ROOT, body, stripped, source = (ns[k] for k in ['a','ROOT','body','stripped','source'])
header = ns['header'].replace('static void action_exec(keyevent_t e) { (void)e; assert(!"complex native Vial action is outside fixture"); }','static void action_exec(keyevent_t e);')
display = ns['display'].replace('static struct {uint8_t mode,cyrillic_enabled;} local_settings={3,1};', 'typedef struct {uint8_t mode,cyrillic_enabled,keymap_enabled,ship_enabled,ship_timeout_s;} sm62_side_settings_t;\nstatic sm62_side_settings_t local_settings={3,1,1,0,0};')
native = ns['native'].replace('static void on_dance_finished(tap_dance_state_t *state, void *user_data) {', 'static void on_dance_finished(tap_dance_state_t *state, void *user_data) { printf("native_finish t=%u pressed=%u count=%u interrupted=%u\\n",now,state->pressed,state->count,state->interrupted);')
base = header + ns['ns']['production'] + ns['headers'] + ns['fixture'] + ns['native_prefix'] + native + ns['processors'] + display + ns['ns']['pre'] + ns['ns']['kb'] + body(ns['ns']['quantum'], 'uint16_t get_record_keycode(') + body(ns['ns']['quantum'], 'bool pre_process_record_quantum(') + ns['ns']['chain']
keyboard = source('quantum/keyboard.c')
debounce = stripped(source('quantum/debounce/asym_eager_defer_pk.c'))
assert '#define DEBOUNCE 10' in source('keyboards/ergohaven/config.h')
assert 'DEBOUNCE_TYPE = asym_eager_defer_pk' in source('keyboards/ergohaven/sm62/rules.mk')
scan = '''
typedef uint8_t matrix_row_t;
typedef uint32_t fast_timer_t;
#define TIMER_DIFF_FAST(a,b) ((fast_timer_t)((a)-(b)))
#define TIMER_DIFF_16(a,b) ((uint16_t)((a)-(b)))
#define timer_read_fast timer_read32
#define DEBOUNCE 10
#define MAKE_TICK_EVENT ((keyevent_t){.type=TICK_EVENT,.time=now})
#define MAKE_KEYEVENT(r,c,p) ((keyevent_t){.key={.row=r,.col=c},.pressed=p,.time=now,.type=KEY_EVENT})
static matrix_row_t raw_rows[MATRIX_ROWS], cooked_rows[MATRIX_ROWS];
static unsigned release_at, block_at=110, block_cost, split_cost;
static unsigned raw_release_at, cooked_release_at, hold_at, tap_at, last_scan, max_scan_gap;
static bool fired, physical_down;
static void process_record(keyrecord_t *r) {
    uint16_t k=get_record_keycode(r,true);
    if (preprocess_tap_dance(k,r)) k=get_record_keycode(r,true);
    if(selected_quantum_chain(k,r) && k<256) { if(r->event.pressed)add_key(k);else del_key(k); }
}
static void action_exec(keyevent_t e) {
    if(e.type!=KEY_EVENT)return;
    printf("event t=%u pressed=%u physical=%u raw=%u cooked=%u\\n",now,e.pressed,physical_down,!!raw_rows[1],!!cooked_rows[1]);
    if(!e.pressed)cooked_release_at=now;
    keyrecord_t r={.event=e};if(pre_process_record_quantum(&r))process_record(&r);
}
static bool matrix_can_read(void) { return true; }
static matrix_row_t matrix_get_row(uint8_t r) { return cooked_rows[r]; }
static void vial_unlock_task(void) {}
static void matrix_scan_perf_task(void) {}
static void matrix_print(void) {}
static struct {bool matrix;} debug_config;
static bool should_process_keypress(void) { return true; }
static bool has_ghost_in_row(uint8_t r,matrix_row_t v) {(void)r;(void)v;return false;}
static void last_matrix_activity_trigger(void) {}
static void led_task(void) {}
'''
scan += debounce
scan += '''
static void matrix_scan(void) {
    if(last_scan && now-last_scan>max_scan_gap)max_scan_gap=now-last_scan;
    last_scan=now;
    physical_down=now>=100 && now<release_at;
    matrix_row_t value=physical_down ? (1u<<2) : 0;
    bool changed=raw_rows[1]!=value;
    if(raw_rows[1] && !value){raw_release_at=now;printf("raw_release t=%u physical_edge=%u\\n",now,release_at);}
    raw_rows[1]=value;debounce(raw_rows,cooked_rows,MATRIX_ROWS/2,changed);
    for(unsigned r=0;r<MATRIX_ROWS;r++)for(unsigned c=0;c<MATRIX_COLS;c++)matrix[r][c]=!!(cooked_rows[r]&(1u<<c));
}
'''
scan += ''.join(body(keyboard,s) for s in ['void switch_events(', 'static inline void generate_tick_event(', 'static bool matrix_task(', 'void quantum_task(', 'void keyboard_task('])
# Execute the actual production on/off screen policy; settings/power and unused screens are boundaries.
policy = source('keyboards/ergohaven/sm62/display_modes.c')
policy = policy[policy.index('typedef enum'):policy.index('void display_init_screens_kb(')] + policy[policy.index('void display_housekeeping_task('):]
policy_boundary = '''
static bool off;
#define SM62_DISPLAY_SYNC_DELAY_MS 2500U
static bool sm62_local_screens_off(void){return off;}
static void hid_housekeeping_task(void){}
static bool sm62_local_mode_transition_pending(void){return false;}
static bool is_display_enabled(void){return true;}
static bool display_init_kb(void){return true;}
static uint32_t get_split_lcd_timeout_ms(void){return 0;}
static uint32_t last_matrix_activity_elapsed(void){return 0;}
static bool is_keyboard_master(void){return true;}
static void display_turn_off(void){}
static void display_turn_on(void){}
static uint32_t sm62_display_boot_elapsed(void){return 3000;}
static uint32_t sm62_local_orbit_idle_ms(uint32_t t){return t;}
static void display_refresh(void){lv_obj_invalidate(lv_scr_act());}
static void nop(void){}
static void sm62_fps_frame(uint8_t s){(void)s;}
static void sm62_diag_frame(void){}
static void sm62_main_frame_completed(void){}
static void sm62_player_frame_completed(void){}
static void sm62_orbit_frame_completed(void){}
static void sm62_orbit_restart(void){}
static const eh_screen_t eh_screen_clock={nop,nop,nop},eh_screen_player={nop,nop,nop},eh_screen_orbit={nop,nop,nop};
'''
# Actual painter callback, with cost charged at the unavailable hardware pixdata boundary.
flush = body(source('quantum/painter/lvgl/qp_lvgl.c'), 'void qp_lvgl_flush(')
flush_boundary = '''
static void *selected_display=(void*)1;
static bool cost_due;
static unsigned frames,flushes;
static void qp_viewport(void *d,int x1,int y1,int x2,int y2){(void)d;(void)x1;(void)y1;(void)x2;(void)y2;}
static void qp_pixdata(void *d,void *p,uint32_t n){(void)d;(void)p;(void)n;if(cost_due){cost_due=false;printf("modeled_SPI begin=%u cost=%u\\n",now,block_cost);now+=block_cost;}}
static void qp_flush(void *d){(void)d;flushes++;}
'''
loop_boundary = '''
static void protocol_pre_task(void){}
static void protocol_post_task(void){}
static void sm62_fps_tick(void){}
static int fps_data;
static void fps_duration(void *d,uint32_t a,uint32_t b,unsigned i){(void)d;(void)a;(void)b;(void)i;}
static void sm62_settings_sync_task(void){}
static void sm62_diag_house(uint32_t t){(void)t;}
static void hid_app_layout_split_task(void){if(fired && split_cost){printf("modeled_split begin=%u cost=%u\\n",now,split_cost);now+=split_cost;split_cost=0;}}
static void qp_internal_task(void){
 if(!fired && now>=block_at){fired=true;if(!off)lv_obj_invalidate(lv_scr_act());cost_due=true;lv_refr_now(NULL);}
}
'''
main_source=source('quantum/main.c')
loop = body(main_source, 'void protocol_keyboard_task(void) {')
service = stripped(source('keyboards/ergohaven/sm62/display_input.c'))
loop = ''  # use the production SM62 wrapper, not QMK's weak default
# Execute the original main-loop body, retaining actual calls/order. Hardware USB
# boundaries and painter scheduler are explicit fixtures, not extra scan calls.
loop += 'static void housekeeping_task(void){housekeeping_task_user();}\n'
loop += 'static void replay_iteration(void){\n'+main_source.split('    while (true) {',1)[1].rsplit('    }',1)[0]+'}\n'
loop = '#define QUANTUM_PAINTER_ENABLE\n' + body(source('keyboards/ergohaven/sm62/sm62.c'),'void housekeeping_task_user(') + loop
cases=(HERE/'scan_timing_cases.inc').read_text()
text=base+scan+policy_boundary+policy+flush_boundary+service+flush+loop_boundary+loop+cases
c=a.output/'scan_timing.c';c.write_text(text)
fonts=[ROOT/'keyboards/ergohaven/src/display/fonts'/f'eh_font_iosevka_{n}.c' for n in [18,28]]+[ROOT/'keyboards/ergohaven/macropad'/f'sm62_font_zen_medium_{n}.c' for n in [16,17,20,28]]
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-type-limits','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_ASSERT_MALLOC=1',*['-I'+str(p) for p in [ROOT/'lib/lvgl',ROOT/'quantum',ROOT/'quantum/sequencer',ROOT/'quantum/keymap_extras',ROOT/'tmk_core/common',ROOT/'keyboards/ergohaven']],str(c),*map(str,fonts),*sorted(str(f) for f in (ROOT/'lib/lvgl/src').rglob('*.c')),'-lm','-o',str(a.output/'scan_timing')]
r=subprocess.run(cmd,capture_output=True,text=True);(a.output/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
results=[]
# Separate identical physical edges, native actions and term; only modeled work changes.
for off in [1,0]:
 for cost in [0,16,50,150,180,181,189,190,191,200,220]:
  r=subprocess.run([str(a.output/'scan_timing'),str(off),str(cost),'0','150'],capture_output=True,text=True)
  log=r.stdout+r.stderr;(a.output/f'off{off}-spi{cost}.log').write_text(log)
  print(log,end='');results.append({'off':off,'spi_cost':cost,'exit':r.returncode,'log':log})
# Split work still precedes display OFF check: same outcome without any SPI work.
for off in [1,0]:
 r=subprocess.run([str(a.output/'scan_timing'),str(off),'0','200','150'],capture_output=True,text=True)
 log=r.stdout+r.stderr;(a.output/f'off{off}-split200.log').write_text(log);print(log,end='');results.append({'off':off,'split_cost':200,'exit':r.returncode,'log':log})
(a.output/'results.json').write_text(json.dumps(results,indent=2))
(a.output/'generated.sha256').write_text(hashlib.sha256(c.read_bytes()).hexdigest()+'  scan_timing.c\n')
assert len(results)==24 and all(r['exit']==0 for r in results)
# A separate behavioral assertion must actually go RED, not merely print HOLD.
# This is RED of the injected scheduling mechanism, NOT measured physical cause.
import os
import resource
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
red=[]
for off in [1,0]:
 r=subprocess.run([str(a.output/'scan_timing'),str(off),'200','0','150'],capture_output=True,text=True,env=dict(os.environ,EXPECT_TAP='1'))
 (a.output/f'EXPECT_TAP-off{off}.log').write_text(r.stdout+r.stderr)
 red.append({'off':off,'exit':r.returncode,'log':r.stdout+r.stderr})
(a.output/'red-control.json').write_text(json.dumps(red,indent=2))
assert red[0]['exit']==0 and red[1]['exit']==-6
print('CHARACTERIZATION 24/24 executed; EXPECT_TAP: OFF exit 0, ON SIGABRT (-6), uninterruptible modeled cost 200ms. No physical GREEN.')
