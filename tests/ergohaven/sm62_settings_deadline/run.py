#!/usr/bin/env python3
"""Focused native deadline regression; invoke through agent-storage run.

Retains complete real settings translation unit and physical-side helpers,
removing only hardware includes. No copied settings functions or timer policy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--phase', required=True)
p.add_argument('--settings-source', type=Path, help='Optional archived production C for baseline replay')
a = p.parse_args()
here = Path(__file__).resolve().parent
root = here.parents[2]
cache = Path(os.environ['AGENT_BUILD_CACHE']).resolve()
out = Path(tempfile.mkdtemp(prefix='settings-deadline-' + a.phase + '-', dir=cache))
print('OUTPUT=' + str(out), flush=True)
inputs = [here / 'boundary.h', root / 'keyboards/ergohaven/sm62/sm62_settings.h',
          root / 'keyboards/ergohaven/sm62/sm62_side.h',
          a.settings_source or root / 'keyboards/ergohaven/sm62/sm62_settings.c', here / 'cases.c']
blobs = [f.read_bytes() for f in inputs]
manifest = {str(f): hashlib.sha256(b).hexdigest() for f, b in zip(inputs, blobs)}
manifest[str(Path(__file__).resolve())] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(out / 'source-hashes.json').write_text(json.dumps(manifest, indent=2) + '\n')
(out / 'settings-production.c').write_bytes(blobs[3])
def strip(b):
    return re.sub(r'^\s*#\s*(?:include[^\n]*|pragma once)[^\n]*', '', b.decode(), flags=re.M)
source = '\n'.join([blobs[0].decode(), *(strip(b) for b in blobs[1:4]), blobs[4].decode()])
(out / 'fixture.c').write_text(source)
cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', '-g', str(out / 'fixture.c'), '-o', str(out / 'deadline')]
(out / 'command.json').write_text(json.dumps(cmd))
r = subprocess.run(cmd, text=True, capture_output=True)
(out / 'compile.log').write_text(r.stdout + r.stderr)
if r.returncode:
    print(r.stdout + r.stderr); raise SystemExit(r.returncode)
results = []
with (out / 'tests.log').open('w') as log:
    for case in ['repeated', 'none', 'single', 'late', 'remote']:
        for left in [0, 1]:
            for master in [0, 1]:
                for epoch in [10000, 4294966000]:
                    args = [str(out / 'deadline'), case, str(left), str(master), str(epoch)]
                    r = subprocess.run(args, text=True, capture_output=True)
                    log.write('$ ' + ' '.join(args) + '\n' + r.stdout + r.stderr)
                    results.append({'case': case, 'left': left, 'master': master, 'epoch': epoch, 'exit': r.returncode})
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
failed = [r for r in results if r['exit']]
summary = {'total': len(results), 'passed': len(results) - len(failed), 'failed': len(failed),
           'failing_cases': sorted({r['case'] for r in failed})}
(out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary), flush=True)
raise SystemExit(bool(failed))
