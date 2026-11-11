#!/usr/bin/env python3
"""Bounded one-slot public protocol/reboot characterization, no peer model.
FULL fixture producer recovered from sm62-complete-layouts-interop/qualify.py.
"""
from pathlib import Path
import argparse,subprocess,json,struct,zlib,binascii,hashlib,os
if not __debug__:
 raise SystemExit('selfcheck requires assertions enabled; optimized Python is unsupported')
ap=argparse.ArgumentParser();ap.add_argument('--exe',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
exe=a.exe.resolve();out=a.output.resolve()
if not a.output.is_absolute() or os.path.lexists(a.output) or not out.parent.is_dir():
 raise SystemExit('output must be a fresh absolute path with an existing parent')
if Path(os.environ['AGENT_BUILD_CACHE']).resolve() not in out.parents:
 raise SystemExit('output must be inside AGENT_BUILD_CACHE')
out.mkdir();image=out/'flash.bin';trace=[]
import atexit
atexit.register(lambda: (out/'wire.json').write_text(json.dumps(trace,indent=2)+'\n'))
class Peer:
 def __init__(self): self.p=subprocess.Popen([str(exe),str(image)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 def cmd(self,s):
  self.p.stdin.write(s+'\n');self.p.stdin.flush();line=self.p.stdout.readline()
  assert line, (s,self.p.poll(),self.p.stderr.read())
  value=json.loads(line);trace.append({'request':s,'reply':value});return value
 def exchange(self,q,report_id=False):
  assert len(q)==32
  r=self.cmd((b'\0'+q if report_id else q).hex())
  replies=[bytes.fromhex(x) for x in r['responses']]
  assert all(len(x)==32 for x in replies)
  return replies
 def one(self,q,report_id=False):
  r=self.exchange(q,report_id);assert len(r)==1,(q.hex(),r);return r[0]
 def close(self):
  self.p.stdin.close();assert self.p.wait(timeout=20)==0,self.p.stderr.read()
peer=Peer()
info=peer.cmd('INFO')
assert tuple(info[x] for x in ['macro_count','macro_size','layers','rows','cols','controls','resident_slots','td'])==(109,6066,16,10,7,62,100,0),info
q=bytes([0x0c])+bytes(31);assert peer.one(q)==bytes([0x0c,109])+bytes(30)
assert peer.one(q,True)==bytes([0x0c,109])+bytes(30)
# Exact public QK_BOOT assignment firewall while locked, no bypass.
q=bytes([5,0,0,5,0x7c,0])+bytes(26);assert peer.one(q)==q
q=bytes([4,0,0,5])+bytes(28);assert peer.one(q)==q # forbidden code becomes KC_NO
q=bytes([5,15,9,3,0,9])+bytes(26);assert peer.one(q,True)==q
q=bytes([4,15,9,3])+bytes(28);assert peer.one(q)==q[:4]+bytes([0,9])+bytes(26)
# Locked macro mutation must be explicitly refused; public physical unlock
# runs the actual matrix-task timer, never assigns vial_unlocked in a test.
q=bytes([15,0,0,4])+b'abc\0'+bytes(24);assert peer.one(q)==bytes([255])+q[1:]
peer.one(bytes([0xfe,6])+bytes(30))
peer.cmd('MATRIX 1 48');peer.cmd('TICK 2999')
assert peer.one(bytes([0xfe,7])+bytes(30))[0]==0
peer.cmd('TICK 1');assert peer.one(bytes([0xfe,7])+bytes(30))[:2]==bytes([1,0])
peer.cmd('MATRIX 1 0')
# Real overlay NOR setter, persisted across new process.
q=bytes([15,0,0,4])+b'abc\0'+bytes(24);assert peer.one(q)==q
q=bytes([14,0,0,4])+bytes(28);assert peer.one(q)==q[:4]+b'abc\0'+bytes(24)
def fixture(slot):
 identity=hashlib.sha1(f'c-interop-{slot}'.encode()).digest();name=f'App-{slot:03}-'.encode().ljust(22,b'N')
 keys=b''.join(struct.pack('<H',4+slot+l*62+k) for l in range(16) for k in range(62));visuals=bytes((slot+i)%17 for i in range(992))
 stacks=b'';crcstacks=b''
 for l in range(16):
  crcstacks+=bytes([4])
  for s in range(4):
   n=f'S{slot:03}L{l:02}X{s}'.encode().ljust(12,b'x');codes=struct.pack('<HH',4+slot+s,9+l+s)
   stacks+=bytes([4,12])+codes+n;crcstacks+=codes+bytes([12])+n
 layers=b''.join(bytes([22])+f'L{l:02}-Slot{slot:03}'.encode().ljust(22,b'L') for l in range(16))
 crc=binascii.crc_hqx(keys+visuals+crcstacks+bytes([22])+name+layers,0xffff)
 begin=bytearray(32);begin[:3]=bytes([0xe1,8,1]);begin[3:7]=struct.pack('<I',slot+1);begin[7:10]=bytes([62,16,22]);begin[10:32]=name
 commit=bytearray(32);commit[:3]=bytes([0xe3,8,1]);commit[3:7]=begin[3:7];commit[7:9]=struct.pack('<H',crc);commit[9:11]=b'W\2';commit[11:31]=identity
 raw=bytes(begin+commit)+keys+visuals+stacks+layers;assert len(raw)==4560
 return raw,identity,zlib.crc32(raw)
def resident(op,slot,identity=bytes(20),crc=0,token=123):
 q=bytes([0xef,9,op,slot])+struct.pack('<I',token)+identity+struct.pack('<I',crc);r=peer.one(q);assert r[:3]==bytes([0xef,9,op|128]);assert r[4:8]==q[4:8];assert r[3]==0,(op,slot,r.hex());return r
def upload(raw,crc):
 n=(len(raw)+23)//24;q=bytearray([0xec,8,2,0])+bytearray(struct.pack('<IHHH',123,len(raw),len(raw),n))+bytearray(18);q[16:24]=struct.pack('<II',crc,crc);assert peer.exchange(q)==[]
 for seq,off in enumerate(range(0,len(raw),24)):
  q=bytes([0xed,8])+struct.pack('<IH',123,seq)+raw[off:off+24].ljust(24,b'\0');assert peer.exchange(q)==[]
 assert peer.exchange(bytes([0xee,8])+struct.pack('<IH',123,n)+struct.pack('<I',crc)+bytes(20))==[]

raw,identity,crc=fixture(0)
upload(raw,crc);resident(5,0,identity,crc);resident(1,0,identity,crc)
assert peer.cmd('INFO')['nor_writes']>0
peer.close();peer=Peer();before=peer.cmd('INFO')
assert before['nor_writes']==before['eeprom_writes']==0
q=bytes([14,0,0,4])+bytes(28);assert peer.one(q,True)==q[:4]+b'abc\0'+bytes(24)
q=bytes([4,15,9,3])+bytes(28);assert peer.one(q)==q[:4]+bytes([0,9])+bytes(26)
resident(0,0,identity,crc);r=resident(3,0);assert r[8:28]==identity and int.from_bytes(r[28:32],'little')==crc
got=bytearray()
for off in range(0,len(raw),20):
 q=bytes([0xef,9,6,0])+struct.pack('<IHB',456,off,20)+bytes(21);r=peer.one(q)
 assert r[:4]==bytes([0xef,9,0x86,0]) and r[4:8]==struct.pack('<I',456) and r[8:10]==struct.pack('<H',off) and r[11]==0
 got+=r[12:12+r[10]]
assert got==raw
resident(2,0,identity,crc)
for l,c in [(0,0),(15,61)]:
 assert peer.cmd(f'KEY {l} {c}')=={'key':4+l*62+c,'visual':(l*62+c)%17,'active':1}
after=peer.cmd('INFO');assert after['nor_writes']==after['eeprom_writes']==0
peer.cmd('TICK 1')
peer.close()
result={'status':'PASS','info':info,'post_restart':after,'resident_raw_bytes':len(raw),'executable':str(exe),'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'checks':['32-byte and 00-prefixed 33-byte exact report framing','locked native QK_BOOT assignment firewall','locked macro mutation refused','actual 2999ms refusal and 3000ms matrix-task unlock','last native key persists','macro NOR write and fresh-process recovery','FULL compact stage/ready/store','cold resident query/enum/full READ/select','zero writes during recovery/read/select','live first and last key/visual','TICK JSON']}
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
(out/'wire.json').write_text(json.dumps(trace,indent=2)+'\n')
