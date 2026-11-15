#!/usr/bin/env python3
"""Production owner and pinned LVGL with native producer/panel/clock fixtures.

This exercises ownership and backpressure, not GPIO, UART or USB latency.
"""
import argparse
import re
import resource
import runpy
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
# Reuse the tested producer boundary and run its regression before consumption.
saved_argv = sys.argv
try:
    sys.argv = [str(HERE / "run_render_exchange.py"), "--output", str(args.output / "capture")]
    capture = runpy.run_path(str(HERE / "run_render_exchange.py"))
finally:
    sys.argv = saved_argv
strip, body = capture["strip"], capture["body"]
header, fixture = capture["header"], capture["fixture"]
fixture = fixture.replace("bool sm62_render_test_owner(void) { return false; }",
                          "static _Thread_local bool owner;\nbool sm62_render_test_owner(void) { return owner; }")
fixture = fixture.replace('void backlight_level_noeeprom(uint8_t x) { (void)x; }',
    'static uint8_t native_pwm;\nvoid backlight_level_noeeprom(uint8_t x) { assert(!owner);native_pwm=x; }')
owner = strip((HERE.parent / "render_owner.c").read_text())
flush = body((ROOT / "quantum/painter/lvgl/qp_lvgl.c").read_text(), "void qp_lvgl_flush(")
boundary = r'''
static bool enabled, panel_active, pixel_finished;
static unsigned mode, initialized, panel_flushes, native_turns, power_offs;
static _Atomic bool entered, release_owner;
static uint64_t monotonic_us(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000U+t.tv_nsec/1000U;
}
static void stall(void) {
    atomic_store(&entered,true);
    while(!atomic_load(&release_owner)) {
        struct timespec t={0,100000}; nanosleep(&t,NULL);
    }
}
static void draw(lv_event_t *event) { (void)event; if(mode==1)stall(); }
static void *selected_display=(void *)1;
void qp_lvgl_flush(lv_disp_drv_t *,const lv_area_t *,lv_color_t *);
static bool qp_viewport(void *p,int x1,int y1,int x2,int y2) {
    (void)p;(void)x1;(void)y1;(void)x2;(void)y2;
    assert(owner);pixel_finished=false;return true;
}
static bool qp_pixdata(void *p,void *data,uint32_t n) {
    (void)p;(void)data;(void)n;assert(owner);panel_active=true;
    if(mode==2)stall();
    panel_active=false;pixel_finished=true;panel_flushes++;return mode!=4;
}
static bool qp_flush(void *p) { (void)p;assert(owner && !panel_active);return true; }
static bool qp_comms_spi_transaction_begin(void *p) { (void)p;assert(owner);return true; }
static bool qp_comms_spi_transaction_end(void *p) { (void)p;assert(owner);return true; }
void display_frame_completed(void) {
    assert(owner && !panel_active && pixel_finished);
    sm62_render_frame(0);
}
void sm62_flash_core1_poll(bool quiescent) { assert(owner && quiescent); }
void sm62_dc_trace_poll(void) { assert(owner); }
bool sm62_spi_quiescent(void) { return !panel_active; }
bool is_display_enabled(void) { assert(owner);return enabled; }
bool display_init_kb(void) {
    assert(owner);initialized++;
    if(mode==3)return false;
    lv_init();
    static lv_color_t pixels[240*5];
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,240*5);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);
    driver.hor_res=240;driver.ver_res=280;driver.draw_buf=&buffer;
    driver.flush_cb=qp_lvgl_flush;lv_disp_drv_register(&driver);
    lv_obj_add_event_cb(lv_scr_act(),draw,LV_EVENT_DRAW_MAIN_BEGIN,NULL);
    enabled=true;return true;
}
void display_turn_off(void) { assert(owner);power_offs++;sm62_render_pwm(0); }
void display_apply_accent_color(uint8_t a,uint8_t b,uint8_t c) { (void)a;(void)b;(void)c;assert(owner); }
void display_apply_background_color(uint8_t a,uint8_t b,uint8_t c) { (void)a;(void)b;(void)c;assert(owner); }
void display_apply_button_style(uint8_t b) { (void)b;assert(owner); }
void display_apply_clock_settings(void) { assert(owner); }
void display_apply_brightness(void) { assert(owner); }
void display_refresh(void) { assert(owner); }
void display_housekeeping_task(void) { assert(owner);lv_obj_invalidate(lv_scr_act()); }
void sm62_keymap_owner_feedback(uint64_t p,uint64_t r,bool last,uint8_t row,uint8_t col) {
    (void)p;(void)r;(void)last;(void)row;(void)col;assert(owner);
}
uint8_t sm62_diag_screen(void) { assert(owner);return 0; }
void qp_internal_task(void) { assert(owner);lv_tick_inc(8);lv_timer_handler(); }
static void keyboard_task(void) { assert(!owner);native_turns++; }
'''
cases = r'''
static void *render(void *unused) {
    (void)unused;owner=true;sm62_render_worker_step();if(mode==1 || mode==2)sm62_render_pwm(80);return NULL;
}
int main(int argc,char **argv) {
    assert(argc==2);mode=atoi(argv[1]);
    if(mode==5)side_left=false;
    sm62_render_ready();
    for(unsigned i=0;i<300;i++)sm62_render_capture();
    pthread_t thread;assert(!pthread_create(&thread,NULL,render,NULL));
    if(mode==1 || mode==2) {
        uint64_t deadline=monotonic_us()+2000000;
        while(!atomic_load(&entered)){assert(monotonic_us()<deadline);sched_yield();}
        uint32_t frames=sm62_render_frames();
        assert(frames==0);
        uint64_t until=monotonic_us()+200000;
        do { protocol_keyboard_task();sm62_render_capture(); } while(monotonic_us()<until);
        assert(native_turns>1000 && sm62_render_frames()==frames);
        sm62_render_suspend(true);sm62_render_suspend(false);
        atomic_store(&release_owner,true);
    }
    assert(!pthread_join(thread,NULL));
    assert(initialized==1 && !sm62_render_busy());
    if(mode==3)assert(panel_flushes==0 && sm62_render_frames()==0);
    else if(mode==4)assert(panel_flushes==1 && sm62_render_frames()==0 && !sm62_render_enabled());
    else assert(panel_flushes>0 && sm62_render_frames()>0);
    if(mode==4) {
        /* The last good PWM must not survive a frozen panel or a wake epoch. */
        native_pwm=80;
        owner=true;sm62_render_worker_step();owner=false;
        sm62_render_capture();assert(native_pwm==0);
        sm62_render_suspend(true);sm62_render_suspend(false);native_pwm=80;
        owner=true;sm62_render_worker_step();sm62_render_worker_step();owner=false;
        sm62_render_capture();assert(native_pwm==0);
    }
    if(mode!=3) {
        uint8_t r=side_left?4:9;
        assert(sm62_view_native_code(15,r,6)==code+r);
        assert(sm62_view_effective_code(15,r,6)==code+r);
        assert(sm62_view_effective_code(15,side_left?9:4,6)==0);
    }
    if(mode==1 || mode==2) {
        sm62_render_capture();assert(native_pwm==0); /* old epoch reply rejected */
        assert(!pthread_create(&thread,NULL,render,NULL));assert(!pthread_join(thread,NULL));
        assert(power_offs==1);
        sm62_render_capture();assert(native_pwm==0); /* forced-off reply rejected */
        assert(!pthread_create(&thread,NULL,render,NULL));assert(!pthread_join(thread,NULL));
        sm62_render_capture();assert(native_pwm==80); /* observed wake restores policy */
    }
    /* A QP call from the input core must be rejected before touching pixels. */
    unsigned before=panel_flushes;
    lv_disp_drv_t driver={0};lv_area_t area={0};lv_color_t pixel={0};
    qp_lvgl_flush(&driver,&area,&pixel);
    assert(panel_flushes==before);
    printf("PASS owner mode=%u native_fixture_turns=%u strips=%u\n",mode,native_turns,panel_flushes);
}
'''
prefix = '#include <pthread.h>\n#include <stdatomic.h>\n#include <sched.h>\n#include <time.h>\n#include "lvgl.h"\n'
service = strip((HERE.parent / "display_input.c").read_text())
generated = args.output / "owner.c"
generated.write_text(prefix+header+fixture+boundary+owner+service+flush+cases)
executable = args.output / "owner"
command = ["cc", "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
           "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
           "-fsanitize=undefined", "-fno-sanitize-recover=all", "-DLV_CONF_SKIP", "-DLV_MEM_CUSTOM=1",
           "-I"+str(HERE.parent), "-I"+str(ROOT / "lib/lvgl"), str(generated),
           str(HERE.parent / "render_exchange.c"),
           *sorted(str(p) for p in (ROOT / "lib/lvgl/src").rglob("*.c")), "-lm", "-o", str(executable)]
result = subprocess.run(command, capture_output=True, text=True)
(args.output / "build.log").write_text(result.stdout+result.stderr)
if result.returncode:
    raise RuntimeError(result.stdout+result.stderr)
for mode in range(6):
    result = subprocess.run([str(executable),str(mode)],capture_output=True,text=True,timeout=10)
    (args.output / f"mode{mode}.log").write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr,end="",flush=True)
    assert result.returncode==0,(mode,result.returncode)

# Final-frame completion before even the first pixel transfer must be rejected.
early = flush.replace('        bool started =', '        display_frame_completed();\n        bool started =', 1)
assert early != flush
generated.write_text(prefix+header+fixture+boundary+owner+service+early+cases)
subprocess.run(command, check=True, capture_output=True, text=True)
result = subprocess.run([str(executable), '0'], capture_output=True, text=True, timeout=10)
(args.output/'early-frame-negative.log').write_text(result.stdout+result.stderr)
assert result.returncode == -6, result.returncode
print('PASS negative control: early frame completion rejected')
generated.write_text(prefix+header+fixture+boundary+owner+service+flush+cases)
subprocess.run(command, check=True, capture_output=True, text=True)
