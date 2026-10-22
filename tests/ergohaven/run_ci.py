#!/usr/bin/env python3
"""One serial native CI wave; no firmware cross-toolchain required."""
from pathlib import Path
import sys
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def commands(output, lvgl):
    return [[sys.executable, *args] for args in [
        ['tests/ergohaven/run_native.py', str(output / 'native')],
        ['tests/ergohaven_app_combo.py'],
        ['tests/ergohaven_display/run.py', 'all'],
        ['tests/ergohaven/test_phenom_settings_sync.py'],
        ['tests/ergohaven/test_macropad_product_name.py'],
        ['tests/ergohaven/run_macropad_bugs.py', str(output / 'encoder'), '--only', '002'],
        ['tests/ergohaven/run_macropad_bugs.py', str(output / 'labels-default'), '--only', '004', '--no-app-layout', '--lvgl', str(lvgl)],
        ['tests/ergohaven/run_macropad_bugs.py', str(output / 'labels-app'), '--only', '004', '--lvgl', str(lvgl)],
        ['tests/ergohaven/run_macropad_wake.py', str(output / 'wake'), '--lvgl', str(lvgl)],
    ]]


def run(invocations, output):
    output.mkdir(parents=True, exist_ok=True)
    temporary = output / 'tmp'
    temporary.mkdir(exist_ok=True)
    env = dict(os.environ, TMPDIR=str(temporary), PYTHONDONTWRITEBYTECODE='1')
    failures = 0
    for index, command in enumerate(invocations, 1):
        print(f'[{index}/{len(invocations)}] {command}', flush=True)
        try:
            result = subprocess.run(command, cwd=ROOT, env=env)
            failed = result.returncode != 0
        except OSError as error:
            print(error, file=sys.stderr)
            failed = True
        failures += int(failed)
    print(f'Native CI: {len(invocations)} commands, {failures} failed', flush=True)
    return int(failures != 0)


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--lvgl', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error('output must be outside the source checkout')
    return run(commands(output, args.lvgl.resolve()), output)


if __name__ == '__main__':
    sys.exit(main())
