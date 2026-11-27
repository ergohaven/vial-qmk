/* Current shipping read-only archive / host-managed compatibility contract.
 * Other locked assignment and exhaustive ingress cases stay in cases.c.
 * Archive is produced by the separately pinned 4.2.0 control, never by a
 * synthesized successful production write or an ingress authorization stub. */
static void resident_case(void) {
    const char *path=getenv("SM62_LOCKED_ARCHIVE");assert(path);
    FILE*f=fopen(path,"rb");assert(f && fread(flash,1,sizeof(flash),f)==sizeof(flash) && fgetc(f)==EOF && !fclose(f));
    uint32_t durable_crc=app_compact_crc(&flash[0][0],sizeof(flash));
    uint8_t raw[APP_COMPACT_LIMIT];eh_app_layout_staging_t s={0};s.keycodes[0][0]=5;
    vial_unlocked=false;publish(4,false,false);
    unsigned n=full(raw,&s);uint32_t g=app_layout_visual_generation;
    transfer(raw,n,1,true);
    CHECK(app_layout_visual_generation==g && effective(0)==4,"compact original raw CRC mismatch rejected");
    uint32_t crc=transfer(raw,n,2,false);
    CHECK(!app_compact.valid && app_layout_visual_generation==g && effective(0)==4,"removed stage-only mode refuses without publication");
    for(unsigned lock=0;lock<2;lock++) {
        vial_unlocked=lock;
        for(unsigned op=1;op<=5;op++)if(op!=3)
            CHECK(resident(op,0,raw,crc)==2,"old READY/STORE/SELECT/DELETE unavailable locked and unlocked");
        CHECK(effective(0)==4 && app_layout_visual_generation==g,"old commands never publish application or native Default");
    }
    vial_unlocked=false;transfer(raw,n,1,false);
    CHECK(effective(0)==5,"locked ordinary host RAM publication succeeds");
    s.keycodes[15][61]=QK_BOOT;n=full(raw,&s);g=app_layout_visual_generation;
    transfer(raw,n,1,false);
    CHECK(app_layout_visual_generation==g && effective(0)==5,"compact immediate publication refuses locked last key");
    crc=transfer(raw,n,2,false);
    CHECK(resident(5,1,raw,crc)==2 && resident(1,1,raw,crc)==2,"locked forbidden import cannot produce READY or STORE");
    vial_unlocked=true;transfer(raw,n,2,false);
    CHECK(app_layout_visual_generation==g && resident(1,1,raw,crc)==2,"unlocked obsolete mode cannot revive resident STORE");
    transfer(raw,n,1,false);
    CHECK(app_layout_keycodes[15][61]==QK_BOOT,"unlocked host RAM publication preserves original forbidden key");
    vial_unlocked=false;cache_scanned=false;publish(4,false,false);g=app_layout_visual_generation;
    CHECK(resident(2,1,raw,crc)==2 && resident_read_exact(1,raw,n,crc) && cache_scanned,
          "cold locked SELECT refuses; historical boot-bearing QUERY/ENUM/READ exact");
    CHECK(app_layout_visual_generation==g && effective(0)==4,"cold semantic archive scan cannot grant ingress or activate map");
    vial_unlocked=true;
    CHECK(resident(2,1,raw,crc)==2,"unlock cannot re-enable SELECT after cold scan");
    vial_unlocked=false;transfer(raw,n,1,false);
    CHECK(app_layout_visual_generation==g && effective(0)==4,"semantic archive read does not authorize later locked host publication");
    /* Cover unused stack fields as well as active key ingress on this source. */
    s.keycodes[15][61]=0;s.stack_keycodes[15][3][1]=QK_BOOT;n=full(raw,&s);
    crc=app_compact_crc(raw,n);cache_scanned=false;
    CHECK(resident_read_exact(2,raw,n,crc),"cold locked archive exports exact inactive encoder-stack QK_BOOT");
    transfer(raw,n,1,false);
    CHECK(app_layout_visual_generation==g,"locked compact rejects inactive stack QK_BOOT");
    vial_unlocked=true;transfer(raw,n,1,false);
    CHECK(app_layout_stack_keycodes[15][3][1]==QK_BOOT,"unlocked host RAM preserves inactive stack payload");
    CHECK(flash_erases==0 && flash_programs==0 && app_compact_crc(&flash[0][0],sizeof(flash))==durable_crc,
          "all current commands and lock transitions leave entire historical archive immutable");
    CHECK(boot_count==0,"current assignment/import/read tests never jump to bootloader");
}
