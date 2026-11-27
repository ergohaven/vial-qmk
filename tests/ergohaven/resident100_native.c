#include <stdlib.h>
/* Current production RAM publisher and read-only archive; modeled NOR only. */
static uint8_t disk[101][8192];
static unsigned writes;
const uint8_t *sm62_cache_flash_record(unsigned r) {return r<101?disk[r]:NULL;}
/* Fail fast if a future parser tries to recover a removed mutator. */
bool sm62_cache_flash_erase(unsigned r) {(void)r;assert(!"read-only archive erase");return false;}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t *p) {(void)r;(void)off;(void)p;assert(!"read-only archive program");return false;}
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
static unsigned upload_mode=1;
void upload(void) {
 uint32_t crc=app_compact_crc(raw,raw_size);uint8_t p[32]={0xec,8,1,0,123};
 p[2]=upload_mode; p[8]=p[10]=raw_size;p[9]=p[11]=raw_size>>8;unsigned n=(raw_size+23)/24;p[12]=n;p[13]=n>>8;
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

static uint8_t saved[101][8192];
static void unchanged(void) {assert(!writes&&!memcmp(saved,disk,sizeof(disk)));}
static void inventory(unsigned status) {
 uint8_t p[32]={0xef,9,7,0,1,2,3,4};receive(p);
 assert(response[2]==0x87&&response[3]==status&&!memcmp(response+4,p+4,4));
 if(status) assert(app_compact_zero(response+8,24));
 else {assert(response[12]==100&&response[13]==13);for(unsigned s=0;s<100;s++)assert(response[14+s/8]&(1u<<(s%8)));assert(!(response[26]&0xf0));}
}
static void archive_read(unsigned slot) {
 fixture_max(slot+1);uint32_t crc=app_compact_crc(raw,raw_size);
 assert(command(0,slot,slot+1,crc,1234)==0);
 assert(command(3,slot,0,0,1235)==0&&response[8]==slot+1&&app_layout_read_u32(response+28)==crc);
 assert(command(0,slot,slot+2,crc,1236)==1&&command(0,slot,slot+1,crc^1,1237)==1);
 for(unsigned off=0;off<=raw_size;off+=20) {
  uint8_t rd[32]={0xef,9,6,slot,1,2,3,4};rd[8]=off;rd[9]=off>>8;rd[10]=20;receive(rd);
  unsigned n=raw_size-off;if(n>20)n=20;
  assert(response[2]==0x86&&response[3]==slot&&response[11]==0&&response[10]==n);
  assert(!memcmp(response+12,raw+off,n)&&app_compact_zero(response+12+n,20-n));
 }
}
static void rejected_mutations(unsigned slot) {
 uint8_t before[10000],after[10000];unsigned n=live_snapshot(before);
 uint32_t crc=app_compact_crc(raw,raw_size);
 for(unsigned op=1;op<=5;op++)if(op!=3)assert(command(op,slot,slot+1,crc,123)==2);
 for(unsigned op=0;op<=2;op++) {
  uint8_t q[32]={0xef,8,op,slot};memcpy(q+8,raw+43,20);cache_put32(q+28,crc);receive(q);
  assert(response[2]==(op|128)&&response[3]==(op==0));
 }
 assert(live_snapshot(after)==n&&!memcmp(before,after,n));unchanged();
}
static void integrity_suite(void) {
 int r=cache_current(0);assert(r>=0);
 uint8_t live[10000],after[10000];unsigned n=live_snapshot(live);
 fixture_max(1);uint32_t crc=app_compact_crc(raw,raw_size);
 /* Every metadata/payload/commit byte remains covered on the current reader. */
 unsigned corruptions=0;
 for(unsigned off=0;off<8192;off++)if(off<44||(off>=256&&off<256+4560)||(off>=7936&&off<7952)) {
  disk[r][off]^=1;assert(!cache_record_valid(r));disk[r][off]^=1;corruptions++;
 }
 disk[r][320]^=1;
 assert(command(0,0,1,crc,1)==7&&command(3,0,0,0,2)==7);
 uint8_t rd[32]={0xef,9,6,0,1,2,3,4,0,0,20};receive(rd);
 assert(response[11]==7&&response[10]==0&&app_compact_zero(response+12,20));inventory(7);
 /* Healthy unrelated records survive hot-index corruption. */
 archive_read(99);disk[r][320]^=1;
 /* A forged transport/header/commit CRC must not bypass semantic CRC16. */
 disk[r][256+39]^=1;cache_put32(disk[r]+16,app_compact_crc(disk[r]+256,4560));
 cache_put32(disk[r]+40,app_compact_crc(disk[r],40));cache_put32(disk[r]+7944,app_layout_read_u32(disk[r]+40));
 cache_put32(disk[r]+7948,app_compact_crc(disk[r]+7936,12));
 assert(!cache_record_valid(r));cache_scanned=false;assert(cache_current(0)<0);
 assert(command(3,0,0,0,3)==1);archive_read(99);
 memcpy(disk,saved,sizeof(disk));cache_scanned=false;cache_init();
 assert(live_snapshot(after)==n&&!memcmp(live,after,n));unchanged();
 printf("PASS current read-only integrity metadata/payload/commit corruptions=%u; forged semantic CRC, unrelated slot and live state preserved\n",corruptions);
}
static void staging_suite(void) {
 fixture_max(1);upload();
 for(unsigned cold=0;cold<2;cold++)for(unsigned op=0;op<4;op++) {
  uint8_t begin[32]={0xe1,8,1,77,0,0,0,62,16};receive(begin);
  uint8_t before[sizeof(app_layout_staging)];memcpy(before,&app_layout_staging,sizeof(before));
  if(cold)cache_scanned=false;
  unsigned status=cold?3:0;
  if(op==0)assert(command(0,0,1,app_compact_crc(raw,raw_size),55)==status);
  if(op==1)assert(command(3,0,0,0,56)==status);
  if(op==2){uint8_t rd[32]={0xef,9,6,0,1,2,3,4,0,0,20};receive(rd);assert(response[11]==status&&response[10]==(cold?0:20));}
  if(op==3)inventory(status);
  assert(!memcmp(before,&app_layout_staging,sizeof(before)));unchanged();
  uint8_t off[32]={0xea,8};receive(off);cache_init();assert(cache_scanned);
 }
 puts("PASS warm/cold QUERY/ENUM/READ/INVENTORY preserve ordinary staging; cold scan defers");
}
int main(void) {
 const char *path=getenv("SM62_ARCHIVE_IMAGE");assert(path);
 FILE*f=fopen(path,"rb");assert(f&&fread(disk,1,sizeof(disk),f)==sizeof(disk)&&fgetc(f)==EOF&&!fclose(f));
 memcpy(saved,disk,sizeof(disk));
 uint8_t cap[32]={0xeb,8,0xa5,2,1,2,3,4};receive(cap);assert(!memcmp(cap,response,32));
 cap[3]=3;receive(cap);assert(response[2]==0x5a&&response[8]==3&&!response[9]&&response[24]==2&&response[25]==100&&response[26]==1&&app_layout_read_u32(response+20)==1);
 assert(!app_layout_active);inventory(0);assert(!app_layout_active);
 for(unsigned s=0;s<100;s++)archive_read(s);
 assert(!app_layout_active);unchanged();
 puts("PASS current EBv2 unsupported echo; EBv3 host-managed RAM + read-only archive; 100 exact historical FULL exports, no autoactivation");
 for(unsigned s=0;s<100;s++) {
  fixture_max(s+1);upload();
  if(getenv("SM62_FIXTURE_DIR")){char path[1024];assert(snprintf(path,sizeof(path),"%s/full%02u.bin",getenv("SM62_FIXTURE_DIR"),s)>0);FILE *f=fopen(path,"wb");assert(f&&fwrite(raw,1,raw_size,f)==raw_size&&!fclose(f));}
  assert(app_layout_active&&app_layout_revision==s+1&&app_layout_keycodes[0][0]==9);
  rejected_mutations(s);
 }
 assert(hid_app_layout_process_keyevent(0,5,true));unsigned u=ups;
 fixture_max(1);upload();assert(ups==u&&hid_app_layout_process_keyevent(0,5,false)&&ups==u+1&&up_code==9);
 unsigned g=app_layout_visual_generation;
 upload_mode=2;upload();assert(app_layout_visual_generation==g);upload_mode=1;
 fixture_max(2);raw[39]^=1;upload();assert(app_layout_visual_generation==g);
 fixture_max(2);raw[63]=1;upload();assert(app_layout_visual_generation==g);
 fixture_max(2);raw[3040]=1;upload();assert(app_layout_visual_generation==g);
 fixture_max(2);raw[64]=QK_BOOT&255;raw[65]=QK_BOOT>>8;app_layout_staging.keycodes[0][0]=QK_BOOT;
 uint16_t c=app_layout_crc16(&app_layout_staging);raw[39]=c;raw[40]=c>>8;
 upload();assert(app_layout_visual_generation==g&&!vial_unlocked);
 vial_unlocked=true;upload();assert(app_layout_visual_generation==g+1&&app_layout_keycodes[0][0]==QK_BOOT);vial_unlocked=false;
 fixture_max(1);upload();unchanged();
 puts("PASS current 100 RAM publications; removed READY/STORE/SELECT/DELETE and mode2 refuse; malformed/semantic/locked QK_BOOT atomic; held release intact");
 integrity_suite();staging_suite();
 fixture_max(1);upload();now+=2499;hid_app_layout_task();assert(app_layout_active);now++;hid_app_layout_task();assert(!app_layout_active);unchanged();
 puts("PASS current lease 2499/2500; native fallback and zero archive writes");
 return 0;
}
