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
static bool is_keyboard_master(void) { return true; }
static bool is_keyboard_left(void) { return true; }
static void raw_hid_send(uint8_t *p, uint8_t n) { assert(n == 32); memcpy(reply,p,n); }
void sm62_dc_trace_scan(void);
'''
cases = r'''
static uint32_t word(unsigned i) {
    uint8_t *p = reply + 8 + 4 * i;
    return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
int main(int argc, char **argv) {
    assert(argc == 4);
    unsigned delay = atoi(argv[1]), bypass = atoi(argv[2]), wrap = atoi(argv[3]);
    /* First sample is exactly zero; elapsed subtraction crosses timer wrap. */
    uint32_t samples[] = {0, 1000, 2000, 3000+delay};
    if (wrap) { samples[0]=UINT32_MAX-1000U; samples[1]=499; samples[2]=1499; samples[3]=2499+delay; }
    for (unsigned i=0; i<4; i++) {
        clock_us=samples[i];
        if (!bypass) sm62_dc_trace_scan();
    }
    uint8_t request[32]={0xD7,0x62,SM62_DC_TRACE_VERSION,1,0,1};
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
}
'''
generated = args.output / "trace.c"
# The baseline has no scan hook. Keep that absence executable as a no-op;
# it must fail the enabled/count assertions rather than produce a zero-gap pass.
if "void sm62_dc_trace_scan(" not in source:
    source += "\nvoid sm62_dc_trace_scan(void) {}\n"
generated.write_text(fixture + source + cases)
results = []
for enabled in [False, True]:
    executable = args.output / ("trace-on" if enabled else "trace-off")
    command = ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
               "-fsanitize=undefined", "-fno-sanitize-recover=all", "-DSM62_RENDER_NATIVE_TEST",
               *(["-DSM62_SCAN_TRACE"] if enabled else []),
               "-I"+str(HERE.parent), str(generated), "-o", str(executable)]
    subprocess.run(command, check=True)
    for name, delay, bypass, wrap in [("idle", 0, 0, 0), ("cpu200", 200000, 0, 0),
                                      ("transfer200", 200000, 0, 0), ("wrap", 0, 0, 1),
                                      ("bypassed", 0, 1, 0)]:
        result = subprocess.run([str(executable), str(delay), str(bypass), str(wrap)], capture_output=True, text=True)
        print(enabled, name, result.returncode, result.stdout, result.stderr, flush=True)
        results.append({"enabled": enabled, "case": name, "exit": result.returncode})
(args.output / "results.json").write_text(json.dumps(results, indent=2)+"\n")
assert all(result["exit"] == 0 for result in results), results
print("PASS scan telemetry, wrap and bypass controls; no hardware qualification")
