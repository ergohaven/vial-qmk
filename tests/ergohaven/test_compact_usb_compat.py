#!/usr/bin/env python3
"""Legacy210-report and RAW/LZ compact activation through actual C/VIA receiver.
Python constructs frames only; parsing/decoding/COMMIT are production C.
Usage: script FIXTURE4560 OUTPUT.
"""
from pathlib import Path
import json,struct,subprocess,sys,zlib
R=Path(__file__).resolve().parents[2];raw=Path(sys.argv[1]).read_bytes();O=Path(sys.argv[2]).resolve();O.mkdir(parents=True,exist_ok=True)
assert len(raw)==4560
def packet(op,head,data):return (bytes([op,8])+bytes(head)+data).ljust(32,b'\0')
legacy=[raw[:32]];pos=64
for l in range(16):
 for s in range(0,62,13):
  n=min(13,62-s);legacy.append(packet(226,[l,s,n],raw[pos:pos+2*n]));pos+=2*n
for l in range(16):
 for s in range(0,62,27):
  n=min(27,62-s);legacy.append(packet(231,[l,s,n],raw[pos:pos+n]));pos+=n
for l in range(16):
 for s in range(4):
  n=raw[pos+1];legacy.append(packet(233,[l,s],raw[pos:pos+6+n]));pos+=6+n
for l in range(16):
 n=raw[pos];legacy.append(packet(228,[l],raw[pos:pos+1+n]));pos+=1+n
legacy.append(raw[32:64]);assert pos==len(raw) and len(legacy)==210
(O/'legacy.bin').write_bytes(b''.join(legacy))
def compact(encoded,codec,mode=1):
 crc=zlib.crc32(encoded);n=(len(encoded)+23)//24
 return [bytes([236,8,mode,codec])+struct.pack('<IHHHHII',123,len(encoded),len(raw),n,0,zlib.crc32(raw),crc)+bytes(8)]+[bytes([237,8])+struct.pack('<IH',123,s)+encoded[s*24:s*24+24].ljust(24,b'\0') for s in range(n)]+[bytes([238,8])+struct.pack('<IHI',123,n,crc)+bytes(20)]
def lz(data):
 out=bytearray();literal=bytearray();seen={};i=0
 def flush():
  while literal:
   n=min(128,len(literal));out.append(n-1);out.extend(literal[:n]);del literal[:n]
 while i<len(data):
  best=0;dist=0
  for j in seen.get(data[i:i+3],[])[-64:]:
   if i-j>4096:continue
   n=0
   while n<130 and i+n<len(data) and data[j+n]==data[i+n]:n+=1
   if n>best:best=n;dist=i-j
  count=best if best>=3 else 1
  if best>=3:flush();out.append(128+best-3);out.extend(struct.pack('<H',dist))
  else:literal.append(data[i]);
  for j in range(i,i+count):seen.setdefault(data[j:j+3],[]).append(j)
  i+=count
  if len(literal)==128:flush()
 flush();return bytes(out)
encoded=lz(raw);assert len(encoded)<len(raw)
subprocess.run([sys.executable,str(R/'tests/ergohaven/run_compact_usb.py'),str(O/'build')],check=True)
exe=O/'build/compact_receiver';results=[]
def run(name,frames,reject=False):
 path=O/(name+'.bin');path.write_bytes(b''.join(frames));p=subprocess.run([str(exe),str(O/'legacy.bin'),str(path)]+(['reject'] if reject else []),capture_output=True,text=True)
 results.append({'name':name,'exit':p.returncode,'output':p.stdout+p.stderr});assert p.returncode==0,results[-1]
run('RAW',compact(raw,0));run('LZ',compact(encoded,1))
frames=compact(raw,0);run('partial',frames[:-2],True)
frames=compact(raw,0);run('out_of_order',[frames[0],frames[2],frames[1],*frames[3:]],True)
frames=compact(raw,0);run('exact_previous_retry',[*frames[:2],frames[1],*frames[2:]])
frames=compact(raw,0);bad=bytearray(frames[1]);bad[8]^=1;run('conflicting_retry',[*frames[:2],bytes(bad),*frames[2:]],True)
frames=compact(raw,0);bad=bytearray(frames[-1]);bad[8]^=1;run('bad_end_crc',[*frames[:-1],bytes(bad)],True)
frames=compact(raw,0);bad=bytearray(frames[1]);bad[2]^=1;run('stale_transaction',[frames[0],bytes(bad),*frames[2:]],True)
frames=compact(raw,0);bad=bytearray(frames[0]);bad[31]=1;run('begin_padding',[bytes(bad),*frames[1:]],True)
for name,data in [('zero_distance',b'\x80\0\0'),('beyond_dictionary',b'\x80\x01\0'),('literal_truncation',b'\x7f\0'),('match_truncation',b'\x80\x01'),('decoded_short',b'\0x')]:run(name,compact(data,1),True)
(O/'results.json').write_text(json.dumps({'raw_bytes':len(raw),'lz_bytes':len(encoded),'cases':results},indent=2)+'\n')
print(json.dumps({'pass':True,'cases':len(results),'raw_bytes':len(raw),'lz_bytes':len(encoded)}))
