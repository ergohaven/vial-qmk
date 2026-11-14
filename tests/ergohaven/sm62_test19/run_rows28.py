#!/usr/bin/env python3
"""Current-production KEYMAP characterization; all generated artifacts external."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
INPUTS = HERE / 'inputs'
FUNCTIONS = [
    ('src/core/lv_refr.c', 'lv_refr_area_part'),
    ('src/core/lv_refr.c', 'lv_refr_obj'),
    ('src/draw/lv_draw_label.c', 'lv_draw_label'),
    ('src/draw/sw/lv_draw_sw_letter.c', 'lv_draw_sw_letter'),
    ('src/font/lv_font.c', 'lv_font_get_glyph_dsc'),
    ('src/font/lv_font.c', 'lv_font_get_glyph_bitmap'),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identity(repo):
    paths = set()
    for directory in ['keyboards/ergohaven', 'quantum', 'tmk_core/common', 'lib/lvgl']:
        paths.update(p for p in (repo / directory).rglob('*')
                     if p.is_file() and p.suffix in {'.c', '.h'})
    return {str(p.relative_to(repo)): sha(p) for p in sorted(paths)}


def run(command, output, name, env=None):
    (output / (name + '.command.json')).write_text(json.dumps(command, indent=2) + '\n')
    result = subprocess.run(command, capture_output=True, text=True, timeout=180, env=env)
    (output / (name + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{name}: exit {result.returncode}\n{result.stderr[-6000:]}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', required=True, type=Path,
                        help='explicit firmware repository root (no Git discovery)')
    parser.add_argument('--output', required=True, type=Path,
                        help='fresh absolute directory outside repository and portable tests')
    args = parser.parse_args()
    if not args.output.is_absolute():
        parser.error('--output must be absolute')
    repo = args.repo_root.resolve(strict=True)
    output = args.output.resolve()
    if output == repo or repo in output.parents or output == HERE or HERE in output.parents:
        parser.error('--output must be outside the repository and portable tests')
    if output.exists():
        parser.error('--output must not already exist')
    if not (repo / 'keyboards/ergohaven/sm62/config.h').is_file():
        parser.error('--repo-root is not an SM62 firmware tree')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before = identity(repo)
    # Preserve the exact 606-blob/five-overlay/approved-timer checks; only
    # manifest locations differ from qualification's external layout.
    scope = {'INPUTS': INPUTS}
    exec(compile((INPUTS / 'validate_lvgl.py').read_text(), 'validate_lvgl.py', 'exec'), scope)
    scope['validate_lvgl'](repo / 'lib/lvgl')
    config = (repo / 'keyboards/ergohaven/sm62/config.h').read_text()
    defines = {}
    for name in ['QP_LVGL_BUFFER_ROWS', 'QP_LVGL_SLICE_MS']:
        matches = re.findall(r'^#define\s+' + name + r'\s+(\d+)\s*$', config, re.M)
        assert len(matches) == 1, (name, matches)
        defines[name] = int(matches[0])
    assert defines == {'QP_LVGL_BUFFER_ROWS': 28, 'QP_LVGL_SLICE_MS': 4}, defines
    output.mkdir(parents=True)
    (output / 'production_geometry.h').write_text(''.join(
        f'#define {name} {value}\n' for name, value in defines.items()))
    source_scope = dict(F=repo, S=repo / 'keyboards/ergohaven/sm62', O=output,
                        INPUTS=INPUTS, re=re)
    exec(compile((INPUTS / 'generate_fixture.py').read_text(), 'generate_fixture.py', 'exec'), source_scope)
    # Each transformed TU contains the current full source body with only
    # include/pragma lines stripped. No embedded historic renderer is used.
    lvgl = output / 'lvgl'
    shutil.copytree(repo / 'lib/lvgl', lvgl, ignore=shutil.ignore_patterns('.git'))
    for rel in sorted({rel for rel, _ in FUNCTIONS}):
        text = (repo / 'lib/lvgl' / rel).read_text()
        text = text.replace('"../../../../quantum/sm62_trace.h"',
                            '"' + str(repo / 'quantum/sm62_trace.h') + '"')
        for index, (path, name) in enumerate(FUNCTIONS):
            if rel != path:
                continue
            matches = list(re.finditer(r'\b' + name + r'\s*\([^;{}]*\)\s*\{', text))
            assert len(matches) == 1, (name, len(matches))
            pos = matches[0].end()
            text = text[:pos] + f' probe_counts[{index}]++;' + text[pos:]
        (lvgl / rel).write_text('extern unsigned long probe_counts[6];\n' + text)
    qp = output / 'qp.c'
    text = qp.read_text()
    needle = 'void qp_lvgl_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {'
    assert text.count(needle) == 1
    qp.write_text('extern unsigned long probe_panel_flushes;\n' +
                  text.replace(needle, needle + ' probe_panel_flushes++;'))
    sm62 = repo / 'keyboards/ergohaven/sm62'
    flags = ['-std=gnu11', '-O1', '-g', '-pthread', '-Wall', '-Wextra', '-Werror',
             '-Wno-sign-compare', '-Wno-unused-function', '-Wno-unused-variable',
             '-Wno-unused-parameter', '-Wno-type-limits', '-Wno-format-security',
             '-fsanitize=undefined', '-fno-sanitize-recover=all', '-DLV_CONF_SKIP',
             '-DLV_MEM_CUSTOM=1', '-DLV_USE_FONT_COMPRESSED=1',
             '-DLV_USE_COOPERATIVE_REFR=1', '-DSM62_DUALCORE_RENDER=1',
             '-DSM62_RENDER_NATIVE_TEST=1', '-DSM62_FLASH_NATIVE_TEST=1']
    flags += ['-I' + str(p) for p in [output, lvgl, repo, repo / 'quantum',
              repo / 'quantum/sequencer', repo / 'quantum/keymap_extras',
              repo / 'tmk_core/common', repo / 'keyboards/ergohaven', sm62]]
    # Preserve the existing compressed-font shift sanitizer exception only.
    font = output / 'font.o'
    run(['gcc', *flags, '-fno-sanitize=shift', '-c',
         str(lvgl / 'src/font/lv_font_fmt_txt.c'), '-o', str(font)], output, 'font-build')
    names = ['render_owner', 'dualcore_trace', 'clock', 'player', 'keymap', 'orbit',
             'display', 'policy', 'formatter', 'stars', 'qp', 'deferred', 'main']
    fonts = sorted((repo / 'keyboards/ergohaven/macropad').glob('sm62_font_*.c'))
    fonts += [repo / f'keyboards/ergohaven/src/display/fonts/eh_font_iosevka_{n}.c'
              for n in [18, 28]]
    extras = [sm62 / 'player_fonts.c', *sorted(sm62.glob('sm62_font_cjk_24*.c')),
              repo / 'keyboards/ergohaven/sm30/anim.c',
              repo / 'keyboards/ergohaven/macropad/sm62_layer_04.c',
              sm62 / 'render_exchange.c', sm62 / 'flash_guard.c', sm62 / 'flash_gate.c']
    sources = [output / (name + '.c') for name in names] + fonts + extras
    sources += sorted(p for p in (lvgl / 'src').rglob('*.c') if p.name != 'lv_font_fmt_txt.c')
    run(['gcc', *flags, *map(str, sources), str(font), '-lm', '-o', str(output / 'test')],
        output, 'build')
    expected = json.loads((INPUTS / 'expected_rows28.json').read_text())
    measurements = []
    panels = []
    for left in [1, 0]:
        stdout = run([str(output / 'test'), str(left), '1'], output, f'run-{left}-1',
                     dict(os.environ, SM62_DIAG_OUT=str(output)))
        print(stdout, end='')
        assert f'PASS rows=28 side={left} role=1' in stdout
        for line in stdout.splitlines():
            if line.startswith('MEASURE '):
                measurements.append(dict(rows=28, left=left, master=1, **json.loads(line[8:])))
            elif line.startswith('PANEL '):
                measurements[-1]['panel_flush_callbacks'] = int(line[6:])
        for label in ['full-keymap', 'press-0-0', 'press-0-1-focus']:
            panel = output / f'{label}-{left}-1.bin'
            assert panel.stat().st_size == 240 * 280 * 2
            panels.append(dict(left=left, label=label, sha256=sha(panel)))
        initial = (output / f'full-keymap-{left}-1.bin').read_bytes()
        for label in ['press-0-0', 'press-0-1-focus']:
            assert (output / f'{label}-{left}-1.bin').read_bytes() != initial
    assert len(measurements) == 6 and measurements == expected['measurements'], measurements
    assert panels == expected['panels'], panels
    after = identity(repo)
    assert before == after, 'production C/header source changed during run'
    result = dict(repo_root=str(repo), geometry=defines, source_sha256=before,
                  measurements=measurements, panels=panels,
                  production_source_unchanged=True, physical_causality_proven=False)
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS current production rows28: 6 exact operation-count cases, 6 panel SHA256 references; production C/header identity unchanged')


if __name__ == '__main__':
    main()
