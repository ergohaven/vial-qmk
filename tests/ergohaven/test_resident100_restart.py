#!/usr/bin/env python3
"""Actual C restarts: current read-only migration and pinned historical controls.
Arguments: PEER_EXE FIXTURE_DIRECTORY NEW_OUTPUT_DIRECTORY.
"""
from pathlib import Path
import hashlib,json,shutil,struct,subprocess,sys,zlib
from resident_legacy_source import PIN,historical_source

exe=Path(sys.argv[1]).resolve();fixtures=Path(sys.argv[2]).resolve();out=Path(sys.argv[3]).resolve()
out.mkdir(parents=True,exist_ok=True)
flash=out/'flash.bin';assert not flash.exists(),'require fresh output'
raws=[(fixtures/f'full{s:02}.bin').read_bytes() for s in range(100)]
assert all(len(raw)==4560 for raw in raws) and len(set(raws))==100
assert all(struct.unpack('<H',raw[64:66])[0]==9 and raw[4192:4215]==bytes([22])+b'A'*22 and raw[3040]==0 for raw in raws),'require actual resident100 C producer fixtures'
archive=fixtures.parent/'historical-archive.bin'
assert archive.stat().st_size==101*8192,'require authentic pinned old-writer archive'
original=archive.read_bytes();shutil.copyfile(archive,flash)
class Peer:
 def __init__(self):self.p=subprocess.Popen([str(exe),str(flash)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
 def line(self,s):
  self.p.stdin.write(s+'\n');self.p.stdin.flush();r=self.p.stdout.readline().strip();assert r,'C peer exited';return r
 def send(self,b):
  assert len(b)==32;r=self.line(b.hex());return None if r=='-' else bytes.fromhex(r)
 def close(self):self.p.stdin.close();assert self.p.wait(timeout=10)==0
 def state(self):return json.loads(self.line('STATE'))
def command(p,op,s,raw,token,status=0):
 q=bytes([239,9,op,s])+struct.pack('<I',token)+(raw[43:63] if op!=3 else bytes(20))+struct.pack('<I',zlib.crc32(raw) if op!=3 else 0)
 a=p.send(q);assert a is not None
 assert a[:3]==bytes([239,9,op|128]) and a[4:8]==q[4:8] and a[3]==status
 if op!=3:assert a[8:]==q[8:]
 return a
def stage(p,raw,token,mode=1):
 n=(len(raw)+23)//24;crc=zlib.crc32(raw)
 q=bytes([236,8,mode,0])+struct.pack('<IHHHHII',token,len(raw),len(raw),n,0,crc,crc)+bytes(8)
 assert p.send(q) is None
 for s in range(n):assert p.send(bytes([237,8])+struct.pack('<IH',token,s)+raw[s*24:s*24+24].ljust(24,b'\0')) is None
 assert p.send(bytes([238,8])+struct.pack('<IHI',token,n,crc)+bytes(20)) is None
read_bytes=0
for boot in range(3):
 p=Peer();assert p.state()['active']==0 and p.state()['writes']==0
 for s,raw in enumerate(raws):
  command(p,0,s,raw,1000+s)
  a=command(p,3,s,raw,2000+s);assert a[8:28]==raw[43:63] and struct.unpack('<I',a[28:])[0]==zlib.crc32(raw)
  recovered=b''
  for off in range(0,len(raw),20):
   q=bytes([239,9,6,s])+struct.pack('<IHB',4000+off,off,20)+bytes(21);a=p.send(q)
   assert a is not None
   assert a[:10]==bytes([239,9,134,s])+q[4:10] and a[11]==0
   recovered+=a[12:12+a[10]]
  assert recovered==raw;read_bytes+=len(raw)
  before=p.state()
  for op in (1,2,4,5):command(p,op,s,raw,5000+s,status=2)
  assert p.state()==before,'removed mutators must not publish, delete or alter RAM'
  if boot==0:
   stage(p,raw,s+1)
   state=p.state();expected_key=9;assert state['active']==1 and state['revision']==struct.unpack('<I',raw[3:7])[0] and state['key0']==expected_key and state['writes']==0 and bytes.fromhex(state['name_hex'])==b'A'*22
   down=json.loads(p.line('KEY 0 5 1'));up=json.loads(p.line('KEY 0 5 0'))
   assert down['handled']==1 and up['handled']==1 and up['ups']==down['ups']+1 and down['down_code']==expected_key and up['up_code']==expected_key
 p.line('TIME 2499')
 if boot==0:assert p.state()['active']==1
 assert p.line('TIME 1')=='OK' and p.state()['active']==0
 assert p.state()['writes']==0;p.close()
 assert flash.read_bytes()==original,'current restarts must preserve the entire old archive byte-for-byte'
# Preserve every original writer/restart/select/delete assertion on its verified
# historical peer, explicitly separate from current read-only qualification.
legacy=exe.parent/'historical-4.2.0'/'resident100_peer'
build=json.loads((exe.parent/'build.json').read_text())
assert build['historical_commit']==PIN and build['historical_not_shipping']
assert hashlib.sha256(legacy.read_bytes()).hexdigest()==build['historical_sha256']
script=historical_source(Path(__file__).resolve().parents[2],'tests/ergohaven/test_resident100_restart.py')
legacy_out=out/'historical-4.2.0'
r=subprocess.run([sys.executable,'-c',script,str(legacy),str(fixtures),str(legacy_out)],capture_output=True,text=True)
(out/'historical-restart.log').write_text(r.stdout+r.stderr)
assert r.returncode==0,r.stdout+r.stderr
legacy_result=json.loads((legacy_out/'result.json').read_text())
result={'pass':True,'current':{'processes':3,'archive_records':100,'read_roundtrip_bytes':read_bytes,'archive_sha256':hashlib.sha256(original).hexdigest(),'archive_byte_preserved':True,'ram_publications':100,'archive_writes':0,'removed_mutations_refused':True,'lease_boundaries_ms':[2499,2500]},'historical':{'classification':'4.2.0-only; not shipping qualification','commit':PIN,'result':legacy_result},'boundary':'actual C parser/callback/VIA with modeled clock, key sinks and NOR'}
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
