#!/usr/bin/env python3
"""Actual C process restart, persisted NOR model; framing only in Python.
Arguments: PEER_EXE FIXTURE_DIRECTORY NEW_OUTPUT_DIRECTORY.
"""
from pathlib import Path
import json,struct,subprocess,sys,zlib
exe=Path(sys.argv[1]).resolve();fixtures=Path(sys.argv[2]);out=Path(sys.argv[3]).resolve();out.mkdir(parents=True,exist_ok=True)
flash=out/'flash.bin';assert not flash.exists(),'require fresh output'
raws=[(fixtures/f'full{s:02}.bin').read_bytes() for s in range(100)]
assert all(len(raw)==4560 for raw in raws) and len(set(raws))==100
class Peer:
 def __init__(self):self.p=subprocess.Popen([str(exe),str(flash)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
 def line(self,s):
  self.p.stdin.write(s+'\n');self.p.stdin.flush();r=self.p.stdout.readline().strip();assert r,'C peer exited';return r
 def send(self,b):
  assert len(b)==32;r=self.line(b.hex());return None if r=='-' else bytes.fromhex(r)
 def close(self):self.p.stdin.close();assert self.p.wait(timeout=10)==0
 def state(self):return json.loads(self.line('STATE'))
def command(p,op,s,raw,token):
 q=bytes([239,9,op,s])+struct.pack('<I',token)+(raw[43:63] if op!=3 else bytes(20))+struct.pack('<I',zlib.crc32(raw) if op!=3 else 0)
 a=p.send(q);assert a[:3]==bytes([239,9,op|128]) and a[4:8]==q[4:8]
 if op!=3:assert a[8:]==q[8:]
 return a
def stage(p,raw,token):
 n=(len(raw)+23)//24;crc=zlib.crc32(raw)
 q=bytes([236,8,2,0])+struct.pack('<IHHHHII',token,len(raw),len(raw),n,0,crc,crc)+bytes(8)
 assert p.send(q) is None
 for s in range(n):assert p.send(bytes([237,8])+struct.pack('<IH',token,s)+raw[s*24:s*24+24].ljust(24,b'\0')) is None
 assert p.send(bytes([238,8])+struct.pack('<IHI',token,n,crc)+bytes(20)) is None
p=Peer()
for s,raw in enumerate(raws):
 stage(p,raw,s+1);assert command(p,5,s,raw,s+1)[3]==0;assert command(p,1,s,raw,s+1)[3]==0
 assert p.state()['active']==0
p.close()
p=Peer();assert p.state()['active']==0
for s,raw in enumerate(raws):
 assert command(p,0,s,raw,1000+s)[3]==0
 a=command(p,3,s,raw,2000+s);assert a[3]==0 and a[8:28]==raw[43:63] and struct.unpack('<I',a[28:])[0]==zlib.crc32(raw)
 assert command(p,2,s,raw,3000+s)[3]==0
 state=p.state();assert state['active']==1 and state['revision']==s+1 and state['key0']==9 and state['writes']==0 and bytes.fromhex(state['name_hex'])==b'A'*22
 down=json.loads(p.line('KEY 0 5 1'));up=json.loads(p.line('KEY 0 5 0'));assert down['handled']==1 and up['handled']==1 and up['ups']==down['ups']+1 and down['down_code']==9 and up['up_code']==9
 # Recover exact original full payload via real READ, before/after ENUM.
 recovered=b''
 for off in range(0,len(raw),20):
  q=bytes([239,9,6,s])+struct.pack('<IHB',4000+off,off,20)+bytes(21);a=p.send(q)
  assert a[:10]==bytes([239,9,134,s])+q[4:10] and a[11]==0
  recovered+=a[12:12+a[10]]
 assert recovered==raw and command(p,3,s,raw,5000+s)[8:]==raw[43:63]+struct.pack('<I',zlib.crc32(raw))
assert p.line('TIME 2500')=='OK' and p.state()['active']==0
for s in (0,99):assert command(p,4,s,raws[s],6000+s)[3]==0
p.close();p=Peer()
for s in range(100):assert command(p,0,s,raws[s],7000+s)[3]==(1 if s in (0,99) else 0)
assert p.state()['writes']==0;p.close()
result={'pass':True,'stored_full_records':100,'record_bytes':4560,'processes':3,'read_roundtrip_bytes':sum(map(len,raws)),'restart_select_flash_writes':0,'deleted_slots_after_restart':[0,99],'boundary':'actual C parser/callback/VIA/store; OS HID, clock, key sinks and NOR are modeled'}
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
