"""Construction contracts, NOT native execution. No compiler is invoked.

Capture complete generated translation units at the compiler boundary. These
checks supplement (never replace) the 47-command native wave and its behavioral
negative controls. Set NATIVE_FIXTURE_EVIDENCE to retain the exact generated C.
"""

import ast
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import runpy
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / 'keyboards/ergohaven/sm62/tests'
sys.path.insert(0, str(Path(__file__).parent))
import macropad_native_support
from native_vial_firewall import firewall_source


class CompilerBoundary(Exception):
    pass


def generated(script, arguments, filename):
    """Run the actual entire generator up to, but never through, GCC."""
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory)
        inputs = {}
        compiler = []
        read_text = Path.read_text
        def record(path,*args,**kwargs):
            text = read_text(path,*args,**kwargs)
            if ROOT in path.resolve().parents:
                inputs[str(path.resolve().relative_to(ROOT))] = hashlib.sha256(text.encode()).hexdigest()
            return text
        def stop(command, *args, **kwargs):
            if command[0] == sys.executable:
                with patch.object(sys, 'argv', command[1:]), patch.dict(os.environ, kwargs.get('env', {})):
                    runpy.run_path(command[1], run_name='__main__')
            assert command[0] == 'gcc', command
            compiler.extend(command)
            raise CompilerBoundary
        argv = [str(script), *[str(output) if x == '{output}' else x for x in arguments]]
        with patch.object(sys, 'argv', argv), patch.dict(os.environ, SM62_FIXTURE_DIR=str(output/'fixtures')), patch.object(Path, 'read_text', record), patch.object(subprocess, 'run', side_effect=stop), patch.object(macropad_native_support, 'validate_lvgl'):
            try:
                runpy.run_path(str(script), run_name='__main__')
            except CompilerBoundary:
                pass
            else:
                raise AssertionError('generator did not reach the compiler boundary')
        text = (output / filename).read_text()
        evidence = os.environ.get('NATIVE_FIXTURE_EVIDENCE')
        if evidence:
            target = Path(evidence) / script.stem
            target.mkdir(parents=True, exist_ok=True)
            for source in output.glob('*.c'):
                (target / source.name).write_text(source.read_text())
            (target/'construction.json').write_text(json.dumps(dict(
                compiler_executed=False, lvgl_validation_mocked_for_construction=True,
                command=[str(x).replace(str(output),'<OUTPUT>') for x in compiler],
                generator=str(script.relative_to(ROOT)), inputs_sha256=inputs),indent=2)+'\n')
        return text


def function(text, name):
    match = re.search(r'^(?:static )?[^\n;]+\b' + name + r'\([^;\n]*\)\s*\{', text, re.M)
    assert match, name
    end = text.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


class NativeFixtureBoundaries(unittest.TestCase):
    def test_post_init_retains_disabled_td_geometry(self):
        text = generated(ROOT/'tests/ergohaven/run_sm62_post_init_preservation.py', ['{output}'], 'post_init.c')
        self.assertNotIn('VIAL_TAP_DANCE_ENTRIES', text)
        self.assertIn('_Static_assert(SM62_RESERVED_TD_BYTES == 40U', text)
        for name in ('kb_settings_layer_labels_init', 'sm62_migrate_factory_defaults', 'keyboard_post_init_user'):
            original = ROOT / ('keyboards/ergohaven/src/eh_settings.c' if name.startswith('kb_settings') else 'keyboards/ergohaven/sm62/sm62.c')
            self.assertIn(function(original.read_text(), name), text)
        self.assertIn('assert(writes==0)', text)
        self.assertIn('memcmp(oldkeys,keys,sizeof(keys))==0', text)
        with self.assertRaises(AssertionError):
            self.assertIn('_Static_assert(SM62_RESERVED_TD_BYTES == 40U', text.replace('SM62_RESERVED_TD_BYTES','MISSING_GEOMETRY'))

    def test_screen_has_readonly_flash_boundary(self):
        text = generated(ROOT/'tests/ergohaven/run_sm62_screen.py', ['{output}', '--lvgl', str(ROOT/'lib/lvgl')], 'screen.c')
        for name in ('sm62_cache_flash_record', 'sm62_cache_flash_program', 'sm62_cache_flash_erase'):
            self.assertRegex(text, r'\b' + name + r'\([^;]*\)\s*\{')
        self.assertIn('assert(!"read-only fixture must not write resident flash")', text)
        self.assertIn('assert(memcmp(before,w.icon_bits,sizeof(before))!=0)', text)
        self.check_hid(text)
        from native_resident_boundary import READONLY_FLASH_SOURCE
        self.assertTrue(READONLY_FLASH_SOURCE in text)
        with self.assertRaises(AssertionError):
            self.assertTrue(READONLY_FLASH_SOURCE in text.replace('assert(!"read-only fixture must not write resident flash")',''))

    def check_hid(self, text):
        hid = (ROOT/'keyboards/ergohaven/hid.c').read_text()
        actual = hid[hid.index('#    define EH_APP_LAYOUT_PROTOCOL_VERSION'):hid.index('\nhid_data_t *get_hid_data')].rsplit('#endif',1)[0]
        self.assertTrue(actual in text, 'complete production HID extraction changed')

    def check_firewall(self, text):
        self.check_hid(text)
        for name in ('combo','tap_dance','key_override'):
            processor = (ROOT/f'quantum/process_keycode/process_{name}.c').read_text()
            actual = '\n'.join(l for l in processor.splitlines() if not re.match(r'\s*#\s*(include|pragma once)',l))+'\n'
            self.assertTrue(actual in text, 'full production '+name+' processor changed')
        real = firewall_source(lambda path: (ROOT/path).read_text(), keycodes_included=True)
        self.assertTrue(real in text, 'exact production firewall dependency missing from complete generated C')
        self.assertLess(text.index('static uint16_t vial_keycode_firewall('), text.index('hid_app_layout_process_packet('))
        self.assertIn('return 0;', real)
        for name in ('sm62_cache_flash_record', 'sm62_cache_flash_program', 'sm62_cache_flash_erase'):
            self.assertTrue(re.search(r'\b' + name + r'\([^;]*\)\s*\{', text), name + ' backend definition missing')

    def test_pipeline_has_real_firewall(self):
        text = generated(HERE/'run_pipeline.py', ['--output', '{output}'], 'pipeline.c')
        self.check_firewall(text)

    def test_repeat_has_real_firewall(self):
        text = generated(HERE/'run_repeat_display.py', ['--output', '{output}'], 'repeat_display.c')
        self.check_firewall(text)
        self.assertIn('void process_tap_dance_action_on_dance_finished(', text)

    def test_scan_has_real_firewall_and_default_trace_header(self):
        text = generated(HERE/'run_scan_timing.py', ['--output', '{output}'], 'scan_timing.c')
        self.check_firewall(text)
        self.assertIn('#include "sm62_trace.h"', text)
        self.assertNotIn('#define SM62_STAGE_TRACE', text)
        self.assertIn('sm62_trace_count(13)', text)
        self.assertIn('sm62_trace_loop()', text)
        with self.assertRaises(AssertionError):
            self.assertIn('#include "sm62_trace.h"', text.replace('#include "sm62_trace.h"',''))


    def test_restart_uses_actual_c_producer_not_interop_inputs(self):
        spec = importlib.util.spec_from_file_location('native_ci_contract', Path(__file__).with_name('run_ci.py'))
        assert spec is not None and spec.loader is not None
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        commands = runner.commands(Path('/external/output'), Path('/external/lvgl'))
        self.assertEqual(len(commands), 50)
        restart = next(c for c in commands if c[1].endswith('test_resident100_restart.py'))
        self.assertEqual(restart[3], '/external/output/resident100/fixtures')
        resident = Path(__file__).with_name('run_resident100.py').read_text()
        self.assertIn('SM62_FIXTURE_DIR', resident)
        consumer = Path(__file__).with_name('test_resident100_restart.py').read_text()
        self.assertIn("bytes.fromhex(state['name_hex'])==b'A'*22", consumer)
        self.assertIn("expected_key=9", consumer)
        # Existing independent interop fixture coverage remains in the wave.
        self.assertTrue(any(c[1].endswith('sm62_interop/fixtures.py') for c in commands))

    def test_resident_producer_and_peer_keep_actual_c_contract(self):
        text = generated(ROOT/'tests/ergohaven/run_resident100.py', ['{output}'], 'compact_receiver.c')
        producer = (ROOT/'tests/ergohaven/resident100_native.c').read_text()
        for name in ('fixture', 'fixture_max', 'upload'):
            self.assertIn(function(producer, name), text)
        self.assertIn('fixture_max(s+1);upload();', text)
        self.assertIn('getenv("SM62_FIXTURE_DIR")', text)
        self.assertIn('fwrite(raw,1,raw_size,f)==raw_size', text)
        peer = generated(ROOT/'tests/ergohaven/run_resident100_peer.py', ['{output}'], 'compact_receiver.c')
        self.assertIn('app_layout_keycodes[0][0]', peer)
        self.assertIn('hid_app_layout_get_name(0,name,32)', peer)
        self.assertIn('hid_app_layout_process_keyevent(row,col,down)', peer)
        # Production geometry maps exactly the asserted callback position.
        geometry = (ROOT/'keyboards/ergohaven/sm62/application_geometry.h').read_text()
        self.assertRegex(geometry, r'sm62_application_positions\[62\]\[2\] = \{\s*\{0, 5\}')

    def test_restart_existing_assertions_reject_corrupt_observations(self):
        # Exercise the consumer's actual Python assertions, NOT a fake C peer.
        # Reference input describes fixture_max's maximum FULL record fields.
        tree = ast.parse(Path(__file__).with_name('test_resident100_restart.py').read_text())
        assertions = [n for n in ast.walk(tree) if isinstance(n, ast.Assert)]
        state_check = next(n for n in assertions if "state['active']" in ast.unparse(n))
        edge_check = next(n for n in assertions if "down['handled']" in ast.unparse(n))
        admission = next(n for n in assertions if n.msg and 'require actual resident100 C producer fixtures' in ast.unparse(n.msg))
        raw = bytearray(4560)
        raw[3:7] = struct.pack('<I', 17)
        raw[64:66] = struct.pack('<H', 9)
        raw[4192:4215] = bytes([22])+b'A'*22
        state = dict(active=1,revision=17,key0=9,writes=0,name_hex=(b'A'*22).hex())
        down = dict(handled=1,ups=3,down_code=9)
        up = dict(handled=1,ups=4,up_code=9)
        env = dict(raw=raw,raws=[raw],state=state,down=down,up=up,expected_key=9,struct=struct)
        def evaluate(node, values):
            exec(compile(ast.fix_missing_locations(ast.Module(body=[node],type_ignores=[])), '<actual restart assertion>', 'exec'), values)
        for check in (state_check, edge_check, admission):
            evaluate(check, dict(env))
        for key, value in [('active',0),('revision',18),('key0',10),('writes',1),('name_hex',(b'B'*22).hex())]:
            with self.subTest(state=key), self.assertRaises(AssertionError):
                evaluate(state_check, dict(env,state=dict(state,**{key:value})))
        for key,value in [('handled',0),('ups',3),('up_code',10)]:
            with self.subTest(edge=key), self.assertRaises(AssertionError):
                evaluate(edge_check,dict(env,up=dict(up,**{key:value})))
        from sm62_interop.fixtures import fixture
        with self.assertRaisesRegex(AssertionError, 'require actual resident100 C producer fixtures'):
            evaluate(admission,dict(env,raws=[fixture(0)[0]]))

    def test_permissive_firewall_mutations_are_rejected(self):
        real = firewall_source(lambda path: (ROOT/path).read_text(), keycodes_included=True)
        for script,filename in [('run_pipeline.py','pipeline.c'),('run_repeat_display.py','repeat_display.c'),('run_scan_timing.py','scan_timing.c')]:
            text = generated(HERE/script, ['--output','{output}'], filename)
            mutant = text.replace(real, 'static uint16_t vial_keycode_firewall(uint16_t in){return in;}\n')
            with self.subTest(script=script), self.assertRaisesRegex(AssertionError, 'exact production firewall'):
                self.check_firewall(mutant)

    def test_peer_controls_are_self_contained_and_source_pinned(self):
        text = generated(HERE/'run_peer_capture.py', ['--out', '{output}'], 'current.c')
        self.assertIn('captured_press.released_at==120', text)
        script = (HERE/'run_peer_capture.py').read_text()
        self.assertNotIn('review-fix-before/', script)
        self.assertNotIn('sm62-test14-display-cooperative/', script)
        self.assertIn('peer_capture_controls.json', script)
        self.assertIn('sha256', script)

    def test_historical_resident_closure_is_offline_and_corruption_fails_closed(self):
        from resident_legacy_source import SHA256, historical_source
        with patch.object(subprocess, 'check_output', side_effect=AssertionError('no Git/history dependency')):
            for name, expected in SHA256.items():
                self.assertEqual(hashlib.sha256(historical_source(ROOT, name).encode()).hexdigest(), expected)
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                name = 'keyboards/ergohaven/hid.c'
                target = root / 'tests/ergohaven/fixtures/resident_4_2_0' / name
                target.parent.mkdir(parents=True)
                target.write_text(historical_source(ROOT, name) + '\n/* corrupt control */\n')
                with self.assertRaisesRegex(AssertionError, 'historical source digest'):
                    historical_source(root, name)

    def test_peer_pin_corruption_rejected_before_compilation(self):
        original = Path.read_text
        def corrupted(path,*args,**kwargs):
            text = original(path,*args,**kwargs)
            if path.name == 'peer_capture_controls.json':
                controls = json.loads(text)
                controls['controls'][0]['hooks'] += '\n/* changed */\n'
                return json.dumps(controls)
            return text
        with patch.object(Path,'read_text',corrupted), self.assertRaisesRegex(AssertionError, 'baseline-peer-missing'):
            generated(HERE/'run_peer_capture.py', ['--out','{output}'], 'current.c')


if __name__ == '__main__':
    unittest.main()
