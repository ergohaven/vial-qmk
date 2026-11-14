#!/usr/bin/env python3
"""Compatibility entry point: authoritative resident100 supersedes three slots.
The historical resident_three_native.c fixture is retained for provenance only;
its slot0-as-Default and eviction assertions contradict schema2 CONTRACT.
"""
from pathlib import Path
import subprocess,sys
print('resident-three gate superseded by resident100 contract; running actual production resident100 tests',flush=True)
raise SystemExit(subprocess.run([sys.executable,str(Path(__file__).with_name('run_resident100.py')),*sys.argv[1:]]).returncode)
