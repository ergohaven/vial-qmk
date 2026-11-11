#include <stdlib.h>
/* Actual production parser/journal above; only flash boundary replaced. */
static uint8_t disk[101][8192];
static int budget=-1, tear=-1;
static unsigned writes;
const uint8_t *sm62_cache_flash_record(unsigned r) {return r<101?disk[r]:NULL;}
bool sm62_cache_flash_erase(unsigned r) {
 assert(r<101);if(budget==0)return false;if(budget>0)budget--;writes++;
 memset(disk[r],255,8192);return true;
}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t *page) {
 assert(r<101 && off%256==0 && off<=8192-256);
 if(budget==0) {if(tear>=0)for(int i=0;i<tear;i++)disk[r][off+i]&=page[i];return false;}
 if(budget>0) {budget--;}
 writes++;for(unsigned i=0;i<256;i++)disk[r][off+i]&=page[i];return true;
}
static void receive(const uint8_t *in) {uint64_t aligned[4];memcpy(aligned,in,32);raw_hid_receive((uint8_t*)aligned,32);}
static uint8_t raw[4560];static unsigned raw_size;
static void fixture(unsigned identity,uint16_t key,bool active) {
 memset(raw,0,sizeof(raw));raw[0]=0xe1;raw[1]=8;raw[2]=active;cache_put32(raw+3,identity);raw[7]=62;raw[8]=16;
 raw[32]=0xe3;raw[33]=8;raw[34]=active;cache_put32(raw+35,identity);raw[41]=0x57;raw[42]=2;
 memset(raw+43,identity,20);
 memset(&app_layout_staging,0,sizeof(app_layout_staging));
 for(unsigned l=0;l<16;l++)for(unsigned k=0;k<62;k++) {
   uint16_t code=k==0?key:0xffff;app_layout_staging.keycodes[l][k]=code;
   unsigned o=64+2*(l*62+k);raw[o]=code;raw[o+1]=code>>8;
 }
 uint16_t crc=app_layout_crc16(&app_layout_staging);raw[39]=crc;raw[40]=crc>>8;
 raw_size=3440;
}
static void fixture_max(unsigned identity) {
 fixture(identity,9,true);unsigned pos=64+1984+992;
 for(unsigned l=0;l<16;l++)for(unsigned s=0;s<4;s++) {
   raw[pos]=0;raw[pos+1]=12;memset(raw+pos+6,'a'+s,12);pos+=18;
   memset(app_layout_staging.stack_names[l][s],'a'+s,12);
 }
 for(unsigned l=0;l<16;l++) {
   raw[pos++]=22;memset(raw+pos,'A'+l,22);pos+=22;
   app_layout_staging.layer_name_lengths[l]=22;memset(app_layout_staging.layer_names[l],'A'+l,22);
 }
 assert(pos==4560);raw_size=pos;
 uint16_t crc=app_layout_crc16(&app_layout_staging);raw[39]=crc;raw[40]=crc>>8;
}
void upload(void) {
 uint32_t crc=app_compact_crc(raw,raw_size);uint8_t p[32]={0xec,8,2,0,123};
 p[8]=p[10]=raw_size;p[9]=p[11]=raw_size>>8;unsigned n=(raw_size+23)/24;p[12]=n;p[13]=n>>8;
 cache_put32(p+16,crc);cache_put32(p+20,crc);receive(p);
 for(unsigned s=0;s<n;s++) {memset(p,0,32);p[0]=0xed;p[1]=8;p[2]=123;p[6]=s;p[7]=s>>8;
 unsigned count=raw_size-24*s;if(count>24)count=24;memcpy(p+8,raw+s*24,count);receive(p);}
 memset(p,0,32);p[0]=0xee;p[1]=8;p[2]=123;p[6]=n;p[7]=n>>8;cache_put32(p+8,crc);receive(p);
}
static unsigned command(unsigned op,unsigned slot,unsigned identity,uint32_t crc,uint32_t token) {
 uint8_t p[32]={0xef,9};p[2]=op;p[3]=slot;cache_put32(p+4,token);memset(p+8,identity,20);cache_put32(p+28,crc);
 unsigned before=replies;receive(p);assert(replies==before+1);assert(response[0]==0xef&&response[1]==9&&response[2]==(op|128));
 assert(!memcmp(response+4,p+4,4));if(op!=3)assert(!memcmp(response+8,p+8,24));return response[3];
}
static unsigned live_snapshot(uint8_t *out) {
 uint8_t *start=out;
#define SAVE(x) memcpy(out,&(x),sizeof(x));out+=sizeof(x)
 SAVE(app_layout_active);SAVE(app_layout_revision);SAVE(app_layout_sync_time);SAVE(app_layout_name);SAVE(app_layout_layer_names);
 SAVE(app_layout_keycodes);SAVE(app_layout_visuals);SAVE(app_layout_stack_counts);SAVE(app_layout_stack_selected);SAVE(app_layout_stack_keycodes);SAVE(app_layout_stack_names);
 SAVE(app_layout_execution_states);SAVE(app_layout_visual_generation);SAVE(app_layout_wake_epoch);SAVE(app_layout_wake_epoch_seen);SAVE(app_layout_wake_identity);SAVE(app_layout_wake_identity_seen);
 SAVE(app_layout_held);SAVE(app_layout_events);SAVE(app_layout_event_head);SAVE(app_layout_event_count);SAVE(app_layout_event_sequence);SAVE(layer_state);SAVE(default_layer_state);SAVE(downs);SAVE(ups);SAVE(wakes);
#undef SAVE
 return out-start;
}
static void inactive_suite(void) {
 fixture_max(2);uint32_t crc=app_compact_crc(raw,raw_size);assert(command(2,1,2,crc,1010)==0);
 layer_state=1u<<4;assert(hid_app_layout_process_keyevent(0,5,true));unsigned u=ups;
 uint8_t state[10000],next[10000];unsigned n=live_snapshot(state);assert(n<sizeof(state));
 fixture_max(1);upload();assert(command(1,0,1,app_compact_crc(raw,raw_size),123)==0);
 assert(live_snapshot(next)==n&&!memcmp(state,next,n));assert(ups==u);
 assert(hid_app_layout_process_keyevent(0,5,false)&&ups==u+1&&up_code==9);
 uint8_t off[32]={0xea,8};receive(off);assert(!app_layout_active&&!hid_app_layout_process_keyevent(0,5,true));
 puts("PASS byte-identical live map/revision/layer/held/events/display/lease/wake state across background stage+STORE; native Default exit");
}
static void boundary_suite(void) {
 fixture_max(2);upload();uint32_t crc=app_compact_crc(raw,raw_size);
 assert(command(5,1,2,crc,123)==0);now+=2499;assert(command(5,1,2,crc,123)==0);
 now++;assert(command(5,1,2,crc,123)==3 && command(1,1,2,crc,123)==3);
 upload();assert(command(5,1,3,crc,123)==3 && command(5,1,2,crc^1,123)==3);
 uint8_t begin[32]={0xe1,8,1,1,0,0,0,62,16};receive(begin);assert(command(5,1,2,crc,123)==3);
 upload();uint8_t invalid[32]={0xed,8,124};receive(invalid);assert(command(5,1,2,crc,123)==3);
 upload();uint8_t off[32]={0xea,8};receive(off);assert(!app_layout_active&&command(5,1,2,crc,123)==3);
 upload();unsigned g=app_layout_visual_generation;uint8_t keep[32]={0xe5,8};receive(keep);assert(command(5,1,2,crc,123)==0&&app_layout_visual_generation==g);
 uint8_t rd[32]={0xef,9,6,1,11,22,33,44};rd[8]=4560&255;rd[9]=4560>>8;rd[10]=20;receive(rd);
 assert(response[10]==0&&response[11]==0&&app_compact_zero(response+12,20));
 rd[8]++;receive(rd);assert(response[11]==2&&response[10]==0);
 rd[8]=rd[9]=0;rd[10]=0;receive(rd);assert(response[11]==2);
 rd[10]=21;receive(rd);assert(response[11]==2);
 rd[10]=1;rd[11]=1;receive(rd);assert(response[11]==2);rd[11]=0;rd[31]=1;receive(rd);assert(response[11]==2);
 assert(command(3,1,1,0,123)==2 && command(7,1,0,0,123)==2 && command(0,255,2,crc,123)==2);
 uint8_t cap[32]={0xeb,8,0xa5,1,4,3,2,1};receive(cap);assert(response[2]==0x5a&&response[8]==3&&response[9]==0);
 vial_unlock_in_progress=true;uint8_t locked[32]={0xef,9,2,1};unsigned before=replies;receive(locked);assert(replies==before+1&&!memcmp(response,locked,32));vial_unlock_in_progress=false;
 fixture_max(2);raw[39]^=1;upload();assert(!app_compact_last_valid);assert(command(5,1,2,app_compact_crc(raw,raw_size),123)==3);
 fixture_max(2);raw[64+1984+992]=1;upload();assert(!app_compact_last_valid);
 puts("PASS ready TTL2499/2500, identity/CRC/nonce, invalid transaction, begin/deactivation, READ bounds/padding, unlock and malformed semantic FULL");
 int corrupt=cache_current(1);assert(corrupt>=0);uint8_t lo=disk[corrupt][12],hi=disk[corrupt][13];disk[corrupt][12]=disk[corrupt][13]=255;
 memset(rd,0,32);rd[0]=0xef;rd[1]=9;rd[2]=6;rd[3]=1;rd[10]=20;receive(rd);assert(response[11]==7);
 assert(command(0,1,2,crc,1336)==7 && cache_match(1,raw+43,crc)<0);
 assert(command(2,1,2,crc,1337)==7);disk[corrupt][12]=lo;disk[corrupt][13]=hi;
 cache_latest[1]=255;assert(cache_match(1,raw+43,crc)<0);cache_scanned=false;
}
/* Keep the scan/index hot while faulting only FULL bytes, not metadata.
 * Each case is independently executable so baseline failures are observable. */
static void integrity_suite(void) {
 const char *only=getenv("SM62_INTEGRITY_CASE");
 const char *cases[]={"query","enum","read","store","select","backend","exhausted","ef8","delete"};
 for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
  if(only && strcmp(only,cases[c])) continue;
  memset(disk,255,sizeof(disk));cache_scanned=false;app_compact_reset();
  fixture_max(1);uint32_t crc=app_compact_crc(raw,raw_size);upload();
  assert(command(1,0,1,crc,123)==0);
  fixture_max(100);uint32_t other=app_compact_crc(raw,raw_size);upload();
  assert(command(1,99,100,other,123)==0);
  int r=cache_current(0),unrelated=cache_current(99);assert(r>=0&&unrelated>=0);
  uint8_t saved[8192];memcpy(saved,disk[unrelated],sizeof(saved));
  fixture_max(1);upload();unsigned w=writes;
  assert(command(0,0,1,crc,1000)==0);
  assert(command(1,0,1,crc,123)==0&&writes==w);
  uint32_t generation=cache_generation;
  cache_generation=UINT32_MAX;
  assert(command(1,0,1,crc,123)==0&&writes==w);
  cache_generation=generation;
  uint8_t live[10000],next[10000];unsigned n=live_snapshot(live);
  disk[r][256+64]^=1;assert(cache_scanned&&!cache_record_valid(r));
  if(c==0) assert(command(0,0,1,crc,1001)==7);
  if(c==1) assert(command(3,0,0,0,1002)==7);
  if(c==2) {
   uint8_t rd[32]={0xef,9,6,0,1,2,3,4,64,0,20};receive(rd);
   assert(response[2]==0x86&&response[3]==0&&response[11]==7);
   assert(response[10]==0&&app_compact_zero(response+12,20));
  }
  if(c==3) {assert(command(5,0,1,crc,123)==0);assert(command(1,0,1,crc,123)==7);}
  if(c==4) assert(command(2,0,1,crc,1003)==7);
  if(c==5) assert(!cache_store(0,raw+43,crc,raw,raw_size));
  if(c==6) {
   cache_generation=UINT32_MAX;
   assert(command(1,0,1,crc,123)==7);
   assert(!cache_store(0,raw+43,crc,raw,raw_size));
   cache_generation=generation;
  }
  if(c==7) {
   uint8_t p[32]={0xef,8,0,0};memset(p+8,1,20);cache_put32(p+28,crc);receive(p);assert(response[3]==0);
  }
  if(c==8) assert(command(4,0,1,crc,1007)==7);
  assert(writes==w&&cache_generation==generation&&cache_current(0)==r);
  assert(live_snapshot(next)==n&&!memcmp(live,next,n));
  assert(!memcmp(saved,disk[unrelated],sizeof(saved)));
  assert(command(0,99,100,other,1004)==0);
  assert(command(3,99,0,0,1005)==0&&response[8]==100);
  /* Once the fault is removed, the same staged good FULL is a no-wear hit. */
  disk[r][256+64]^=1;
  if(c==4) upload();
  assert(command(1,0,1,crc,123)==0&&writes==w);
  assert(command(2,0,1,crc,1006)==0&&app_layout_keycodes[0][0]==9);
  assert(writes==w);
  printf("PASS post-scan payload integrity %s: explicit failure, healthy no-wear, unrelated slot/live state preserved\n",cases[c]);
 }
}
/* Ordinary wire transfer, not app_compact_expand: interleave real read packets. */
static void staging_probe(unsigned op,bool cold,uint32_t crc) {
 uint8_t before[sizeof(app_layout_staging)];memcpy(before,&app_layout_staging,sizeof(before));
 unsigned w=writes;
 if(op==6) {
  uint8_t p[32]={0xef,9,6,0,1,2,3,4,0,0,20};receive(p);
  assert(response[11]==(cold?3:0));
  assert(response[10]==(cold?0:20));
  if(!cold) assert(!memcmp(response+12,raw,20));
  else assert(app_compact_zero(response+12,20));
 } else if(op==8) {
  uint8_t p[32]={0xef,8,0,0};memset(p+8,1,20);cache_put32(p+28,crc);receive(p);
  assert(response[3]==(cold?0:1));
 } else {
  assert(command(op,0,op==3?0:1,op==3?0:crc,555)==(cold?3:0));
  if(op==3&&!cold) assert(response[8]==1&&app_layout_read_u32(response+28)==crc);
 }
 assert(!memcmp(before,&app_layout_staging,sizeof(before))&&writes==w);
}
static void staging_suite(void) {
 const char *only=getenv("SM62_STAGING_CASE");
 unsigned ops[]={0,3,6,8};
 for(unsigned cold=0;cold<2;cold++)for(unsigned o=0;o<4;o++)for(unsigned end=0;end<2;end++) {
  char name[64];snprintf(name,sizeof(name),"%s-%u-%s",cold?"cold":"warm",ops[o],end?"commit":"begin");
  if(only&&strcmp(only,name))continue;
  memset(disk,255,sizeof(disk));cache_scanned=false;app_compact_reset();
  fixture_max(1);uint32_t crc=app_compact_crc(raw,raw_size);upload();
  assert(command(1,0,1,crc,123)==0);assert(command(0,0,1,crc,444)==0);
  /* Read-only work also preserves an independently staged FULL's ready token. */
  if(cold)cache_scanned=false;
  staging_probe(ops[o],false,crc);assert(command(5,0,1,crc,123)==0);
  if(cold)cache_scanned=false;
  unsigned g=app_layout_visual_generation,w=writes;receive(raw);
  if(!end)staging_probe(ops[o],cold,crc);
  unsigned pos=64;uint8_t p[32];
  for(unsigned l=0;l<16;l++)for(unsigned s=0;s<62;s+=13){unsigned n=62-s;if(n>13)n=13;
   memset(p,0,32);p[0]=0xe2;p[1]=8;p[2]=l;p[3]=s;p[4]=n;memcpy(p+5,raw+pos,n*2);pos+=n*2;receive(p);}
  for(unsigned l=0;l<16;l++)for(unsigned s=0;s<62;s+=27){unsigned n=62-s;if(n>27)n=27;
   memset(p,0,32);p[0]=0xe7;p[1]=8;p[2]=l;p[3]=s;p[4]=n;memcpy(p+5,raw+pos,n);pos+=n;receive(p);}
  for(unsigned l=0;l<16;l++)for(unsigned s=0;s<4;s++){unsigned n=raw[pos+1];
   memset(p,0,32);p[0]=0xe9;p[1]=8;p[2]=l;p[3]=s;memcpy(p+4,raw+pos,6+n);pos+=6+n;receive(p);}
  for(unsigned l=0;l<16;l++){unsigned n=raw[pos];memset(p,0,32);p[0]=0xe4;p[1]=8;p[2]=l;memcpy(p+3,raw+pos,1+n);pos+=1+n;receive(p);}
  assert(pos==raw_size);if(end)staging_probe(ops[o],cold,crc);
  receive(raw+32);
  assert(app_layout_active&&app_layout_revision==1&&app_layout_keycodes[0][0]==9&&app_layout_visual_generation==g+1&&writes==w);
  assert(command(0,0,1,crc,556)==0&&cache_scanned&&writes==w);
  printf("PASS staging %s: response, byte-identical staging, ready token, ordinary publication, zero wear\n",name);
 }
}
static void fault_suite(void) {
 memset(disk,255,sizeof(disk));cache_scanned=false;app_compact_reset();
 uint32_t old[100];
 for(unsigned s=0;s<100;s++){fixture_max(s+1);old[s]=app_compact_crc(raw,raw_size);assert(cache_store(s,raw+43,old[s],raw,raw_size));}
 static uint8_t saved[101][8192],complete[101][8192];memcpy(saved,disk,sizeof(disk));
 fixture_max(100);raw[64]=12;app_layout_staging.keycodes[0][0]=12;
 uint16_t c=app_layout_crc16(&app_layout_staging);raw[39]=c;raw[40]=c>>8;
 uint32_t fresh=app_compact_crc(raw,raw_size);unsigned cuts=0;
 for(int b=0;b<22;b++)for(int t=-1;t<256;t++) {
  memcpy(disk,saved,sizeof(disk));cache_scanned=false;cache_init();budget=b;tear=t;
  bool ok=cache_store(99,raw+43,fresh,raw,raw_size);budget=-1;tear=-1;cache_scanned=false;
  bool a=cache_match(99,raw+43,old[99])>=0, z=cache_match(99,raw+43,fresh)>=0;
  assert(a||z);if(ok)assert(z);uint8_t id[20];memset(id,1,20);assert(cache_match(0,id,old[0])>=0);memset(id,99,20);assert(cache_match(98,id,old[98])>=0);cuts++;
 }
 printf("PASS exhaustive maximum FULL COW powercuts=%u (every page byte and IO boundary)\n",cuts);
 memcpy(disk,saved,sizeof(disk));cache_scanned=false;assert(cache_store(99,raw+43,fresh,raw,raw_size));
 int newest=cache_current(99);memcpy(complete,disk,sizeof(disk));unsigned corruptions=0;
 for(unsigned off=0;off<8192;off++)if(off<44 || (off>=256&&off<256+raw_size) || (off>=7936&&off<7952)) {
  memcpy(disk,complete,sizeof(disk));disk[newest][off]^=1;assert(!cache_record_valid(newest));
  if(off<44 || off%256==0 || off>=7936){cache_scanned=false;assert(cache_match(99,raw+43,old[99])>=0);assert(cache_match(99,raw+43,fresh)<0);}
  corruptions++;
 }
 printf("PASS every metadata/payload/commit corruption=%u; reboot fallback and unrelated slots preserved\n",corruptions);
 memcpy(disk,complete,sizeof(disk));cache_scanned=false;cache_init();
 unsigned w=writes;assert(cache_store(99,raw+43,fresh,raw,raw_size)&&writes==w);
 cache_generation=UINT32_MAX;assert(!cache_store(0,raw+43,fresh,raw,raw_size)&&writes==w);
 assert(command(4,99,100,fresh,909)==8&&writes==w);cache_scanned=false;
 assert(command(4,99,100,fresh,910)==0);cache_scanned=false;assert(cache_match(99,raw+43,fresh)<0);
 assert(command(3,99,0,0,911)==1);
 puts("PASS generation exhaustion/no-wear/tombstone reboot prevents resurrection");
 memcpy(disk,complete,sizeof(disk));cache_scanned=false;cache_init();memcpy(saved,disk,sizeof(disk));unsigned deletes=0;
 for(int b=0;b<4;b++)for(int t=-1;t<256;t++){
  memcpy(disk,saved,sizeof(disk));cache_scanned=false;cache_init();budget=b;tear=t;
  bool ok=cache_append(99,raw+43,fresh,0,0,true);budget=-1;tear=-1;cache_scanned=false;
  int r=cache_current(99);assert(r>=0);const uint8_t *p=sm62_cache_flash_record(r);assert(p[6]==2 || cache_match(99,raw+43,fresh)>=0);if(ok)assert(p[6]==2);deletes++;
 }
 printf("PASS tombstone powercuts=%u; committed delete or intact previous FULL\n",deletes);
 memcpy(disk,complete,sizeof(disk));cache_scanned=false;cache_init();newest=cache_current(99);
 /* Forge transport and record checksums around an invalid real semantic CRC16:
  * scan must still reject it through the sole production validator. */
 disk[newest][256+39]^=1;cache_put32(disk[newest]+16,app_compact_crc(disk[newest]+256,4560));
 cache_put32(disk[newest]+40,app_compact_crc(disk[newest],40));cache_put32(disk[newest]+7944,app_layout_read_u32(disk[newest]+40));
 cache_put32(disk[newest]+7948,app_compact_crc(disk[newest]+7936,12));
 assert(!cache_record_valid(newest));cache_scanned=false;assert(cache_match(99,raw+43,old[99])>=0);
 puts("PASS forged valid CRC32/header/commit cannot bypass actual semantic COMMIT validation");
}
int main(void) {
 if(getenv("SM62_STAGING_CASE")) {staging_suite();return 0;}
 if(getenv("SM62_INTEGRITY_CASE")) {integrity_suite();return 0;}
 memset(disk,255,sizeof(disk));
 uint8_t cap[32]={0xeb,8,0xa5,2,1,2,3,4};receive(cap);
 assert(response[2]==0x5a && response[3]==2 && response[8]==2 && response[9]==100);
 fixture_max(1);unsigned g=app_layout_visual_generation;upload();
 assert(app_compact_last_valid && app_layout_visual_generation==g && !app_layout_active);
 puts("PASS EBv2 and stage-only validates without live publication");
 uint32_t crcs[100];
 for(unsigned s=0;s<100;s++){
  fixture_max(s+1);upload();crcs[s]=app_compact_crc(raw,raw_size);
  if(getenv("SM62_FIXTURE_DIR")){char path[1024];assert(snprintf(path,sizeof(path),"%s/full%02u.bin",getenv("SM62_FIXTURE_DIR"),s)>0);FILE *f=fopen(path,"wb");assert(f&&fwrite(raw,1,raw_size,f)==raw_size&&!fclose(f));}
  assert(command(5,s,s+1,crcs[s],123)==0);
  assert(command(1,s,s+1,crcs[s],123)==0);
  assert(app_layout_visual_generation==g&&!app_layout_active);
 }
 for(unsigned s=0;s<100;s++)assert(command(0,s,s+1,crcs[s],400+s)==0);
 puts("PASS 100 distinct complete maximum FULL records including slots0/99 via EF09 STORE_READY+QUERY");
 unsigned w=writes;
 assert(command(1,99,100,crcs[99],123)==0 && writes==w);
 assert(command(1,99,100,crcs[99],124)==3);
 assert(command(2,99,100,crcs[99],800)==0);
 assert(app_layout_active && app_layout_revision==100 && app_layout_keycodes[0][0]==9);
 char name[32];assert(hid_app_layout_get_name(0,name,sizeof(name))&&!strcmp(name,"AAAAAAAAAAAAAAAAAAAAAA"));
 assert(hid_app_layout_process_keyevent(0,5,true));unsigned u=ups;
 fixture_max(1);upload();assert(ups==u&&app_layout_revision==100);assert(hid_app_layout_process_keyevent(0,5,false)&&ups==u+1);
 assert(command(1,100,1,crcs[0],123)==2 && writes==w);
 assert(command(1,99,1,crcs[0],123)==6 && writes==w);
 assert(command(2,0,1,crcs[0],801)==0 && writes==w);
 assert(command(5,0,1,crcs[0],123)==3);
 assert(command(3,0,0,0,802)==0 && response[8]==1 && app_layout_read_u32(response+28)==crcs[0]);
 uint8_t rd[32]={0xef,9,6,0,1,2,3,4,0,0,20};receive(rd);
 assert(response[2]==0x86&&response[3]==0&&response[10]==20&&response[11]==0&&!memcmp(response+12,disk[cache_current(0)]+256,20));
 assert(command(4,0,1,crcs[0],803)==0);cache_scanned=false;
 assert(command(0,0,1,crcs[0],804)==1 && command(3,0,0,0,805)==1);
 assert(command(0,99,100,crcs[99],806)==0);
 puts("PASS no-wear/token/conflict/overflow/SELECT keypress+name/nonactivation/READ/delete/recovery");
 fixture_max(1);raw[42]=1;upload();assert(!app_compact_last_valid);
 puts("PASS stage-only rejects non-semantic FULL");
 uint8_t gate[32]={0xef,9,0};
 for(unsigned op=0;op<=6;op++){gate[2]=op;assert(hid_app_layout_packet_safe_during_refresh(gate,32)==(op!=1&&op!=4));}
 gate[0]=0xeb;gate[1]=8;gate[2]=0xa5;gate[3]=2;assert(hid_app_layout_packet_safe_during_refresh(gate,32));
 uint8_t old[32]={0xef,8,2,1};memset(old+8,2,20);cache_put32(old+28,crcs[1]);receive(old);assert(response[3]==0);
 puts("PASS exact refresh allowlist; deprecated EF8 cannot replay application as Default");
 fixture_max(100);raw[64]=12;app_layout_staging.keycodes[0][0]=12;
 uint16_t c=app_layout_crc16(&app_layout_staging);raw[39]=c;raw[40]=c>>8;
 upload();uint32_t edited=app_compact_crc(raw,raw_size);
 budget=20;tear=16;assert(command(1,99,100,edited,123)==5);budget=-1;tear=-1;
 assert(command(0,99,100,edited,900)==0);
 puts("PASS uncertain complete commit is discovered on read-only reconciliation");
 fixture_max(1);raw[63]=1;upload();assert(!app_compact_last_valid);
 puts("PASS nonzero FULL padding rejected");
 inactive_suite();
 boundary_suite();
 if(!getenv("SM62_SKIP_FAULTS")) fault_suite();
 integrity_suite();
 staging_suite();
 return 0;}
