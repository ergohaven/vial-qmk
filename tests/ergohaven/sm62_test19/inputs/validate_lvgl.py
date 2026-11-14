import json, hashlib
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate_lvgl(lvgl):
 T=INPUTS;m=json.loads((T/'sm62-lvgl-blobs.json').read_text());ov=json.loads((T/'sm62-lvgl-cooperative-blobs.json').read_text());a=json.loads((INPUTS/'approved-overlay.json').read_text())
 assert m['revision']=='e19410f8f8a256609da72cff549598e0df6fa4cf' and len(m['blobs'])==606
 assert set(ov)=={'src/core/lv_refr.c','src/core/lv_refr.h','src/hal/lv_hal_disp.c','src/hal/lv_hal_disp.h','src/misc/lv_timer.c'}
 assert a['candidate_blob']=='e7ba614ada69d7ae8e7fd82ceeee7ef3dd941c53' and a['candidate_sha256']=='92eb59b8cdeb0aae6f808227431a287d874b4d38671804869abb338ee1313513'
 for p,oid in m['blobs'].items():
  if p in ov:assert ov[p]['base']==oid;oid=ov[p]['patched']
  if p==a['path']:assert oid==a['historical_overlay_blob'];oid=a['candidate_blob'];assert sha(lvgl/p)==a['candidate_sha256']
  data=(lvgl/p).read_bytes();assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==oid,p
 print('PASS exact pinned revision + 606 blobs + five base-checked overlays; only approved timer replacement',flush=True)
