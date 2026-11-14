static void process_record(keyrecord_t *r) {
    uint16_t k=get_record_keycode(r,true);
    if (preprocess_tap_dance(k,r)) k=get_record_keycode(r,true);
    if (selected_quantum_chain(k,r) && k < 256) {
        if(r->event.pressed) add_key(k); else del_key(k);
    }
}
static void event(uint8_t control, bool pressed) {
    keyrecord_t r={.event={.key={.row=sm62_application_positions[control][0],.col=sm62_application_positions[control][1]},.pressed=pressed,.time=now,.type=KEY_EVENT}};
    if(pre_process_record_quantum(&r)) process_record(&r);
}
static void drain(void) { now+=300;combo_task();tap_dance_task(); }
static void deactivate(void) {
    uint8_t p[32]={EH_APP_LAYOUT_DEACTIVATE,EH_APP_LAYOUT_PROTOCOL_VERSION};
    assert(hid_app_layout_process_packet(p,sizeof(p)));
}
int main(int argc,char **argv) {
    assert(argc==2);
    setbuf(stdout,NULL);
    app_layout_active=true;app_layout_sync_time=now;app_layout_revision=123;
    for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++)app_layout_keycodes[l][c]=KC_TRNS;
    app_layout_keycodes[0][0]=KC_A;app_layout_keycodes[0][1]=KC_C;
    uint8_t row=sm62_application_positions[0][0],col=sm62_application_positions[0][1];
    defaults[0][row][col]=KC_X;
    if(!strcmp(argv[1],"override")) {
        mods=2;event(0,true);drain();
        printf("override replacement B=%u direct=%u via=%u\n",adds[KC_B],direct_downs,via_calls);
        assert(adds[KC_B]==1 && !keys[KC_A]);
        event(0,false);assert(!keys[KC_B] && active_override==NULL);
    } else if(!strcmp(argv[1],"td")) {
        tap_dance_pair_t pair={KC_E,KC_F};
        dance=(tap_dance_action_t){.fn={tap_dance_pair_on_each_tap,tap_dance_pair_finished,tap_dance_pair_reset,NULL},.user_data=&pair};
        app_layout_keycodes[0][0]=TD(0);
        event(0,true);assert(dance.state.pressed && dance.state.count==1);
        deactivate();event(0,false);drain();
        assert(adds[KC_E]==1 && dels[KC_E]==1 && !dance.state.count && !active_td);
    } else if(!strcmp(argv[1],"combo_replay")) {
        mods=2;event(0,true);assert(key_buffer_size==1 && adds[KC_B]==0);
        app_layout_keycodes[0][0]=KC_Z;app_layout_revision=999;deactivate();
        drain();assert(adds[KC_B]==1);
        assert(app_layout_events[app_layout_event_head].revision==123);
        event(0,false);assert(!keys[KC_B] && active_override==NULL);
    } else if(!strcmp(argv[1],"combo_match")) {
        event(0,true);event(1,true);drain();assert(adds[KC_D]==1 && adds[KC_A]==0 && adds[KC_C]==0);
        layer_move(1);app_layout_keycodes[1][0]=KC_Z;app_layout_keycodes[1][1]=KC_Y;
        event(0,false);event(1,false);drain();assert(!test_combo.active && dels[KC_D]==1);
    } else if(!strcmp(argv[1],"held_transition")) {
        combo_disable();event(0,true);assert(keys[KC_A]);
        deactivate();app_layout_keycodes[0][0]=KC_Z;assert(keys[KC_A]);
        event(0,false);assert(!keys[KC_A] && dels[KC_A]==1 && adds[KC_Z]==0);
    } else if(!strcmp(argv[1],"fallback_transition")) {
        combo_disable();app_layout_active=false;event(0,true);assert(keys[KC_X]);
        app_layout_active=true;app_layout_sync_time=now;event(0,false);assert(!keys[KC_X] && adds[KC_A]==0);
    } else if(!strcmp(argv[1],"no_key")) {
        combo_disable();app_layout_keycodes[0][0]=KC_NO;event(0,true);event(0,false);
        assert(adds[KC_X]==0 && adds[KC_A]==0);
    } else assert(false);
    assert(direct_downs==0 && direct_ups==0);
    printf("PASS %s\n",argv[1]);
}
