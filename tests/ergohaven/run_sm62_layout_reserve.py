#!/usr/bin/env python3
"""Native compiler/linker protection probes; no device or firmware build."""
import atexit
import json
import re
import subprocess
import sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
S = R / 'keyboards/ergohaven/sm62'
O = Path(sys.argv[1]).resolve()
O.mkdir(parents=True, exist_ok=True)
results = []
atexit.register(lambda: (O/'results.json').write_text(json.dumps(results, indent=2)+'\n'))

def run(cmd):
    p = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    results.append(dict(command=cmd, exit=p.returncode, output=p.stdout))
    return p

# Behavioral RED on the accepted baseline: a 2 MiB build must be refused by
# the actual production backend, not by a copied test-only geometry model.
p = run([sys.executable, str(R/'tests/ergohaven/run_resident_flash_backend.py'), str(O/'backend')])
assert p.returncode == 0, p.stdout
backend = O/'backend'
header = S/'layout_reserve.h'
if header.exists():
    (backend/'layout_reserve.h').write_bytes(header.read_bytes())
base = ['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-I'+str(backend/'include'), '-DSM62_CODE_FLASH_LIMIT=0x1f2000']
def compile_backend(flash):
    return run(base + ['-DPICO_FLASH_SIZE_BYTES='+str(flash), '-c', str(backend/'resident_cache_flash.c'), '-o', str(O/'probe.o')])
p = compile_backend(2097152)
assert p.returncode != 0, 'Missing reservation protection: production backend accepts 2 MiB geometry'
assert 'SM62 reservation requires 4 MiB' in p.stdout, p.stdout
assert compile_backend(4194304).returncode == 0
# Compile-time mutation controls of the real header, never weakened assertions.
original = header.read_text()
mutations = {
    'unaligned': ('0x200000u', '0x200001u'),
    'cache_overlap': ('0x2ca000u', '0x3f4000u'),
    'too_small': ('8192u', '4096u'),
    'too_few_records': ('100u', '99u'),
    'payload_growth': ('4560u', '8000u'),
}
for name, (old, new) in mutations.items():
    assert old in original
    (backend/'layout_reserve.h').write_text(original.replace(old, new))
    p = compile_backend(4194304)
    assert p.returncode != 0 and 'static assertion failed' in p.stdout, (name, p.stdout)
(backend/'layout_reserve.h').write_text(original)
assert int(re.search(r'#define APP_COMPACT_LIMIT (\d+)', (S/'compact_usb.h').read_text())[1]) == 4560
probe = O/'geometry.c'
probe.write_text('''#include <stdio.h>
#include "layout_reserve.h"
int main(void) {
 printf("%u %u %u %u %u %u %u\\n", SM62_LAYOUT_RESERVE_BASE, SM62_LAYOUT_RESERVE_END,
 SM62_LAYOUT_RESERVED_LOGICAL_RECORDS, SM62_LAYOUT_RESERVED_SPARES,
 SM62_LAYOUT_RESERVED_RECORD_SIZE, SM62_LAYOUT_RESERVE_BYTES, SM62_LAYOUT_FULL_PAYLOAD_MAX);
}
''')
p = run(base + ['-I'+str(backend), '-DPICO_FLASH_SIZE_BYTES=4194304', str(probe), '-o', str(O/'geometry')])
assert p.returncode == 0, p.stdout
p = run([str(O/'geometry')])
assert p.stdout.strip() == '2097152 2924544 100 1 8192 827392 4560', p.stdout
# Use the production supplementary linker script, with the same FLASH_LEN flag.
for limit, success in [('0x1f2000', True), ('0x200001', False)]:
    p = run(['ld', '-r', '--defsym=FLASH_LEN='+limit, '-T', str(S/'layout_reserve.ld'), str(O/'probe.o'), '-o', str(O/'linked.o')])
    assert (p.returncode == 0) == success, p.stdout
    if success:
        symbols = run(['nm', str(O/'linked.o')])
        assert re.search(r'10200000 A __sm62_layout_reserve_start', symbols.stdout)
        assert re.search(r'102ca000 A __sm62_layout_reserve_end', symbols.stdout)
    else:
        assert 'SM62 code overlaps layout reservation' in p.stdout, p.stdout
# Evaluate SM62 rules with make: the actual C and linker flags must agree.
makefile = O/'flags.mk'
makefile.write_text('include '+str(S/'rules.mk')+'\nprint:\n\t@printf "%s\\n" "$(OPT_DEFS)" "$(LDFLAGS)"\n')
p = run(['make', '--no-print-directory', '-f', str(makefile), 'print'])
assert p.returncode == 0, p.stdout
assert '-DPICO_FLASH_SIZE_BYTES=4194304' in p.stdout
assert '-DSM62_CODE_FLASH_LIMIT=0x1f2000' in p.stdout
assert '--defsym=FLASH_LEN=0x1f2000' in p.stdout
assert '-T,keyboards/ergohaven/sm62/layout_reserve.ld' in p.stdout
(O/'results.json').write_text(json.dumps(results, indent=2)+'\n')
print('PASS production backend 4MiB/rejected 2MiB; 5 invalid geometries; exact 100+1 record capacity; linker overlap rejection; unchanged backend behavior')
