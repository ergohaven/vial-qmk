/* Native fixture: production code is included by test_macro_storage.py.
 * Only OS/HID/MMIO/NOR boundaries and unrelated dispatcher branches are stubs. */
static uint8_t before[SM62_MACRO_SIZE],after[SM62_MACRO_SIZE],got[SM62_MACRO_SIZE];
static uint8_t saved[2*SM62_MACRO_BANK_SIZE];
static unsigned campaigns,cuts_checked,faults_checked;
static void reboot_ram(void){
 reading=false;read_snapshot=0;last_ok=true;
 depth=0;failed=false;owned_token=0;core1_prepared=0;memset(&gate,0,sizeof(gate));
 irq_depth=0;cut=-1;fail_call=-1;silent_call=-1;read_fail=-1;read_error_at=-1;context_ok=true;bank_reads=0;
 events=0;io_calls=0;sent=0;vial_unlocked=true;vial_unlock_in_progress=false;
}
static void fresh(void){
 memset((void*)0x10000000,0xa5,4194304);
 memset((void*)(0x10000000+SM62_MACRO_FLASH_BASE),255,sizeof(saved));
 for(unsigned i=0;i<8192;i++)eeprom[i]=(i*13+7)&255;
 memcpy(original,eeprom,sizeof(eeprom));reboot_ram();
}
static uint8_t request(uint8_t cmd,unsigned off,unsigned n,const uint8_t*p){
 uint64_t aligned[4]={0};uint8_t *q=(uint8_t*)aligned;
 q[0]=cmd;q[1]=off>>8;q[2]=off;q[3]=n;if(p && n<=28)memcpy(q+4,p,n);
 unsigned was=sent;raw_hid_receive(q,32);assert(sent==was+1);
 return reply[0];
}
static void read_all(uint8_t *p){
 for(unsigned off=0;off<SM62_MACRO_SIZE;off+=28){
  unsigned n=SM62_MACRO_SIZE-off>28?28:SM62_MACRO_SIZE-off;
  assert(request(id_dynamic_keymap_macro_get_buffer,off,n,0)==id_dynamic_keymap_macro_get_buffer);
  memcpy(p+off,reply+4,n);
 }
}
static void assert_preserved(void){
 assert(!memcmp(eeprom,original,sizeof(eeprom)));
 const uint8_t *flash=(const void*)0x10000000;
 for(unsigned i=0;i<SM62_MACRO_FLASH_BASE;i++)assert(flash[i]==0xa5);
 for(unsigned i=SM62_MACRO_FLASH_BASE+sizeof(saved);i<4194304;i++)assert(flash[i]==0xa5);
}
static void campaign(unsigned off,unsigned n,const uint8_t*p,bool reset){
 /* Enumerate EVERY byte boundary in the REAL two sector erases and all 26
  * real page programs, not just one cut per page or a copied journal model. */
 memcpy(saved,(const void*)(0x10000000+SM62_MACRO_FLASH_BASE),sizeof(saved));
 assert(sm62_macro_read(0,SM62_MACRO_SIZE,before));memcpy(after,before,sizeof(after));
 if(reset)memset(after,0,sizeof(after));else memcpy(after+off,p,n);
 reboot_ram();assert(sm62_flash_begin());
 assert(reset?sm62_macro_reset():sm62_macro_update(off,n,p));assert(sm62_flash_end());
 unsigned total=events; // events counts only with cut enabled; calibrate below
 memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));
 reboot_ram();cut=1000000;
 assert(reset?sm62_macro_reset():sm62_macro_update(off,n,p));total=events;assert(total==8192+26*256);
 for(unsigned c=0;c<=total;c++){
  memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));reboot_ram();cut=(int)c;
  if(!setjmp(powercut)){
   assert(sm62_flash_begin());
   bool ok=reset?sm62_macro_reset():sm62_macro_update(off,n,p);
   assert(ok);assert(sm62_flash_end());
  }
  reboot_ram();assert(sm62_macro_read(0,SM62_MACRO_SIZE,got));
  assert(!memcmp(got,before,sizeof(got)) || !memcmp(got,after,sizeof(got)));
  assert(!memcmp(eeprom,original,sizeof(eeprom)));cuts_checked++;
 }
 campaigns++;assert_preserved();
}
static void storage_campaigns(void){
 fresh();uint8_t patch[28];for(unsigned i=0;i<28;i++)patch[i]=(i*19+31)&255;
 campaign(5210,28,patch,false); // first migration: crosses legacy/suffix boundary
 patch[0]^=0x5a;campaign(255,28,patch,false); // 2 committed banks, unaligned pages
 campaign(0,0,0,true); // full-pool reset, reboot never sees a prefix reset
 printf("PASS powercut: %u campaigns, %u every-byte erase/program boundaries, old-or-new only\n",campaigns,cuts_checked);
}
static void public_lifecycle(void){
 fresh();assert(!macro_hw_bank(2));
 assert(!macro_hw_erase(2));assert(!macro_hw_program(2,0,after));
 assert(!macro_hw_program(0,1,after));assert(!macro_hw_program(0,8192,after));assert(io_calls==0);
 fresh();assert(request(id_dynamic_keymap_macro_get_count,0,0,0)==id_dynamic_keymap_macro_get_count);assert(reply[1]==109);
 assert(request(id_dynamic_keymap_macro_get_buffer_size,0,0,0)==id_dynamic_keymap_macro_get_buffer_size);assert(((unsigned)reply[1]*256+reply[2])==6066);
 read_all(got);assert(!memcmp(got,eeprom+2968,5224));for(unsigned i=5224;i<6066;i++)assert(got[i]==0);assert(io_calls==0);
 for(unsigned i=0;i<5957;i++)after[i]='a'+i%26;
 memset(after+5957,0,109);
 for(unsigned off=0;off<6066;off+=28){
  unsigned n=6066-off>28?28:6066-off;
  assert(request(id_dynamic_keymap_macro_set_buffer,off,n,after+off)==id_dynamic_keymap_macro_set_buffer);
  assert(request(id_dynamic_keymap_macro_get_buffer,off,n,0)==id_dynamic_keymap_macro_get_buffer);assert(!memcmp(after+off,reply+4,n));
 }
 reboot_ram();read_all(got);assert(!memcmp(after,got,sizeof(got)));
 uint8_t sentinel=0x7b;
 assert(request(id_dynamic_keymap_macro_set_buffer,6065,1,&sentinel)==id_dynamic_keymap_macro_set_buffer);
 reboot_ram();assert(request(id_dynamic_keymap_macro_get_buffer,6065,1,0)==id_dynamic_keymap_macro_get_buffer && reply[4]==0x7b);
 output_chars=0;dynamic_keymap_macro_send(0);assert(output_chars==0 && !reading); // QMK unfinished-import sentinel retained
 sentinel=0;assert(request(id_dynamic_keymap_macro_set_buffer,6065,1,&sentinel)==id_dynamic_keymap_macro_set_buffer);
 for(unsigned id=0;id<109;id++){
  bank_reads=0;output_chars=0;dynamic_keymap_macro_send(id);
  assert(output_chars==(id==0?5957u:0u));assert(bank_reads==2 && !reading);
 }
 dynamic_keymap_macro_send(109);assert(!reading);
 unsigned writes=io_calls;
 for(unsigned off=0;off<6066;off+=28){unsigned n=6066-off>28?28:6066-off;assert(request(id_dynamic_keymap_macro_set_buffer,off,n,after+off)==id_dynamic_keymap_macro_set_buffer);}
 assert(io_calls==writes); // verified identical SET does not wear either bank
 assert(request(id_dynamic_keymap_macro_get_buffer,6065,2,0)==id_unhandled);assert(reply[4]==0 && reply[5]==0);
 assert(request(id_dynamic_keymap_macro_set_buffer,6065,2,after)==id_unhandled);
 assert(request(id_dynamic_keymap_macro_set_buffer,0,29,after)==id_unhandled);
 vial_unlocked=false;
 assert(request(id_dynamic_keymap_macro_set_buffer,0,1,after)==id_unhandled);
 assert(request(id_dynamic_keymap_macro_reset,0,0,0)==id_unhandled);assert(io_calls==writes);
 vial_unlocked=true;context_ok=false;
 assert(request(id_dynamic_keymap_macro_set_buffer,0,1,after)==id_unhandled);assert(io_calls==writes);
 context_ok=true;
 assert(sm62_macro_read_begin());assert(!sm62_macro_update(0,1,after));sm62_macro_read_end();
 assert(request(id_dynamic_keymap_macro_reset,0,0,0)==id_dynamic_keymap_macro_reset);
 reboot_ram();read_all(got);for(unsigned i=0;i<6066;i++)assert(got[i]==0);
 writes=io_calls;assert(request(id_dynamic_keymap_macro_reset,0,0,0)==id_dynamic_keymap_macro_reset);assert(io_calls==writes);
 assert_preserved();
 puts("PASS public complete raw_hid_receive: count109/6066, full import/readback/reboot, actual playback all109 incl5957-char macro, last-byte, zero-wear equality/reset, bounds, locked, denied owner, read lease; EEPROM8192 unchanged");
}
static void legacy_abort_marker(void){
 fresh();memset(eeprom+2968,0,SM62_MACRO_LEGACY_SIZE);
 eeprom[2968]='a';memcpy(original,eeprom,sizeof(eeprom));
 output_chars=0;dynamic_keymap_macro_send(0);assert(output_chars==1 && !reading);
 eeprom[2968+SM62_MACRO_LEGACY_SIZE-1]=0x7b;memcpy(original,eeprom,sizeof(eeprom));
 for(unsigned restart=0;restart<2;restart++){
  reboot_ram();output_chars=0;dynamic_keymap_macro_send(0);
  assert(output_chars==0 && !reading); /* Legacy interrupted import must stay muted. */
  read_all(got);assert(!memcmp(got,eeprom+2968,SM62_MACRO_LEGACY_SIZE));
  for(unsigned i=SM62_MACRO_LEGACY_SIZE;i<SM62_MACRO_SIZE;i++)assert(got[i]==0);
  assert(io_calls==0);assert_preserved();
 }
 /* An explicitly successful overlay update or reset may end legacy fallback. */
 uint8_t replacement[2]={'b',0};
 assert(request(id_dynamic_keymap_macro_set_buffer,0,2,replacement)==id_dynamic_keymap_macro_set_buffer);
 reboot_ram();output_chars=0;dynamic_keymap_macro_send(0);assert(output_chars==1 && !reading);
 assert(request(id_dynamic_keymap_macro_reset,0,0,0)==id_dynamic_keymap_macro_reset);
 reboot_ram();read_all(got);for(unsigned i=0;i<SM62_MACRO_SIZE;i++)assert(got[i]==0);
 assert_preserved();puts("PASS legacy abort marker: valid playback, interrupted-import refusal across reboot, unchanged public GET/EEPROM, explicit overlay update/reset recovery");
}
static void td_dispatch(void){
 fresh();
 for(unsigned op=1;op<=2;op++)for(unsigned index=0;index<256;index++){
  uint64_t q[4]={0};uint8_t *p=(uint8_t*)q;
  p[0]=id_vial_prefix;p[1]=vial_dynamic_entry_op;p[2]=op;p[3]=index;
  raw_hid_receive(p,32);assert(reply[0]==id_vial_prefix); // unhandled echo, not success(0)
 }
 for(unsigned len=0;len<32;len++){
  uint8_t q[32]={id_vial_prefix,vial_dynamic_entry_op,dynamic_vial_tap_dance_set,31},copy[32];memcpy(copy,q,32);
  vial_handle_cmd(q,len);assert(!memcmp(q,copy,32));
 }
 uint64_t q[4]={0};uint8_t*p=(uint8_t*)q;p[0]=id_vial_prefix;p[1]=vial_dynamic_entry_op;p[2]=dynamic_vial_get_number_of_entries;
 raw_hid_receive(p,32);assert(reply[0]==0 && reply[1]==32 && reply[2]==32);assert(io_calls==0);assert_preserved();
 puts("PASS actual VIA→Vial dispatcher: TD GET/SET all256 indices excluded, malformed Vial lengths0..31 rejected, count0; zero storage writes");
}
static void fault_campaigns(void){
 fresh();memset(after,0x3a,sizeof(after));assert(sm62_macro_update(0,sizeof(after),after));
 memcpy(saved,(const void*)(0x10000000+SM62_MACRO_FLASH_BASE),sizeof(saved));memcpy(before,after,sizeof(before));after[256]^=0x71;
 for(unsigned mode=0;mode<2;mode++)for(unsigned f=0;f<28;f++){
  memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));reboot_ram();
  if(mode) silent_call=f;else fail_call=f;
  uint8_t result=request(id_dynamic_keymap_macro_set_buffer,256,1,after+256);
  reboot_ram();assert(sm62_macro_read(0,sizeof(got),got));
  if(result!=id_unhandled)assert(!memcmp(got,after,sizeof(got)));
  else assert(!memcmp(got,before,sizeof(got)) || !memcmp(got,after,sizeof(got)));
  faults_checked++;
 }
 for(unsigned f=0;f<28;f++){
  memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));reboot_ram();read_error_at=f;
  assert(request(id_dynamic_keymap_macro_set_buffer,256,1,after+256)==id_unhandled);
  reboot_ram();assert(sm62_macro_read(0,sizeof(got),got));
  assert(!memcmp(got,before,sizeof(got)) || !memcmp(got,after,sizeof(got)));faults_checked++;
 }
 memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));reboot_ram();
 for(unsigned b=0;b<2;b++){
  read_fail=b;assert(request(id_dynamic_keymap_macro_get_buffer,0,28,0)==id_unhandled);
  for(unsigned i=4;i<32;i++)assert(reply[i]==0);
  assert(request(id_dynamic_keymap_macro_set_buffer,0,28,before)==id_unhandled);assert(io_calls==0);
  read_fail=-1;faults_checked++;
 }
 /* Header/version/payload/marker integrity bits. Single-bank corruption must
  * not certify a bad value as successful equal SET. Marker faults may recover
  * the immutable legacy image (same as a torn initial commit). */
 for(unsigned region=0;region<3;region++){
  unsigned start=region==0?0:region==1?256:6400;
  unsigned len=region==0?256:region==1?6066:256;
  for(unsigned i=0;i<len;i++)for(unsigned bit=0;bit<8;bit++){
   memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));reboot_ram();
   ((uint8_t*)macro_hw_bank(0))[start+i]^=1u<<bit;
   bool ok=sm62_macro_read(0,sizeof(got),got);
   if(ok){assert(!memcmp(got,eeprom+2968,5224));for(unsigned x=5224;x<6066;x++)assert(got[x]==0);}
   faults_checked++;
  }
 }
 memcpy((void*)(0x10000000+SM62_MACRO_FLASH_BASE),saved,sizeof(saved));reboot_ram();
 ((uint8_t*)macro_hw_bank(0))[300]^=1;
 assert(request(id_dynamic_keymap_macro_set_buffer,0,28,before)==id_unhandled);assert(io_calls==0); // corrupt committed store != equal success
 /* Generation rollover uses production CRC helpers, both banks adjacent. */
 fresh();assert(sm62_macro_update(0,1,(uint8_t[]){0x91}));
 uint8_t *bank=(uint8_t*)macro_hw_bank(0);store32(bank+8,UINT32_MAX);store32(bank+20,header_crc(bank));
 assert(sm62_macro_update(0,1,(uint8_t[]){0x92}));assert(load32(macro_hw_bank(1)+8)==0);
 reboot_ram();assert(sm62_macro_read(0,1,got) && got[0]==0x92);
 assert(sm62_macro_update(0,1,(uint8_t[]){0x93}));assert(load32(macro_hw_bank(0)+8)==1);
 reboot_ram();assert(sm62_macro_read(0,1,got) && got[0]==0x93);
 /* Inconsistent serials fail closed; no arbitrary winner/equality success. */
 bank=(uint8_t*)macro_hw_bank(0);store32(bank+8,100);store32(bank+20,header_crc(bank));
 assert(!sm62_macro_read(0,1,got));assert(got[0]==0);
 assert(request(id_dynamic_keymap_macro_set_buffer,0,1,(uint8_t[]){0x93})==id_unhandled);
 assert_preserved();
 printf("PASS faults: %u read/erase/program/CRC-header/payload/marker bit controls; no fake public success, fail-closed equality, generation UINT32_MAX→0→1\n",faults_checked);
}
int main(int argc,char**argv){
 assert(mmap((void*)0x10000000,4194304,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==(void*)0x10000000);
 if(argc==2 && !strcmp(argv[1],"--legacy-abort-only")){legacy_abort_marker();return 0;}
 if(argc==3){
  FILE*f;
  if(!strcmp(argv[1],"--save")){
   fresh();for(unsigned i=0;i<6066;i++)after[i]=(i*23+17)&255;assert(sm62_macro_update(0,6066,after));
   f=fopen(argv[2],"wb");assert(f);assert(fwrite((const void*)0x10000000,1,4194304,f)==4194304);assert(fwrite(eeprom,1,8192,f)==8192);fclose(f);puts("PASS fresh-process persistent image saved");return 0;
  }
  assert(!strcmp(argv[1],"--load"));f=fopen(argv[2],"rb");assert(f);assert(fread((void*)0x10000000,1,4194304,f)==4194304);assert(fread(eeprom,1,8192,f)==8192);fclose(f);
  read_all(got);for(unsigned i=0;i<6066;i++)assert(got[i]==((i*23+17)&255));assert(io_calls==0);puts("PASS independently restarted process: full6066 public GET and zero writes");return 0;
 }
 legacy_abort_marker();public_lifecycle();td_dispatch();storage_campaigns();fault_campaigns();puts("ALL MACRO STORAGE TESTS PASS");return 0;
}
