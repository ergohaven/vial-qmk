#!/usr/bin/env python3
"""Source-backed Phenom rotation regression; run with python3.

Extracts both production rotations verbatim for all three variants. Only the
report type and orientation enum are host substitutes; this does not test
sensors, split transport, USB or hardware. Requires a C11 compiler (CC or cc).
The int16 minimum retains existing narrowing/negation behavior, not saturation.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VARIANTS = ("phenom", "phenom_mini", "phenom_micro")


def function(source, name):
    match = re.search(r"^static report_mouse_t " + name + r"\([^\n]*\) \{", source, re.M)
    if match is None:
        raise ValueError(f"Missing production function: {name}")
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PREAMBLE = r'''
#include <stdint.h>
#include <stdio.h>
typedef int16_t mouse_xy_report_t;
typedef enum {ROT_0, ROT_90, ROT_180, ROT_270} orientation_t;
typedef struct {mouse_xy_report_t x,y; int8_t h,v; uint8_t buttons;} report_mouse_t;
'''
HARNESS = r'''
int main(void) {
    const int values[] = {-32768,-32767,-2048,-2047,-300,-256,-255,-129,-128,-127,-1,0,1,127,128,129,255,256,300,2047,2048,32767};
    unsigned count = 0;
    for (unsigned a=0; a<sizeof(values)/sizeof(values[0]); a++)
    for (unsigned b=0; b<sizeof(values)/sizeof(values[0]); b++)
    for (unsigned r=0; r<4; r++)
    for (unsigned ball=0; ball<2; ball++) {
        int32_t x=values[a], y=values[b], ex=x, ey=y;
        if (r==1) {ex=-y; ey=x;}
        if (r==2) {ex=-x; ey=-y;}
        if (r==3) {ex=y; ey=-x;}
        /* Preserve production narrowing at every assignment, including -32768. */
        mouse_xy_report_t expected_x=ex, expected_y=ey;
        if (ball) expected_y=-expected_y;
        report_mouse_t input={.x=x,.y=y,.h=3,.v=-4,.buttons=5};
        report_mouse_t out=ball ? phenom_rotate_trackball_report(input,r) : phenom_rotate_report(input,r);
        if (out.x!=expected_x || out.y!=expected_y || out.h!=3 || out.v!=-4 || out.buttons!=5) {
            fprintf(stderr,"FAIL input=%ld,%ld rot=%u ball=%u got=%d,%d expected=%d,%d\n",
                    (long)x,(long)y,r,ball,out.x,out.y,expected_x,expected_y);
            return 1;
        }
        count++;
    }
    printf("PASS: %u cases including int16 negation boundary; buttons/wheels preserved\n", count);
    return 0;
}
'''


class RotationTests(unittest.TestCase):
    def test_full_width_rotations(self):
        for variant in VARIANTS:
            with self.subTest(variant=variant):
                source = (ROOT / "keyboards/ergohaven" / variant / "rev1/pointing.c").read_text()
                rotations = "\n".join(function(source, name) for name in
                                      ("phenom_rotate_report", "phenom_rotate_trackball_report"))
                with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR"), prefix="phenom-rotation-") as tmp:
                    path = Path(tmp)
                    (path / "test.c").write_text(PREAMBLE + rotations + HARNESS)
                    env = dict(os.environ)
                    env.pop("C_INCLUDE_PATH", None)
                    subprocess.run(shlex.split(env.get("CC", "cc")) + [
                        "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                        "-fno-sanitize-recover=all", str(path / "test.c"), "-o", str(path / "test")],
                        check=True, env=env)
                    result = subprocess.run([str(path / "test")], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    print(variant + ": " + result.stdout.strip())


if __name__ == "__main__":
    unittest.main()
