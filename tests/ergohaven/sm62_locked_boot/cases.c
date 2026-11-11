static unsigned failures;
#define CHECK(c, label) do { if (!(c)) { fprintf(stderr,"FAIL %s\n",label);failures++; } else printf("PASS %s\n",label); } while(0)
static uint16_t native_get(void) {
    uint8_t p[32] __attribute__((aligned(8)))={id_dynamic_keymap_get_keycode,0,0,0};
    unsigned old=sent;raw_hid_receive(p,32);assert(sent==old+1);
    assert(reply[0]==id_dynamic_keymap_get_keycode);
    return ((uint16_t)reply[4]<<8)|reply[5];
}
static void native_set(uint16_t kc) {
    uint8_t p[32] __attribute__((aligned(8)))={id_dynamic_keymap_set_keycode,0,0,0,kc>>8,kc};
    unsigned old=sent;raw_hid_receive(p,32);assert(sent==old+1);
}
static void native_case(void) {
    vial_unlocked=false;
    native_set(4);CHECK(native_get()==4,"native locked ordinary SET/GET");
    vial_unlocked=true;native_set(QK_BOOT);
    CHECK(native_get()==QK_BOOT,"native unlocked QK_BOOT SET/GET");
    vial_unlocked=false;
    nvm_dynamic_keymap_update_keycode(0,0,0,QK_BOOT);
    CHECK(native_get()==QK_BOOT,"native internal/factory NVM path permits QK_BOOT while locked");
    uint8_t bulk[2]={QK_BOOT>>8,QK_BOOT&255};
    nvm_dynamic_keymap_update_buffer(0,2,bulk);
    CHECK(native_get()==0xffff,"bulk existing locked QK_BOOT replacement remains FFFF");
    native_set(4);native_set(QK_BOOT);
    CHECK(native_get()==KC_NO,"native locked QK_BOOT assignment neutralized");
    CHECK(boot_count==0,"native assignment alone never jumps to bootloader");
}
static void wire(uint8_t p[32]) {
    // Enter via the complete public VIA dispatcher and keyboard callback,
    // not a direct staging helper or a synthesized publication.
    uint8_t aligned[32] __attribute__((aligned(8)));memcpy(aligned,p,32);
    unsigned old=sent;raw_hid_receive(aligned,32);assert(sent==old+1);
}
static void publish_snapshot(const eh_app_layout_staging_t *source,bool bad_crc,bool incomplete) {
    eh_app_layout_staging_t expected=*source;
    uint8_t begin[32]={0xe1,8,1,42,0,0,0,62,16};wire(begin);
    for(unsigned l=0;l<16;l++) {
        for(unsigned s=0;s<62;s+=13) {
            unsigned n=62-s<13?62-s:13;
            uint8_t p[32]={0xe2,8,l,s,n};
            for(unsigned i=0;i<n;i++) {
                uint16_t k=expected.keycodes[l][s+i];p[5+2*i]=k;p[6+2*i]=k>>8;
            }
            if (!(incomplete && l==15 && s==52)) wire(p);
        }
        for(unsigned s=0;s<62;s+=27) {
            unsigned n=62-s<27?62-s:27;uint8_t p[32]={0xe7,8,l,s,n};wire(p);
        }
        uint8_t name[32]={0xe4,8,l,0};wire(name);
        for(unsigned s=0;s<4;s++){
            uint16_t a=expected.stack_keycodes[l][s][0],b=expected.stack_keycodes[l][s][1];
            uint8_t p[32]={0xe9,8,l,s,expected.stack_counts[l],0,a,a>>8,b,b>>8};wire(p);
        }
    }
    // Compute CRC from a separate packet-source snapshot, never parser staging.
    uint16_t crc=app_layout_crc16(&expected) ^ (bad_crc?1:0);
    uint8_t commit[32]={0xe3,8,1,42,0,0,0,crc,crc>>8};wire(commit);
}
static void publish(uint16_t kc,bool bad_crc,bool incomplete) {
    eh_app_layout_staging_t expected={0};
    expected.keycodes[0][0]=kc;expected.keycodes[0][1]=4;
    publish_snapshot(&expected,bad_crc,incomplete);
}
static uint16_t effective(unsigned control) {
    uint16_t kc=0xdead;bool got=hid_app_layout_get_keycode(0,control,&kc);
    assert(got);return kc;
}
static void application_case(void) {
    vial_unlocked=false;publish(5,false,false);
    CHECK(effective(0)==5 && effective(1)==4,"application SINGLE8 complete locked ordinary publication");
    publish(QK_BOOT,true,false);
    CHECK(effective(0)==5,"application bad CRC does not publish");
    publish(QK_BOOT,false,true);
    CHECK(effective(0)==5,"application incomplete chunks do not publish");
    vial_unlocked=true;publish(QK_BOOT,false,false);
    CHECK(effective(0)==QK_BOOT,"application unlocked QK_BOOT publication");
    publish(5,false,false);
    vial_unlocked=false;publish(QK_BOOT,false,false);
    CHECK(effective(0)==5,"application locked QK_BOOT publication rejected atomically");
    CHECK(effective(1)==4,"application locked ordinary neighbor preserved");
    now+=2501;hid_app_layout_task();uint16_t kc;
    CHECK(!hid_app_layout_get_keycode(0,0,&kc),"application lease expiration preserved");
    CHECK(boot_count==0,"application publication alone never jumps to bootloader");
}
/* Packet-source FULL builder, independent of parser staging. */
static unsigned full(uint8_t *raw,const eh_app_layout_staging_t *s) {
    memset(raw,0,APP_COMPACT_LIMIT);
    uint8_t begin[32]={0xe1,8,1,42,0,0,0,62,16};memcpy(raw,begin,32);
    uint16_t crc=app_layout_crc16(s);
    uint8_t commit[32]={0xe3,8,1,42,0,0,0,crc,crc>>8,0x57,2};memcpy(raw+32,commit,32);
    for(unsigned i=0;i<20;i++)raw[43+i]=i+1;
    unsigned pos=64;
    for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++) {
        uint16_t k=s->keycodes[l][c];raw[pos++]=k;raw[pos++]=k>>8;
    }
    pos+=16*62; /* visuals are zero */
    for(unsigned l=0;l<16;l++)for(unsigned slot=0;slot<4;slot++) {
        raw[pos++]=s->stack_counts[l];raw[pos++]=0;
        for(unsigned d=0;d<2;d++){uint16_t k=s->stack_keycodes[l][slot][d];raw[pos++]=k;raw[pos++]=k>>8;}
    }
    pos+=16;return pos;
}
static uint32_t transfer(const uint8_t *raw,unsigned n,unsigned mode,bool corrupt_raw_crc) {
    uint32_t crc=app_compact_crc(raw,n),tx=123;
    unsigned chunks=(n+23)/24;
    uint8_t p[32]={0xec,8,mode,0};cache_put32(p+4,tx);
    p[8]=n;p[9]=n>>8;p[10]=n;p[11]=n>>8;p[12]=chunks;p[13]=chunks>>8;
    cache_put32(p+16,crc^(corrupt_raw_crc?1:0));cache_put32(p+20,crc);wire(p);
    for(unsigned i=0;i<chunks;i++) {
        memset(p,0,32);p[0]=0xed;p[1]=8;cache_put32(p+2,tx);p[6]=i;p[7]=i>>8;
        unsigned count=n-i*24;if(count>24)count=24;memcpy(p+8,raw+i*24,count);wire(p);
    }
    memset(p,0,32);p[0]=0xee;p[1]=8;cache_put32(p+2,tx);p[6]=chunks;p[7]=chunks>>8;cache_put32(p+8,crc);wire(p);
    return crc;
}
static unsigned resident(unsigned op,unsigned slot,const uint8_t *raw,uint32_t crc) {
    uint8_t p[32]={0xef,9,op,slot};cache_put32(p+4,123);memcpy(p+8,raw+43,20);cache_put32(p+28,crc);wire(p);
    assert(reply[2]==(op|0x80));return reply[3];
}
/* Public read-only visibility of an immutable boot-bearing record while locked. */
static bool resident_read_exact(unsigned slot,const uint8_t *raw,unsigned n,uint32_t crc) {
    bool ok=resident(0,slot,raw,crc)==0;
    uint8_t p[32]={0xef,9,3,slot};wire(p);
    ok=ok && reply[3]==0 && !memcmp(reply+8,raw+43,20) && app_layout_read_u32(reply+28)==crc;
    for(unsigned off=0;off<n;off+=20) {
        memset(p,0,sizeof(p));p[0]=0xef;p[1]=9;p[2]=6;p[3]=slot;
        p[8]=off;p[9]=off>>8;p[10]=20;wire(p);
        unsigned count=n-off;if(count>20)count=20;
        ok=ok && reply[11]==0 && reply[10]==count && !memcmp(reply+12,raw+off,count);
    }
    return ok;
}
static void ingress_case(void) {
    eh_app_layout_staging_t s={0};s.keycodes[0][0]=5;s.keycodes[0][1]=4;
    vial_unlocked=false;publish_snapshot(&s,false,false);
    unsigned rejected=0;
    for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++) {
        s.keycodes[l][c]=QK_BOOT;uint32_t g=app_layout_visual_generation;
        publish_snapshot(&s,false,false);rejected+=app_layout_visual_generation==g && !app_layout_staging.valid;
        s.keycodes[l][c]=(l==0 && c<2)?(c?4:5):0;
    }
    CHECK(rejected==16*62,"SINGLE8 rejects QK_BOOT at every layer/control ingress");
    rejected=0;
    for(unsigned l=0;l<16;l++)for(unsigned slot=0;slot<4;slot++)for(unsigned d=0;d<2;d++) {
        s.stack_keycodes[l][slot][d]=QK_BOOT;uint32_t g=app_layout_visual_generation;
        publish_snapshot(&s,false,false);rejected+=app_layout_visual_generation==g && !app_layout_staging.valid;
        s.stack_keycodes[l][slot][d]=0;
    }
    CHECK(rejected==16*4*2,"SINGLE8 rejects every stack slot/direction even count zero");
    s.stack_counts[15]=4;s.stack_keycodes[15][3][1]=QK_BOOT;
    uint32_t g=app_layout_visual_generation;publish_snapshot(&s,false,false);
    CHECK(app_layout_visual_generation==g,"active four-slot encoder stack rejects locked QK_BOOT");
    vial_unlocked=true;publish_snapshot(&s,false,false);
    CHECK(app_layout_stack_keycodes[15][3][1]==QK_BOOT,"unlocked stack QK_BOOT remains exact");
    s.stack_keycodes[15][3][1]=4;vial_unlocked=false;publish_snapshot(&s,false,false);
    CHECK(app_layout_stack_counts[15]==4 && app_layout_stack_keycodes[15][3][1]==4,"locked ordinary four-slot stack preserved");
    CHECK(boot_count==0,"ingress assignment tests never jump to bootloader");
}
static void resident_case(void) {
    uint8_t raw[APP_COMPACT_LIMIT];eh_app_layout_staging_t s={0};s.keycodes[0][0]=5;
    memset(flash,255,sizeof(flash));
    vial_unlocked=false;publish(4,false,false);
    unsigned n=full(raw,&s);uint32_t g=app_layout_visual_generation;
    transfer(raw,n,1,true);
    CHECK(app_layout_visual_generation==g && effective(0)==4,"compact original raw CRC mismatch rejected");
    uint32_t crc=transfer(raw,n,2,false);
    CHECK(app_compact_last_valid && effective(0)==4,"locked ordinary stage-only import validates without publication");
    CHECK(resident(1,0,raw,crc)==0,"locked ordinary resident STORE succeeds");
    CHECK(resident(2,0,raw,crc)==0 && effective(0)==5,"locked ordinary SELECT succeeds");
    unsigned erases=flash_erases,programs=flash_programs;
    CHECK(resident(2,0,raw,crc)==0 && flash_erases==erases && flash_programs==programs,"resident SELECT remains no-wear");
    CHECK(!memcmp(sm62_cache_flash_record(cache_current(0))+256,raw,n),"resident ordinary raw payload remains byte exact");
    s.keycodes[15][61]=QK_BOOT;n=full(raw,&s);g=app_layout_visual_generation;
    transfer(raw,n,1,false);
    CHECK(!app_compact_last_valid && app_layout_visual_generation==g,"compact immediate publication refuses locked last key");
    crc=transfer(raw,n,2,false);
    CHECK(!app_compact_last_valid && resident(5,1,raw,crc)!=0,"locked resident import cannot produce READY token");
    CHECK(resident(1,1,raw,crc)!=0 && flash_erases==erases && flash_programs==programs,"locked forbidden STORE refuses without flash writes");
    vial_unlocked=true;crc=transfer(raw,n,2,false);
    CHECK(app_compact_last_valid && resident(1,1,raw,crc)==0,"unlocked forbidden-key resident import and STORE succeed");
    CHECK(!memcmp(sm62_cache_flash_record(cache_current(1))+256,raw,n),"unlocked forbidden-key resident raw payload remains byte exact");
    crc=transfer(raw,n,2,false);vial_unlocked=false;
    erases=flash_erases;programs=flash_programs;
    CHECK(resident(1,3,raw,crc)!=0 && flash_erases==erases && flash_programs==programs,"STORE revalidates lock after an unlocked READY transaction");
    vial_unlocked=false;g=app_layout_visual_generation;erases=flash_erases;programs=flash_programs;
    CHECK(resident(2,1,raw,crc)!=0 && app_layout_visual_generation==g && effective(0)==5,"warm-index SELECT rechecks current lock and refuses QK_BOOT");
    CHECK(flash_erases==erases && flash_programs==programs,"refused SELECT preserves all durable bytes");
    vial_unlocked=true;
    CHECK(resident(2,1,raw,crc)==0 && app_layout_keycodes[15][61]==QK_BOOT,"unlocked SELECT preserves original forbidden-key payload");
    vial_unlocked=false;cache_scanned=false;publish(4,false,false);g=app_layout_visual_generation;
    uint32_t durable_crc=app_compact_crc(&flash[0][0],sizeof(flash));
    bool refused=resident(2,1,raw,crc)!=0;
    bool readable=resident_read_exact(1,raw,n,crc);
    CHECK(refused && readable && cache_scanned && app_layout_visual_generation==g && effective(0)==4 &&
          flash_erases==erases && flash_programs==programs && app_compact_crc(&flash[0][0],sizeof(flash))==durable_crc &&
          app_compact_crc(raw,n)==crc,"cold locked SELECT refuses; QUERY/ENUM/READ remain byte exact without mutation");
    vial_unlocked=true;
    CHECK(cache_scanned && resident(2,1,raw,crc)==0 && app_layout_keycodes[15][61]==QK_BOOT &&
          flash_erases==erases && flash_programs==programs && app_compact_crc(&flash[0][0],sizeof(flash))==durable_crc,
          "unlock after cold locked scan permits existing resident QK_BOOT without rescan or flash writes");
    vial_unlocked=false;
    /* Stack ingress in compact validation-only mode, including unused fields. */
    s.keycodes[15][61]=0;s.stack_keycodes[15][3][1]=QK_BOOT;n=full(raw,&s);
    crc=transfer(raw,n,2,false);
    CHECK(!app_compact_last_valid && resident(1,2,raw,crc)!=0,"resident import refuses inactive encoder stack QK_BOOT");
    vial_unlocked=true;crc=transfer(raw,n,2,false);
    CHECK(app_compact_last_valid && resident(1,2,raw,crc)==0,"unlocked inactive-stack resident STORE succeeds");
    vial_unlocked=false;g=app_layout_visual_generation;
    CHECK(resident(2,2,raw,crc)!=0 && app_layout_visual_generation==g,"resident SELECT refuses inactive encoder stack QK_BOOT");
    CHECK(boot_count==0,"resident assignment tests never jump to bootloader");
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"native"))native_case();
    else if(!strcmp(argv[1],"application"))application_case();
    else if(!strcmp(argv[1],"ingress"))ingress_case();
    else if(!strcmp(argv[1],"resident"))resident_case();
    else return 2;
    printf("RESULT failures=%u bootloader_calls=%u\n",failures,boot_count);
    return failures?1:0;
}
