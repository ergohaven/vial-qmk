int main(int argc,char **argv) {
    assert(argc==5);setbuf(stdout,NULL);
    off=atoi(argv[1]);block_cost=atoi(argv[2]);split_cost=atoi(argv[3]);release_at=atoi(argv[4]);
    unsigned initial_split_cost=split_cost;
    now=90;combo_disable();lv_init();
    static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*4];
    lv_disp_draw_buf_init(&buf,pixels,NULL,240*4);
    static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;
    drv.draw_buf=&buf;drv.flush_cb=qp_lvgl_flush;lv_disp_drv_register(&drv);
    eh_screen_keymap.init();eh_screen_keymap.load();current_screen=eh_screen_keymap;active_screen=SM62_SCREEN_KEYMAP;
    lv_refr_now(NULL);flushes=0; /* settle startup dirt before matched steady-state windows */
    startup_display_released=true;display_ready_at=90;side_left=true;
    unsigned control=16,row=sm62_application_positions[control][0],col=sm62_application_positions[control][1];
    printf("fixture control=%u row=%u col=%u physical_press=100 physical_release=%u term=200 debounce=10 off=%u spi_cost=%u split_cost=%u\n",control,row,col,release_at,off,block_cost,split_cost);
    assert(row==1 && col==2);
    for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++)app_layout_keycodes[l][c]=KC_TRNS;
    app_layout_active=true;app_layout_revision=123;app_layout_keycodes[0][control]=TD(0);defaults[0][row][col]=KC_T;
    native_entries[0]=(vial_tap_dance_entry_t){.on_tap=KC_5,.on_hold=KC_T,.on_double_tap=KC_R,.custom_tapping_term=200};
    reload_tap_dance();debounce_init(MATRIX_ROWS/2);
    while(now<600) {
        app_layout_sync_time=now; /* Fresh lease control, not modeled host traffic. */
        replay_iteration();
        if(adds[KC_T] && !hold_at)hold_at=now;
        if(adds[KC_5] && !tap_at)tap_at=now;
        now++;
    }
    printf("RESULT tap=%u hold=%u double=%u raw_release=%u cooked_release=%u hold_observed=%u tap_observed=%u max_scan_gap=%u flushes=%u\n",adds[KC_5],adds[KC_T],adds[KC_R],raw_release_at,cooked_release_at,hold_at,tap_at,max_scan_gap,flushes);
    /* Candidate post-flush scan observes release one tick earlier than the
     * archived baseline: 181 is now TAP; 182+ remains unbounded-stall HOLD. */
    bool expected_hold=(initial_split_cost>=181 || (!off && block_cost>=182));
    if(getenv("EXPECT_TAP"))assert(adds[KC_5]==1 && adds[KC_T]==0);
    assert(adds[KC_T]==expected_hold && adds[KC_5]==!expected_hold && adds[KC_R]==0);
    assert(dels[KC_T]==adds[KC_T] && dels[KC_5]==adds[KC_5]);
    assert(!active_td && !tap_dance_actions[0].state.count && app_layout_active);
    assert(cooked_release_at>=raw_release_at && raw_release_at>=release_at);
    debounce_free();
    return 0;
}
