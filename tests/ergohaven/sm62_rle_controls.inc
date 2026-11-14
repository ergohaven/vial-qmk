/* Included into both host-native production translation units, not firmware. */
unsigned ROLE_encoded(void *out) {
    if (!IS_MASTER) return 0;
    uint16_t n = sm62_app_encode();
    memcpy(out, sm62_app_tx_encoded, n); return n;
}
static void native_rpc(uint8_t *p, uint8_t *ack) {
    uint16_t c=sm62_app_crc(p,38);p[38]=c;p[39]=c>>8;
    callback(40,p,5,ack);
}
static void native_begin(uint8_t *p, unsigned length, uint16_t crc) {
    memset(p,0,40);sm62_app_put32(p,sm62_app_rx_epoch+1);
    p[4]=0xfd;p[5]=0xff;p[7]=length;p[8]=length>>8;
    p[9]=sizeof(sm62_app_rx)&255;p[10]=sizeof(sm62_app_rx)>>8;
    p[11]=crc;p[12]=crc>>8;p[13]=1;
}
static bool native_stream(const uint8_t *b,unsigned length) {
    uint8_t p[40],ack[5];bool accepted=false;
    for(unsigned i=0;i<length;i+=31){
        memset(p,0,40);sm62_app_put32(p,sm62_app_rx_epoch);p[4]=i;p[5]=i>>8;
        unsigned n=length-i>31?31:length-i;p[6]=n;memcpy(p+7,b+i,n);
        native_rpc(p,ack);accepted=ack[0]==1;
        if(i+n<length)assert(accepted&&!sm62_app_rx_pending);
    }
    return accepted;
}
void ROLE_bad_format_controls(void *encoded, unsigned *length, uint16_t *crc) {
    uint8_t old=sm62_app_tx.format;
    sm62_app_tx.format=0;sm62_app_tx.crc=sm62_app_crc(&sm62_app_tx,offsetof(sm62_app_image_t,crc));
    *crc=sm62_app_tx.crc;*length=sm62_app_encode();assert(*length);
    memcpy(encoded,sm62_app_tx_encoded,*length);
    sm62_app_tx.format=old;sm62_app_tx.crc=sm62_app_crc(&sm62_app_tx,offsetof(sm62_app_image_t,crc));
}
void ROLE_reject_format(const void *encoded,unsigned length,uint16_t crc) {
    uint8_t p[40],ack[5];native_begin(p,length,crc);native_rpc(p,ack);assert(ack[0]==4);
    assert(!native_stream(encoded,length)&&sm62_app_rx_invalid&&!sm62_app_rx_pending);
    puts("PASS CRC-correct decoded image with invalid format rejected before publication");
}
void ROLE_receiver_controls(const void *encoded,unsigned length,const void *image) {
    assert(!IS_MASTER&&length>31);
    const sm62_app_image_t *s=image;uint8_t p[40],ack[5];
    // Invalid BEGIN cannot disturb retained valid publication or admit a stream.
    for(unsigned mode=0;mode<9;mode++){
        native_begin(p,length,s->crc);
        switch(mode){case 0:p[7]=p[8]=0;break;case 1:p[7]=255;p[8]=255;break;
        case 2:p[9]^=1;break;case 3:p[13]=2;break;case 4:p[14]=1;break;
        case 5:sm62_app_put32(p,sm62_app_rx_epoch);break;
        case 6:p[6]=32;break;case 7:p[6]=1;break;case 8:p[10]^=1;break;}
        uint32_t e=sm62_app_rx_epoch;native_rpc(p,ack);assert(!ack[0]&&sm62_app_rx_epoch==e&&!sm62_app_rx_pending);
    }
    puts("PASS 9 malformed/version/epoch/length BEGIN rejections");
    // Underflow, truncated payload, overflow and same-length corruption.
    uint8_t broken[sizeof(sm62_app_image_t)];
    for(unsigned mode=0;mode<5;mode++){
        unsigned n=length;memcpy(broken,encoded,n);
        if(mode==0){n=1;broken[0]=0x80;} // missing repeated word
        if(mode==1){n=3;broken[0]=0xff;broken[1]=broken[2]=0;} // short decoded image
        if(mode==2){n=81;for(unsigned i=0;i<n;i+=3){broken[i]=0xff;broken[i+1]=broken[i+2]=0;}} // output overflow
        if(mode==3){n=length-1;} // incomplete final word
        if(mode==4){broken[1]^=1;} // valid packet CRC but invalid whole image
        native_begin(p,n,s->crc);native_rpc(p,ack);assert(ack[0]==4);
        assert(!native_stream(broken,n)&&!sm62_app_rx_pending&&sm62_app_rx_invalid);
        hid_app_layout_split_task();assert(!memcmp(app_layout_keycodes,s->keycodes,sizeof(s->keycodes)));
    }
    puts("PASS 5 malformed/truncated/overflow/corrupt streams rejected without publication");
    native_begin(p,length,s->crc);native_rpc(p,ack);assert(ack[0]==4);
    native_rpc(p,ack);assert(ack[0]==4); // exact BEGIN ACK retry
    p[11]^=1;native_rpc(p,ack);assert(!ack[0]);p[11]^=1;
    // Out-of-order and impossible DATA boundaries cannot advance progress.
    uint8_t data[40]={0};sm62_app_put32(data,sm62_app_rx_epoch);data[4]=31;data[6]=31;memcpy(data+7,(const uint8_t*)encoded+31,31);
    native_rpc(data,ack);assert(!ack[0]&&!sm62_app_rx_encoded_offset);
    data[4]=1;native_rpc(data,ack);assert(!ack[0]&&!sm62_app_rx_encoded_offset);
    data[4]=0;data[6]=30;native_rpc(data,ack);assert(!ack[0]);
    data[6]=31;memcpy(data+7,encoded,31);native_rpc(data,ack);assert(ack[0]==1);
    native_rpc(data,ack);assert(ack[0]==1&&sm62_app_rx_encoded_offset==31);
    data[7]^=1;native_rpc(data,ack);assert(!ack[0]&&sm62_app_rx_encoded_offset==31);
    native_rpc(p,ack);assert(!ack[0]); // BEGIN after progress is not an exact retry
    clock_us+=(EH_APP_LAYOUT_TIMEOUT_MS+1)*1000ULL;
    data[4]=31;memcpy(data+7,(const uint8_t*)encoded+31,31);native_rpc(data,ack);assert(!ack[0]&&sm62_app_rx_invalid);
    puts("PASS exact BEGIN/DATA retry, mismatched duplicate, reordered DATA and staging TTL");
    // Newer epoch supersedes a stalled stream; a completed image freezes staging.
    native_begin(p,length,s->crc);native_rpc(p,ack);assert(ack[0]==4);
    assert(native_stream(encoded,length)&&sm62_app_rx_pending==1);
    native_begin(p,length,s->crc);native_rpc(p,ack);assert(!ack[0]&&sm62_app_rx_pending==1);
    clock_us+=(EH_APP_LAYOUT_TIMEOUT_MS+1)*1000ULL;
    hid_app_layout_split_task();assert(!sm62_app_rx_pending&&sm62_app_rx_retained);
    // Delayed main-loop adoption cannot revive an expired staged active image.
    assert(!app_layout_active);
    puts("PASS newer epoch supersedes incomplete stream; complete staging frozen until main-loop publication; expired active lease not revived");
}
