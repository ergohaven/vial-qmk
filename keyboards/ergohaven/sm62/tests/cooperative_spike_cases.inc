int main(int argc,char **argv) {
    assert(argc==10);setbuf(stdout,NULL);
    off=atoi(argv[1]);yield_enabled=atoi(argv[2]);strip_rate=atoi(argv[3]);singleton=atoi(argv[4]);block_cost=strip_rate;
    block_at=atoi(argv[5]);release_at=atoi(argv[6]);pulse2=atoi(argv[7]);cpu_cost=atoi(argv[8]);ready=atoi(argv[9]);
    now=90;combo_disable();lv_init();
    static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*28];
    lv_disp_draw_buf_init(&buf,pixels,NULL,240*28);
    static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=280;
    drv.draw_buf=&buf;drv.flush_cb=qp_lvgl_flush;lv_disp_drv_register(&drv);
    eh_screen_keymap.init();eh_screen_keymap.load();current_screen=eh_screen_keymap;active_screen=SM62_SCREEN_KEYMAP;
    lv_refr_now(NULL);flushes=0;yield_count=0;
    startup_display_released=true;display_ready_at=90;side_left=true;
    unsigned control=16,row=sm62_application_positions[control][0],col=sm62_application_positions[control][1];
    assert(row==1 && col==2);
    for(unsigned l=0;l<16;l++)for(unsigned c=0;c<62;c++)app_layout_keycodes[l][c]=KC_TRNS;
    app_layout_active=true;app_layout_revision=123;app_layout_keycodes[0][control]=TD(0);defaults[0][row][col]=KC_T;
    native_entries[0]=(vial_tap_dance_entry_t){.on_tap=KC_5,.on_hold=KC_T,.on_double_tap=KC_R,.custom_tapping_term=200};
    reload_tap_dance();debounce_init(MATRIX_ROWS/2);
    while(now<900) {
        app_layout_sync_time=now; /* Fresh lease control: NOT real host traffic. */
        replay_iteration();now++;
    }
    printf("RESULT off=%u yield=%u strip_rate=%u singleton=%u start=%u release=%u pulse2=%u tap=%u hold=%u double=%u max_scan_gap=%u raw_release=%u cooked_release=%u yields=%u px=%u modeled_ms=%u recent=%u recent_row=%u recent_col=%u\n",off,yield_enabled,strip_rate,singleton,block_at,release_at,pulse2,adds[KC_5],adds[KC_T],adds[KC_R],max_scan_gap,raw_release_at,cooked_release_at,yield_count,charged_pixels,charged_ms,recent_press_valid,recent_press_row,recent_press_col);
    assert(dels[KC_T]==adds[KC_T] && dels[KC_5]==adds[KC_5] && dels[KC_R]==adds[KC_R]);
    assert(!active_td && !tap_dance_actions[0].state.count && app_layout_active && !bus_locked && !yielding);
    if(!ready || !yield_enabled)assert(yield_count==0);
    const char *expect=getenv("SPIKE_EXPECT");
    if(expect && !strcmp(expect,"tap"))assert(adds[KC_5]==1 && !adds[KC_T] && !adds[KC_R]);
    if(expect && !strcmp(expect,"double"))assert(adds[KC_R]==1 && !adds[KC_T] && !adds[KC_5]);
    if(expect && !strcmp(expect,"neighborhood"))assert(recent_press_row==row && recent_press_col==col);
    debounce_free();return 0;
}
