/* Actual production parser/journal above; only flash boundary replaced. */
static uint8_t disk[4][8192];
static int budget=-1, tear=-1;
static unsigned writes;
const uint8_t *sm62_cache_flash_record(unsigned r) {assert(r<4);return disk[r];}
bool sm62_cache_flash_erase(unsigned r) {
 assert(r<4);if(budget==0)return false;if(budget>0)budget--;writes++;
 memset(disk[r],255,8192);return true;
}
bool sm62_cache_flash_program(unsigned r,unsigned off,const uint8_t *page) {
 assert(r<4 && off%256==0 && off<=8192-256);
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
static void upload(void) {
 uint32_t crc=app_compact_crc(raw,raw_size);uint8_t p[32]={0xec,8,1,0,123};
 p[8]=p[10]=raw_size;p[9]=p[11]=raw_size>>8;unsigned n=(raw_size+23)/24;p[12]=n;p[13]=n>>8;
 cache_put32(p+16,crc);cache_put32(p+20,crc);receive(p);
 for(unsigned s=0;s<n;s++) {memset(p,0,32);p[0]=0xed;p[1]=8;p[2]=123;p[6]=s;p[7]=s>>8;
 unsigned count=raw_size-24*s;if(count>24)count=24;memcpy(p+8,raw+s*24,count);receive(p);}
 memset(p,0,32);p[0]=0xee;p[1]=8;p[2]=123;p[6]=n;p[7]=n>>8;cache_put32(p+8,crc);receive(p);
}
static bool command(unsigned cmd,unsigned slot,unsigned identity,uint32_t crc) {
 uint8_t p[32]={0xef,8};p[2]=cmd;p[3]=slot;cache_put32(p+4,0x98765432);memset(p+8,identity,20);cache_put32(p+28,crc);
 unsigned before=replies;receive(p);assert(replies==before+1);assert(response[0]==0xef&&response[1]==8&&response[2]==(cmd|128));
 assert(!memcmp(response+4,p+4,28));assert(response[3]<=1);return response[3]==1;
}
int main(void) {
 memset(disk,255,sizeof(disk));
 uint8_t cap[32]={0xeb,8,0xa5,1};receive(cap);assert(response[2]==0x5a&&response[8]==3&&response[9]==3);
 puts("PASS exactly-three capability through production VIA tail");
 uint32_t crc[4];
 for(unsigned s=0;s<3;s++) {fixture(s+1,4+s,true);upload();assert(app_compact_last_valid);crc[s]=app_compact_crc(raw,raw_size);assert(command(1,s,s+1,crc[s]));}
 assert(!command(1,3,3,crc[2]));assert(!command(2,3,3,crc[2]));assert(!command(2,0,2,crc[0]));assert(!command(2,0,1,crc[0]^1));
 puts("PASS STORE positive ACK; three slots only; ID/CRC miss rejection");
 unsigned w=writes;
 for(unsigned s=0;s<3;s++) {assert(command(0,s,s+1,crc[s]));assert(command(2,s,s+1,crc[s]));assert(app_layout_keycodes[0][0]==4+s);assert(app_layout_revision==s+1);}
 assert(writes==w);puts("PASS SELECT positive ACK and replay; zero flash writes on selection");
 assert(command(1,2,3,crc[2]));assert(writes==w);
 vial_unlock_in_progress=true;uint8_t locked[32]={0xef,8,2,0};receive(locked);assert(!memcmp(response,locked,32));vial_unlock_in_progress=false;
 uint8_t gate[32]={0xef,8,1};assert(!hid_app_layout_packet_safe_during_refresh(gate,32));gate[2]=2;assert(hid_app_layout_packet_safe_during_refresh(gate,32));gate[2]=0;assert(hid_app_layout_packet_safe_during_refresh(gate,32));gate[1]=7;assert(!hid_app_layout_packet_safe_during_refresh(gate,32));
 puts("PASS idempotent store, outer unlock, render STORE deferred / SELECT admitted");
 /* Select same content preserves a held press; changed content releases it. */
 assert(command(2,0,1,crc[0]));assert(hid_app_layout_process_keyevent(0,5,true));unsigned u=ups;
 assert(command(2,0,1,crc[0]));assert(ups==u);
 assert(command(2,1,2,crc[1]));assert(ups==u+1);
 now+=2500;hid_app_layout_task();assert(!app_layout_active);assert(command(2,0,1,crc[0]));assert(app_layout_active);
 fixture(4,0xffff,false);upload();crc[3]=app_compact_crc(raw,raw_size);assert(command(1,0,4,crc[3]));assert(command(2,0,4,crc[3]));assert(!app_layout_active);
 assert(command(2,1,2,crc[1]));app_compact_reset();memset(&app_layout_staging,0,sizeof(app_layout_staging));app_layout_active=false;
 assert(command(2,2,3,crc[2]));assert(app_layout_active);
 puts("PASS held release, lease recovery, inactive native Default, RAM-reset reboot selection");
 /* Preserve full journal; update slot0 with power failure before every IO and
  * after every byte of every page (plus erase-before-program boundaries). */
 static uint8_t saved[4][8192];memcpy(saved,disk,sizeof(disk));
 fixture_max(5);upload();assert(app_compact_last_valid);uint32_t next=app_compact_crc(raw,raw_size);
 unsigned cuts=0;
 for(int b=0;b<22;b++)for(int t=-1;t<256;t++) {
   memcpy(disk,saved,sizeof(disk));budget=b;tear=t;
   bool ok=command(1,0,5,next);budget=-1;tear=-1;
   bool old=command(0,0,4,crc[3]), fresh=command(0,0,5,next);
   assert(old||fresh);if(ok)assert(fresh);assert(command(0,1,2,crc[1]));assert(command(0,2,3,crc[2]));cuts++;
 }
 printf("PASS production journal atomic powercuts=%u (each page byte + IO boundary)\n",cuts);
 memcpy(disk,saved,sizeof(disk));budget=-1;assert(command(1,0,5,next));
 int latest[3];uint32_t gen;cache_scan(latest,&gen);int newest=latest[0];assert(newest>=0);
 static uint8_t complete[4][8192];memcpy(complete,disk,sizeof(disk));unsigned corruptions=0;
 /* Every metadata/CRC byte, commit marker byte, and actual payload byte. */
 for(unsigned off=0;off<8192;off++)if(off<44 || (off>=256&&off<256+raw_size) || (off>=7936&&off<7940)) {
   memcpy(disk,complete,sizeof(disk));disk[newest][off]^=1;
   assert(!command(0,0,5,next));assert(command(0,0,4,crc[3]));assert(command(0,1,2,crc[1]));assert(command(0,2,3,crc[2]));corruptions++;
 }
 printf("PASS production journal corruptions=%u reject new / retain previous\n",corruptions);
 memcpy(disk,complete,sizeof(disk));
 fixture(6,12,true);raw[39]^=1;upload();assert(!app_compact_last_valid);assert(!command(1,0,6,app_compact_crc(raw,raw_size)));
 puts("PASS compact CRC16-rejected COMMIT cannot STORE");
 puts("ALL RESIDENT THREE BEHAVIOR PASS");return 0;
}
