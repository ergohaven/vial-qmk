#!/usr/bin/env python3
"""Replay physical edges during real LVGL flushes and QMK scan/debounce.

SPI costs, matrix reads, clock, and report sink are synthetic boundaries.
This is a scheduling regression, not a measurement of hardware latency.
"""
import json
import resource
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
# Share the existing source-backed scan/debounce/dispatch/LVGL fixture.
namespace = {"__file__": str(HERE / "run_scan_timing.py")}
prefix = (HERE / "run_scan_timing.py").read_text().split("c=a.output/")[0]
exec(compile(prefix, str(HERE / "run_scan_timing.py"), "exec"), namespace)
args, root, text = (namespace[key] for key in ["a", "ROOT", "text"])
clang_warning_flags = namespace["clang_warning_flags"]
text = text[:text.index("int main(int argc,char **argv)")]


def replace_once(old, new):
    global text
    assert text.count(old) == 1, old
    text = text.replace(old, new)


replace_once("static bool fired, physical_down;", """static bool fired, physical_down;
static unsigned scenario, keycode, strip_cost, scan_row = 1;
""")
replace_once("physical_down=now>=100 && now<release_at;", """physical_down = scenario == 0 ? (now >= 100 && now < 125) :
        ((now >= 80 && now < 110) || (now >= 135 && now < 180));""")
# Exercise either logical half through QMK's merged matrix. UART is not modeled.
replace_once("bool changed=raw_rows[1]!=value;", "bool changed=raw_rows[scan_row]!=value;")
replace_once("if(raw_rows[1] && !value)", "if(raw_rows[scan_row] && !value)")
replace_once("raw_rows[1]=value;debounce(raw_rows,cooked_rows,MATRIX_ROWS/2,changed);",
             "raw_rows[scan_row]=value;debounce(raw_rows,cooked_rows,MATRIX_ROWS,changed);")
replace_once("static void qp_pixdata(void *d,void *p,uint32_t n){(void)d;(void)p;(void)n;if(cost_due){cost_due=false;printf(\"modeled_SPI begin=%u cost=%u\\n\",now,block_cost);now+=block_cost;}}", """static void qp_pixdata(void *d,void *p,uint32_t n) {
    (void)d; (void)p; (void)n;
    if (cost_due) now += strip_cost;
}""")
text += r'''
int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    assert(argc == 5);
    scenario = atoi(argv[1]); keycode = atoi(argv[2]);
    strip_cost = atoi(argv[3]); scan_row = atoi(argv[4]);
    now = 90; side_left = scan_row < 5; combo_disable(); lv_init();
    static lv_disp_draw_buf_t buffer;
    static lv_color_t pixels[240 * 28];
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 240 * 28);
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 240; driver.ver_res = 280;
    driver.draw_buf = &buffer; driver.flush_cb = qp_lvgl_flush;
    lv_disp_drv_register(&driver);
    lv_refr_now(NULL); flushes = 0;
    defaults[0][scan_row][2] = keycode;
    layer_state = default_layer_state = 1;
    debounce_init(MATRIX_ROWS);
    protocol_keyboard_task();
    cost_due = true;
    lv_obj_invalidate(lv_scr_act()); lv_refr_now(NULL);
    cost_due = false;
    for (; now < 250; now++) protocol_keyboard_task();
    unsigned expected = scenario == 0 ? 1 : 2;
    printf("scenario=%u keycode=%u strip_ms=%u row=%u downs=%u ups=%u max_scan_gap=%u flushes=%u\n",
           scenario, keycode, strip_cost, scan_row, adds[keycode], dels[keycode], max_scan_gap, flushes);
    assert(flushes == 10);
    assert(adds[keycode] == expected && dels[keycode] == expected && !hid_keys[keycode]);
    assert(max_scan_gap <= strip_cost);
    debounce_free();
}
'''
generated = args.output / "render_input.c"
generated.write_text(text)
fonts = [root / "keyboards/ergohaven/src/display/fonts" / f"eh_font_iosevka_{n}.c" for n in [18, 28]]
fonts += [root / "keyboards/ergohaven/macropad" / f"sm62_font_zen_medium_{n}.c" for n in [16, 17, 20, 28]]
includes = [root / p for p in ["lib/lvgl", "quantum", "quantum/sequencer", "quantum/keymap_extras", "tmk_core/common", "keyboards/ergohaven"]]
executable = args.output / "render_input"
command = ["gcc", "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
           "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-type-limits",
           *clang_warning_flags("gcc", lvgl=True),
           "-fsanitize=undefined", "-fno-sanitize-recover=all", "-DLV_CONF_SKIP", "-DLV_USE_ASSERT_MALLOC=1",
           *["-I" + str(p) for p in includes], str(generated), *map(str, fonts),
           *sorted(str(p) for p in (root / "lib/lvgl/src").rglob("*.c")), "-lm", "-o", str(executable)]
subprocess.run(command, check=True)
results = []
for scenario in [0, 1]:
    for keycode in [34, 42, 40]:  # KC_5, KC_BSPC, KC_ENT
        for row in [1, 6]:
            # Every full-frame stall spans a complete pulse or release gap.
            for strip_cost in [6, 8, 12]:
                result = subprocess.run([str(executable), *map(str, [scenario, keycode, strip_cost, row])],
                                        capture_output=True, text=True)
                print(result.stdout + result.stderr, end="", flush=True)
                results.append({"scenario": scenario, "keycode": keycode, "row": row,
                                "strip_ms": strip_cost, "exit": result.returncode})
(args.output / "results.json").write_text(json.dumps(results, indent=2))
assert all(result["exit"] == 0 for result in results), results
print(f"PASS {len(results)} render/input scenarios; synthetic timing, no hardware validation")
