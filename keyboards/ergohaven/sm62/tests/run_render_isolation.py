#!/usr/bin/env python3
"""Exercise production scan telemetry; clocks and raw HID transport are fixtures.

Injected delays characterize the scheduling boundary, not hardware latency.
The full input/LVGL starvation controls remain in run_scan_timing.py.
"""
import argparse
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
source = (HERE.parent / "dualcore_trace.c").read_text()
source = "\n".join(line for line in source.splitlines() if not line.startswith('#include "'))
def body(text, signature):
    start=text.index(signature); end=text.index('{',start)+1; depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end]+'\n'
board=(HERE.parent/'sm62.c').read_text()
rpc=body(board,'static void sm62_diag_rpc(')+body(board,'bool sm62_dc_peer_scan(')
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dualcore_trace.h"
static uint32_t clock_us;
static uint8_t reply[32];
uint32_t sm62_dc_test_us(void) { return clock_us; }
static uint8_t sm62_render_screen(void) { return 0; }
static uint32_t sm62_render_frames(void) { return 0; }
static bool master=true, left=true;
static bool is_keyboard_master(void) { return master; }
static bool is_keyboard_left(void) { return left; }
#define RPC_SM62_DIAG 1
static bool transaction_rpc_exec(int,uint8_t,const void *,uint8_t,void *);
/* Legacy branch stays compiled but is intentionally unreachable on isolation.
 * Fail if dispatch ever falls through and presents old metrics as qualified. */
static bool diag_started;
static uint32_t diag_absolute[6];
static bool sm62_fps_rpc(uint8_t a,const void *b,uint8_t c,void *d) { (void)a;(void)b;(void)c;(void)d;abort(); }
static uint8_t sm62_side_id(void) { abort(); }
static void diag_put32(uint8_t *p,uint32_t v) { (void)p;(void)v;abort(); }
static void diag_put16(uint8_t *p,uint16_t v) { (void)p;(void)v;abort(); }
static uint16_t diag_clip(uint32_t v) { (void)v;abort(); }
static uint32_t timer_read32(void) { abort(); }
static void raw_hid_send(uint8_t *p, uint8_t n) { assert(n == 32); memcpy(reply,p,n); }
void sm62_dc_trace_scan(void);
'''
cases = r'''
static bool peer_transport_ok=true, peer_wrong_role, peer_same_side;
static bool transaction_rpc_exec(int id,uint8_t in_len,const void *in,uint8_t out_len,void *out) {
    assert(id==RPC_SM62_DIAG && in_len==1 && out_len==32);
    if (!peer_transport_ok) return false;
    master=peer_wrong_role;left=peer_same_side;
    sm62_diag_rpc(in_len,in,out_len,out);
    master=true;left=true;return true;
}
static uint32_t word(unsigned i) {
    uint8_t *p = reply + 8 + 4 * i;
    return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
int main(int argc, char **argv) {
    assert(argc == 4);
    unsigned delay = atoi(argv[1]), bypass = atoi(argv[2]), wrap = atoi(argv[3]);
    uint8_t request[32]={0xD7,0x62,SM62_DC_TRACE_VERSION,3,0,1};
    /* Boot/previous trial gaps must not poison a new measurement window. */
    clock_us=0;sm62_dc_trace_scan();clock_us=400000;sm62_dc_trace_scan();
    assert(sm62_dc_diag_raw(request,32) && reply[7]==1);
    /* First sample is exactly zero; elapsed subtraction crosses timer wrap. */
    uint32_t samples[] = {0, 1000, 2000, 3000+delay};
    if (wrap) { samples[0]=UINT32_MAX-1000U; samples[1]=499; samples[2]=1499; samples[3]=2499+delay; }
    for (unsigned i=0; i<4; i++) {
        clock_us=samples[i];
        if (!bypass) sm62_dc_trace_scan();
    }
    request[3]=1;
    assert(sm62_dc_diag_raw(request,32));
    sm62_dc_trace_poll(); sm62_dc_trace_abort();
    request[3]=2; request[6]=3;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==2);
    printf("count=%u max_gap_us=%u last_us=%u enabled=%u\n",word(2),word(3),word(4),word(5));
#ifdef SM62_SCAN_TRACE
    assert(word(5)==1 && word(2)==(bypass?0:4));
    if (!bypass) {
        assert(word(3)==(delay ? 1000+delay : wrap ? 1500 : 1000));
        assert(word(4)==samples[3]);
    }
#else
    assert(word(5)==0 && word(2)==0 && word(3)==0 && word(4)==0);
#endif
    /* Reading a completed frame does not stop the independent scan window. */
    clock_us=samples[3]+1000U;
    if (!bypass) sm62_dc_trace_scan();
    request[3]=4;request[6]=0;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==2);
#ifdef SM62_SCAN_TRACE
    assert(word(0)==(bypass?0:5) && word(3)==1);
    if(!bypass) assert(word(1)==(delay ? 1000+delay : wrap ? 1500 : 1000));
#else
    assert(!word(0) && !word(1) && !word(3));
#endif
    /* The serial callback queues a peer reset without modifying scan state.
     * A read stays pending until the next core0 scan consumes it. */
    request[3]=6;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==
#ifdef SM62_SCAN_TRACE
        8
#else
        0xFE
#endif
    ); /* no valid peer trial before reset */
    request[3]=5;
    assert(sm62_dc_diag_raw(request,32));
#ifdef SM62_SCAN_TRACE
    assert(reply[7]==1 && reply[6]==0x22 && word(0)==1);
    assert(sm62_dc_diag_raw(request,32) && reply[7]==6); /* no pending overwrite */
    request[3]=6;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==3);
    clock_us+=1000;sm62_dc_trace_scan();clock_us+=1250;sm62_dc_trace_scan();
    assert(sm62_dc_diag_raw(request,32) && reply[7]==2);
    assert(word(0)==2 && word(1)==1250 && word(3)==1 && word(5)==1);
    STORE(&dc_scan_version,LOAD(&dc_scan_version)+1U); /* preempted writer */
    assert(sm62_dc_diag_raw(request,32) && reply[7]==3);
    request[3]=1;request[5]=2;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==1);
    sm62_dc_trace_poll();sm62_dc_trace_abort();
    request[3]=2;request[6]=3;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==2 && !word(5));
    assert(dc_words[7]&32U); /* torn frame sample is explicitly invalid */
    STORE(&dc_scan_version,LOAD(&dc_scan_version)+1U);
    request[3]=6;request[6]=0;
    STORE(&dc_scan_reset_request,0U);STORE(&dc_scan_reset_ack,0U); /* peer reboot */
    assert(sm62_dc_diag_raw(request,32) && reply[7]==8);
#else
    assert(reply[7]==0xFE);
#endif
    peer_wrong_role=true;request[3]=6;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==7);
    peer_wrong_role=false;peer_same_side=true;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==7);
    peer_same_side=false;
    uint8_t old_reply[32];sm62_diag_rpc(0,NULL,32,old_reply);
    assert(old_reply[2]==SM62_DC_TRACE_VERSION && old_reply[7]==0xFE);
    peer_transport_ok=false;request[3]=6;
    assert(sm62_dc_diag_raw(request,32) && reply[7]==7);
}
'''
generated = args.output / "trace.c"
# The baseline has no scan hook. Keep that absence executable as a no-op;
# it must fail the enabled/count assertions rather than produce a zero-gap pass.
if "void sm62_dc_trace_scan(" not in source:
    source += "\nvoid sm62_dc_trace_scan(void) {}\n"
generated.write_text(fixture + source + rpc + cases)
results = []
for enabled in [False, True]:
    executable = args.output / ("trace-on" if enabled else "trace-off")
    command = ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
               "-fsanitize=undefined", "-fno-sanitize-recover=all", "-DSM62_RENDER_NATIVE_TEST", "-DSM62_DUALCORE_RENDER",
               *(["-DSM62_SCAN_TRACE"] if enabled else []),
               "-I"+str(HERE.parent), str(generated), "-o", str(executable)]
    subprocess.run(command, check=True)
    for name, delay, bypass, wrap in [("idle", 0, 0, 0), ("injected-gap200", 200000, 0, 0),
                                      ("wrap", 0, 0, 1),
                                      ("bypassed", 0, 1, 0)]:
        result = subprocess.run([str(executable), str(delay), str(bypass), str(wrap)], capture_output=True, text=True)
        print(enabled, name, result.returncode, result.stdout, result.stderr, flush=True)
        results.append({"enabled": enabled, "case": name, "exit": result.returncode})
(args.output / "results.json").write_text(json.dumps(results, indent=2)+"\n")
assert all(result["exit"] == 0 for result in results), results
print("PASS scan telemetry, wrap and bypass controls; no hardware qualification")
