static void reset(uint32_t at) { mock_now=at; holding=false; vial_unlocked=0; command(vial_unlock_start); }
static void scan(bool down) { holding=down; vial_unlock_task(); }
static void advance(unsigned duration, unsigned cadence) {
    for(unsigned i=0;i<duration;i++) { mock_now++; vial_unlock_task(); if(cadence && i%cadence==0) command(vial_unlock_poll); }
}
int main(void) {
    reset(0); advance(10000,0); scan(true); command(vial_unlock_poll); assert(!vial_unlocked);
    advance(2999,0); assert(!vial_unlocked && vial_unlock_counter==1); advance(1,0); assert(vial_unlocked);
    unsigned cadences[]={0,1,10,100,333,1000,4000};
    for(unsigned i=0;i<sizeof(cadences)/sizeof(cadences[0]);i++) {
        reset(0); scan(true); advance(2999,cadences[i]); assert(!vial_unlocked);
        advance(1,cadences[i]); assert(vial_unlocked && !vial_unlock_in_progress);
        reset(0); scan(true); advance(2900,cadences[i]); scan(false); advance(1,0); scan(true);
        command(vial_unlock_poll); assert(!vial_unlocked && vial_unlock_counter==30);
        advance(2999,cadences[i]); assert(!vial_unlocked); advance(1,cadences[i]); assert(vial_unlocked);
    }
    reset(UINT32_MAX-1500); scan(true); advance(2999,1); assert(!vial_unlocked); advance(1,1); assert(vial_unlocked);
    reset(0); scan(true); advance(2900,1); command(vial_unlock_start); scan(true); advance(2999,1); assert(!vial_unlocked); advance(1,1); assert(vial_unlocked);
    reset(0); scan(true); advance(2900,1); command(vial_lock); advance(5000,1); assert(!vial_unlocked && !vial_unlock_in_progress);
    reset(0); mock_now=100000; holding=true; command(vial_unlock_poll); assert(!vial_unlocked); scan(true); assert(!vial_unlocked);
    /* Irregular scans must consume full ticks but retain the remainder. */
    unsigned gaps[]={1,7,99,100,101,333,999,2999,3000,65000,65537,100000,0x80000000u};
    uint32_t starts[]={0,UINT16_MAX-1500,UINT32_MAX-1500};
    for(unsigned start=0;start<sizeof(starts)/sizeof(starts[0]);start++) {
        for(unsigned g=0;g<sizeof(gaps)/sizeof(gaps[0]);g++) {
            reset(starts[start]); scan(true);
            unsigned elapsed=0;
            do {
                mock_now+=gaps[g]; elapsed+=gaps[g]; vial_unlock_task();
                unsigned left=elapsed>=3000?0:30-elapsed/100;
                assert(vial_unlock_counter==left);
                assert(vial_unlocked==(elapsed>=3000));
            } while(elapsed<3000);
        }
    }
    reset(0); scan(true); mock_now=2999; vial_unlock_task(); assert(vial_unlock_counter==1 && !vial_unlocked);
    scan(false); mock_now+=100000; scan(true); assert(!vial_unlocked && vial_unlock_counter==30);
    mock_now+=2999; vial_unlock_task(); assert(!vial_unlocked); mock_now++; vial_unlock_task(); assert(vial_unlocked);
    /* Differential reference at every irregular physical sample; polling is
     * reporting only, including START/release/LOCK and 32-bit wrap cases. */
    uint32_t random=1, ref_start=0; bool ref_holding=false, ref_active=false;
    reset(UINT32_MAX-20000); ref_active=true;
    for(unsigned step=0;step<20000;step++) {
        random=random*1664525u+1013904223u;
        mock_now+=(random>>16)%431;
        if(step%79==0) { command(vial_unlock_start); vial_unlocked=0; ref_holding=false; ref_active=true; }
        if(step%127==0) { command(vial_lock); ref_holding=false; ref_active=false; }
        holding=(random&15)!=0;
        bool expected_unlocked=vial_unlocked;
        unsigned expected_counter=vial_unlock_counter;
        if(ref_active) {
            if(!holding) { ref_holding=false; expected_counter=30; }
            else {
                if(!ref_holding) { ref_holding=true; ref_start=mock_now; }
                uint32_t held=mock_now-ref_start;
                if(held>=3000) { ref_active=false; expected_counter=0; expected_unlocked=true; }
                else expected_counter=30-held/100;
            }
        }
        command(vial_unlock_poll); vial_unlock_task();
        assert(vial_unlocked==expected_unlocked && vial_unlock_counter==expected_counter && vial_unlock_in_progress==ref_active);
    }
    puts("unlock: 39 irregular-scan/wrap/gap sequences, 32-bit half-range gap and 20000 differential physical samples: PASS");
    puts("unlock: exact 3000ms continuous hold, 1ms release, seven poll cadences incl absent, restart/lock cancellation, unsampled time and 32-bit wrap: PASS");
}
