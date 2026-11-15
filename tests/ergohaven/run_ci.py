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
        ['tests/ergohaven/test_split_state_sync.py'],
        ['tests/ergohaven/test_rgblight_sync.py'],
        ['tests/ergohaven/test_macropad_product_name.py'],
        ['tests/ergohaven/run_macropad_bugs.py', str(output / 'encoder'), '--only', '002'],
        ['tests/ergohaven/run_macropad_bugs.py', str(output / 'labels-default'), '--only', '004', '--no-app-layout', '--lvgl', str(lvgl)],
        ['tests/ergohaven/run_macropad_bugs.py', str(output / 'labels-app'), '--only', '004', '--lvgl', str(lvgl)],
        ['tests/ergohaven/run_macropad_wake.py', str(output / 'wake'), '--lvgl', str(lvgl)],
        # Production SINGLE/core0 SM62 uses the pinned base LVGL, not the
        # historical cooperative/core1 overlay. Keep the legacy wave above.
        ['tests/ergohaven/run_sm62_layout.py', str(output / 'sm62-layout')],
        ['tests/ergohaven/run_sm62_split.py', str(output / 'sm62-split')],
        ['tests/ergohaven/run_sm62_cached_split.py', str(output / 'sm62-cached')],
        ['tests/ergohaven/run_sm62_delta_split.py', str(output / 'sm62-delta')],
        ['tests/ergohaven/run_sm62_rle_split.py', str(output / 'sm62-rle')],
        ['tests/ergohaven/run_sm62_defaults.py', str(output / 'sm62-defaults')],
        ['tests/ergohaven/run_sm62_layout_reserve.py', str(output / 'sm62-reserve')],
        ['tests/ergohaven/run_sm62_peer_backlight.py', str(output / 'sm62-backlight')],
        ['tests/ergohaven/run_sm62_peer_power.py', str(output / 'sm62-power')],
        ['tests/ergohaven/run_sm62_peer_recovery.py', str(output / 'sm62-rejoin')],
        ['tests/ergohaven/run_sm62_rpc_recovery.py', str(output / 'sm62-rpc')],
        ['tests/ergohaven/run_sm62_startup.py', str(output / 'sm62-startup')],
        ['tests/ergohaven/run_sm62_raw_fifo.py', str(output / 'sm62-fifo')],
        ['tests/ergohaven/run_sm62_storage.py', str(output / 'sm62-storage')],
        ['tests/ergohaven/run_sm62_post_init_preservation.py', str(output / 'sm62-post-init')],
        ['tests/ergohaven/run_sm62_screen.py', str(output / 'sm62-screen'), '--lvgl', str(lvgl)],
        ['tests/ergohaven/run_sm62_player_cjk.py', str(output / 'sm62-player')],
        ['tests/ergohaven/run_resident100.py', str(output / 'resident100')],
        ['tests/ergohaven/sm62_interop/fixtures.py', '--output', str(output / 'fixtures')],
        ['tests/ergohaven/test_compact_usb_compat.py', str(output / 'fixtures/full00.bin'), str(output / 'compact-usb')],
        ['tests/ergohaven/run_resident100_peer.py', str(output / 'resident-peer')],
        ['tests/ergohaven/test_resident100_restart.py', str(output / 'resident-peer/resident100_peer'), str(output / 'resident100/fixtures'), str(output / 'resident-restart')],
        ['tests/ergohaven/sm62_macro_storage/test_macro_storage.py', '--source-root', str(ROOT), '--output-dir', str(output / 'macro-storage')],
        ['tests/ergohaven/sm62_locked_boot/run.py', '--output-dir', str(output / 'locked-boot')],
        ['tests/ergohaven/sm62_locked_boot/run_via_optional.py', '--source-root', str(ROOT), '--output-dir', str(output / 'via-optional')],
        ['tests/ergohaven/sm62_settings_deadline/run.py', '--phase', 'ci'],
        ['tests/ergohaven/sm62_usb_identity/run.py', '--firmware-only', '--phase', 'ci'],
        ['tests/ergohaven/sm62_interop/build.py', '--source', str(ROOT), '--output', str(output / 'interop')],
        ['tests/ergohaven/sm62_interop/selfcheck.py', '--exe', str(output / 'interop/firmware-bridge'), '--output', str(output / 'interop/selfcheck')],
        ['-m', 'unittest', 'discover', '-s', 'tests/ergohaven/sm62_interop', '-p', 'test_*.py', '-v'],
        ['-m', 'unittest', 'discover', '-s', 'tests/ergohaven', '-p', 'test_sm62_production_board.py', '-v'],
        ['-m', 'unittest', 'discover', '-s', 'tests/ergohaven', '-p', 'test_lvgl_validation_profiles.py', '-v'],
        ['keyboards/ergohaven/sm62/tests/run_pipeline.py', '--output', str(output / 'sm62-pipeline')],
        ['keyboards/ergohaven/sm62/tests/run_repeat_display.py', '--output', str(output / 'sm62-repeat')],
        ['keyboards/ergohaven/sm62/tests/run_scan_timing.py', '--output', str(output / 'sm62-scan')],
        ['keyboards/ergohaven/sm62/tests/run_render_input.py', '--output', str(output / 'sm62-render-input')],
        ['keyboards/ergohaven/sm62/tests/run_display_guards.py', '--output', str(output / 'sm62-display-guards')],
        ['keyboards/ergohaven/sm62/tests/run_render_isolation.py', '--output', str(output / 'sm62-isolation')],
        ['keyboards/ergohaven/sm62/tests/run_render_owner.py', '--output', str(output / 'sm62-owner')],
        ['keyboards/ergohaven/sm62/tests/run_render_lifecycle.py', '--output', str(output / 'sm62-lifecycle')],
        ['keyboards/ergohaven/sm62/tests/run_peer_capture.py', '--out', str(output / 'sm62-capture')],
    ]]


def run(invocations, output):
    output.mkdir(parents=True, exist_ok=True)
    temporary = output / 'tmp'
    temporary.mkdir(exist_ok=True)
    env = dict(os.environ, TMPDIR=str(temporary), AGENT_BUILD_CACHE=str(output), PYTHONDONTWRITEBYTECODE='1',
               SM62_FIXTURE_DIR=str(output / 'resident100/fixtures'))
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
    if output == ROOT or ROOT in output.parents or output in ROOT.parents:
        parser.error('output must be outside the source checkout and its ancestors')
    if os.path.lexists(args.output):
        parser.error('output must be fresh for the complete CI wave')
    return run(commands(output, args.lvgl.resolve()), output)


if __name__ == '__main__':
    sys.exit(main())
