#!/usr/bin/env python3
"""Current read-only archive/RAM qualification + separately pinned 4.2.0 controls."""
from pathlib import Path
import json, os, subprocess, sys
from resident_legacy_source import PIN, historical_source, receipt
R=Path(__file__).resolve().parents[2]
O=Path(sys.argv[1]).resolve(); O.mkdir(parents=True,exist_ok=True)
# Restart consumes this actual current C producer, not separate interop inputs.
fixtures=Path(os.environ.get('SM62_FIXTURE_DIR',str(O/'fixtures'))).resolve()
fixtures.mkdir(parents=True,exist_ok=True)
base=(R/'tests/ergohaven/run_compact_usb.py').read_text()
def build(native, output, ref=None):
 output.mkdir(parents=True,exist_ok=True)
 script=base[:base.index("main=r'''")]+"main=r'''\n"+native+"'''\n"+base[base.index('text=prefix+prod'):base.index('# Baseline mode')]+"subprocess.run(cmd,check=True)\n"
 script=script.replace("R=Path(__file__).resolve().parents[2]",'R=Path('+repr(str(R))+')')
 runner=output/'runner.py';runner.write_text(script)
 subprocess.run([sys.executable,str(runner),str(output)]+(['--ref',ref] if ref else []),check=True)
 return output/'compact_receiver'
# Compile current first: construction contracts inspect the shipping boundary.
current=build((R/'tests/ergohaven/resident100_native.c').read_text(),O)
legacy=historical_source(R,'tests/ergohaven/resident100_native.c')
# Keep every historical assertion/fault campaign unchanged. After it passes,
# produce an authentic schema2 archive with the pinned old production writer.
legacy=legacy.replace('int main(void)', 'static int legacy_main(void)')+r'''
int main(void) {
 int result=legacy_main();assert(!result);
 memset(disk,255,sizeof(disk));cache_scanned=false;app_compact_reset();budget=tear=-1;
 for(unsigned s=0;s<100;s++){fixture_max(s+1);assert(cache_store(s,raw+43,app_compact_crc(raw,raw_size),raw,raw_size));}
 const char *path=getenv("SM62_ARCHIVE_IMAGE");assert(path);FILE*f=fopen(path,"wb");assert(f&&fwrite(disk,1,sizeof(disk),f)==sizeof(disk)&&!fclose(f));
 return 0;
}
'''
old=build(legacy,O/'historical-4.2.0',PIN)
archive=O/'historical-archive.bin'
env=dict(os.environ,SM62_FIXTURE_DIR=str(fixtures),SM62_ARCHIVE_IMAGE=str(archive))
results=[]
for label,exe in [('historical-4.2.0-only-not-shipping',old),('current-readonly-host-managed',current)]:
 p=subprocess.run([str(exe)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 (exe.parent/'result.log').write_text(p.stdout+f'\nEXIT={p.returncode}\n');print(label+'\n'+p.stdout,flush=True)
 results.append(dict(classification=label,exit=p.returncode))
 if p.returncode:break
receipt(O/'historical-source.json')
(O/'result.json').write_text(json.dumps(dict(results=results,legacy_commit=PIN,archive=str(archive),fixtures=str(fixtures)),indent=2)+'\n')
raise SystemExit(int(any(r['exit'] for r in results)))
