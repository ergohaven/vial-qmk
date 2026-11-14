static unsigned flushes, repeats;
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p) { (void)a;(void)p;flushes++;lv_disp_flush_ready(d); }
static void process_record(keyrecord_t *r) {
    uint16_t k=get_record_keycode(r,true);
    if (preprocess_tap_dance(k,r)) k=get_record_keycode(r,true);
    if (selected_quantum_chain(k,r) && k<256) {
        if(r->event.pressed) add_key(k);else del_key(k);
    }
}
/* Hash all input-routing state around an actual full neighborhood paint. */
static void paint(void) {
    uint32_t layer=layer_state,rev=app_layout_revision,sync=app_layout_sync_time,gen=app_layout_visual_generation;
    bool active=app_layout_active;
    uint16_t held[62];memcpy(held,app_layout_held,sizeof(held));
    uint16_t codes[16][62];memcpy(codes,app_layout_keycodes,sizeof(codes));
    uint8_t cache[sizeof(source_layers_cache)];memcpy(cache,source_layers_cache,sizeof(cache));
    eh_screen_keymap.housekeep();lv_refr_now(NULL);
    assert(!memcmp(cache,source_layers_cache,sizeof(cache)));
    assert(layer_state==layer && app_layout_revision==rev && app_layout_sync_time==sync);
    assert(app_layout_active==active && app_layout_visual_generation==gen);
    assert(!memcmp(held,app_layout_held,sizeof(held)) && !memcmp(codes,app_layout_keycodes,sizeof(codes)));
}
static void event(uint8_t c,bool pressed) {
    uint8_t row=sm62_application_positions[c][0],col=sm62_application_positions[c][1];
    matrix[row][col]=pressed;
    assert(sm62_keymap_observe_matrix());paint();
    keyrecord_t r={.event={.key={.row=row,.col=col},.pressed=pressed,.time=now,.type=KEY_EVENT}};
    if(pre_process_record_quantum(&r))process_record(&r);
    paint();
}
static void tick(unsigned elapsed) { now+=elapsed;combo_task();tap_dance_task();paint(); }
static void keepalive(void) {
    uint8_t p[7]={EH_APP_LAYOUT_KEEPALIVE,EH_APP_LAYOUT_PROTOCOL_VERSION,1};
    memcpy(p+3,&app_layout_revision,4);assert(hid_app_layout_process_packet(p,sizeof(p)));
}
static void scenario(bool left,uint16_t output,unsigned td,unsigned gap,bool buffered) {
    side_left=left;uint8_t control=left?16:22;
    uint8_t row=sm62_application_positions[control][0],col=sm62_application_positions[control][1];
    memset(matrix,0,sizeof(matrix));memset(old_pressed,0,sizeof(old_pressed));
    memset(released_fading,0,sizeof(released_fading));
    memset(adds,0,sizeof(adds));memset(dels,0,sizeof(dels));memset(hid_keys,0,sizeof(hid_keys));
    memset(tap_dance_actions,0,sizeof(tap_dance_actions));reload_tap_dance();
    native_entries[0]=(vial_tap_dance_entry_t){.on_tap=output,.on_hold=td==2?KC_NO:output,.on_double_tap=td==3?output:KC_NO,.custom_tapping_term=200};
    app_layout_active=true;app_layout_sync_time=now;app_layout_revision=123;
    defaults[0][row][col]=KC_T;app_layout_keycodes[0][control]=td?TD(0):output;
    if(buffered) { combo_enable();test_combo.keys=(const uint16_t[]){td?TD(0):output,KC_C,COMBO_END}; }
    else combo_disable();
    force_refresh=true;eh_screen_keymap.load();paint();
    unsigned before=flushes;
    for(unsigned i=0;i<80;i++) {
        keepalive();event(control,true);tick(15);event(control,false);tick(gap);repeats++;
    }
    keepalive();tick(250);
    printf("side=%s ordinary=%u native_td=%u gap=%u combo_buffer=%u down=%u up=%u native_T=%u flushes=%u\n",left?"left":"right",output,td,gap,buffered,adds[output],dels[output],adds[KC_T],flushes-before);
    unsigned expected=td==3 && gap<200?40:80;
    assert(adds[output]==expected && dels[output]==expected && adds[KC_T]==0 && !hid_keys[output]);
    assert(!active_td && !tap_dance_actions[0].state.count && !key_buffer_size);
    assert(flushes>before);
    for(unsigned i=0;i<62;i++)app_layout_keycodes[0][i]=KC_TRNS;
    test_combo.keys=chord;
}
static void expiration_control(void) {
    combo_disable();side_left=true;
    unsigned c=16;unsigned r=sm62_application_positions[c][0],col=sm62_application_positions[c][1];
    app_layout_active=true;app_layout_sync_time=now;app_layout_keycodes[0][c]=KC_5;defaults[0][r][col]=KC_T;
    unsigned five=adds[KC_5],native=adds[KC_T];
    event(c,true);event(c,false);assert(adds[KC_5]==five+1);
    app_layout_keycodes[3][c]=KC_5;defaults[3][r][col]=KC_T;
    layer_move(3);now+=EH_APP_LAYOUT_TIMEOUT_MS;
    /* Current contract: overlay expiry selects native Default base L0. */
    force_refresh=true;eh_screen_keymap.housekeep();lv_refr_now(NULL);
    assert(!app_layout_active && layer_state==(1u<<0)); /* fixture layer_clear() encodes base as bit 0 */
    event(c,true);event(c,false);assert(adds[KC_T]==native+1);
    /* KEEPALIVE alone cannot revive an expired overlay. */
    keepalive();assert(!app_layout_active);
    app_layout_active=true;app_layout_sync_time=now;
    event(c,true);event(c,false);assert(adds[KC_5]==five+2);
    printf("PASS lease control: keepalive gap expires overlay, selects native Default L0 -> T fallback; KEEPALIVE does not reactivate\n");
    /* Hold through expiration: release remains the press-time binding. */
    event(c,true);now+=EH_APP_LAYOUT_TIMEOUT_MS;force_refresh=true;
    eh_screen_keymap.housekeep();lv_refr_now(NULL);assert(!app_layout_active);
    event(c,false);assert(!hid_keys[KC_5]);
    printf("PASS held identity: expiration during hold still releases original overlay KC_5\n");
}
int main(void) {
    setbuf(stdout,NULL);lv_init();
    static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*30];lv_disp_draw_buf_init(&buf,pixels,NULL,240*30);
    static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;drv.draw_buf=&buf;drv.flush_cb=flush;lv_disp_drv_register(&drv);
    for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++)app_layout_keycodes[l][c]=KC_TRNS;
    eh_screen_keymap.init();eh_screen_keymap.load();
    unsigned scenarios=0;
    for(unsigned s=0;s<2;s++)for(unsigned out=0;out<2;out++)for(unsigned td=0;td<4;td++)for(unsigned b=0;b<2;b++)for(unsigned g=0;g<3;g++) {
        scenario(s,out?KC_R:KC_5,td,(unsigned[]){5,60,230}[g],b);scenarios++;
    }
    expiration_control();
    printf("PASS characterization: scenarios=%u physical_event_pairs=%u actual_KEYMAP_LVGL_flushes=%u; NOT hardware PASS\n",scenarios,repeats,flushes);
}
