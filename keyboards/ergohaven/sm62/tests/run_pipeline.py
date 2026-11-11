#!/usr/bin/env python3
"""Offline source-backed QMK processor regression; outputs only in --output.
Hardware/report/modifier/time boundaries are synthetic. The actual full Combo,
Key Override and Tap Dance processors and quantum hook ordering are compiled.
--ref compiles baseline production with current assertions (genuine RED).
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--firmware', type=Path, default=Path(__file__).resolve().parents[4])
p.add_argument('--output', type=Path, required=True)
p.add_argument('--ref')
a = p.parse_args()
a.firmware = a.firmware.resolve()
a.output.mkdir(parents=True, exist_ok=True)
def source(path):
    if a.ref:
        return subprocess.check_output(['git', 'show', f'{a.ref}:{path}'], cwd=a.firmware, text=True)
    return (a.firmware / path).read_text()
def body(text, signature):
    start = text.index(signature)
    i = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[i] == '{') - (text[i] == '}')
        i += 1
    return text[start:i] + '\n'
def stripped(text):
    return '\n'.join(l for l in text.splitlines() if not re.match(r'\s*#\s*(include|pragma once)', l)) + '\n'
main = source('keyboards/ergohaven/ergohaven_main.c')
hid = source('keyboards/ergohaven/hid.c')
quantum = source('quantum/quantum.c')
production = hid[hid.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):hid.index('\nhid_data_t *get_hid_data')].rsplit('#endif', 1)[0]
header = (Path(__file__).parent / 'pipeline_boundary.h').read_text()
processors = ''
for name in ['key_override', 'tap_dance', 'combo']:
    processors += stripped(source(f'quantum/process_keycode/process_{name}.h'))
processors += (Path(__file__).parent / 'pipeline_fixture.inc').read_text()
for name in ['key_override', 'tap_dance', 'combo']:
    processors += stripped(source(f'quantum/process_keycode/process_{name}.c'))
pre = body(main, 'bool pre_process_record_kb(')
# Retain actual dispatch boundary; unrelated Ergohaven custom-key switch omitted.
kb = main[main.index('bool process_record_kb('):main.index('    // Some Vial keymaps')] + 'return process_record_user(keycode, record);\n}\n'
chain_start = quantum.index('    if (!(\n', quantum.index('bool process_record_quantum_helper('))
chain_end = quantum.index('    if (record->event.pressed) {', chain_start)
chain = 'static bool selected_quantum_chain(uint16_t keycode, keyrecord_t *record) {\n' + quantum[chain_start:chain_end] + 'return true;\n}\n'
text = header + production + processors + pre + kb + body(quantum, 'uint16_t get_record_keycode(') + body(quantum, 'bool pre_process_record_quantum(') + chain + (Path(__file__).parent / 'pipeline_cases.inc').read_text()
c = a.output / 'pipeline.c'
c.write_text(text)
cmd = ['gcc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-type-limits', '-I'+str(a.firmware/'quantum'), '-I'+str(a.firmware/'quantum/sequencer'), '-I'+str(a.firmware/'quantum/keymap_extras'), '-I'+str(a.firmware/'tmk_core/common'), '-I'+str(a.firmware/'keyboards/ergohaven'), str(c), '-o', str(a.output/'pipeline')]
r = subprocess.run(cmd, capture_output=True, text=True)
(a.output/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:
    print(r.stdout+r.stderr)
    raise SystemExit(r.returncode)
failed = 0
for case in ['override', 'td', 'combo_replay', 'combo_match', 'held_transition', 'fallback_transition', 'no_key']:
    r = subprocess.run([str(a.output/'pipeline'), case], capture_output=True, text=True)
    log = r.stdout+r.stderr
    (a.output/(case+'.log')).write_text(log)
    print(f'{case}: exit={r.returncode}\n{log}', end='', flush=True)
    failed += r.returncode != 0
print(f'RESULT {7-failed}/7 passed')
raise SystemExit(bool(failed))
