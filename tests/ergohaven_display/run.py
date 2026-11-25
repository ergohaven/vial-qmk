#!/usr/bin/env python3
"""Compile the shared macropad.c (only hardware includes replaced), then run regressions.
Usage: python3 tests/ergohaven_display/run.py [custom|migration|all] [--source-ref HEAD]
No QMK toolchain or third-party Python packages required.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode', nargs='?', choices=('custom', 'migration', 'all'), default='all')
parser.add_argument('--source-ref', help='Compile production source from a git revision, e.g. HEAD')
args = parser.parse_args()
source = ROOT / 'keyboards/ergohaven/macropad/macropad.c'
if args.source_ref:
    # Older immutable sources predate the shared owner; keep historical controls usable.
    owners = [source.relative_to(ROOT), Path('keyboards/ergohaven/macropad/rev3/rev3.c')]
    owner = next(path for path in owners if subprocess.run(
        ['git', 'cat-file', '-e', args.source_ref + ':' + str(path)], cwd=str(ROOT),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0)
    source_text = subprocess.check_output(['git', 'show', args.source_ref + ':' + str(owner)], cwd=str(ROOT)).decode()
else:
    source_text = source.read_text()
results = []
with tempfile.TemporaryDirectory(prefix='ergohaven-display-') as tmp:
    tmp = Path(tmp)
    # Compile every function from production, not a reimplementation of settings.
    body = re.sub(r'^#include[^\n]*\n', '', source_text, flags=re.MULTILINE)
    (tmp / 'production.c').write_text(body)
    for flash in (False, True):
        exe = tmp / ('flash' if flash else 'eeprom')
        command = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(tmp)]
        if flash:
            command.append('-DEH_DISPLAY_SETTINGS_FLASH')
        subprocess.run(command + [str(HERE / 'regression.c'), '-o', str(exe)], check=True)
        print('storage:', exe.name, flush=True)
        results.append(subprocess.run([str(exe), args.mode]).returncode)
sys.exit(1 if any(results) else 0)
