#!/usr/bin/env python3
"""Native production HID/VIA and journal behavioral regression; no hardware."""
from pathlib import Path
import subprocess, sys
R=Path(__file__).resolve().parents[2]
O=Path(sys.argv[1]); O.mkdir(parents=True,exist_ok=True)
base=(R/'tests/ergohaven/run_compact_usb.py').read_text()
# Reuse the existing production-extraction boundary, not a model parser.
base=base[:base.index("main=r'''")]+"main=r'''\n"+(R/'tests/ergohaven/resident100_native.c').read_text()+"'''\n"+base[base.index('text=prefix+prod'):base.index('# Baseline mode')]+"subprocess.run(cmd,check=True)\nraise SystemExit(subprocess.run([str(exe)]).returncode)\n"
runner=O/'runner.py';runner.write_text(base.replace("R=Path(__file__).resolve().parents[2]",'R=Path('+repr(str(R))+')'))
log=O/'result.log'
p=subprocess.run([sys.executable,str(runner),str(O)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
log.write_text(p.stdout+f'\nEXIT={p.returncode}\n');print(p.stdout);raise SystemExit(p.returncode)
