/* Hardware/native data boundaries only. Actual owner, policies, screens, QP,
 * LVGL, deferred executor, exchange and flash protocol are compiled separately. */
static _Thread_local bool owner;
static uint32_t now=3000; static bool left,master;
static hid_data_t native_hid; static sm62_side_settings_t settings[2];
static uint8_t rows[10]; static char names[16][23];
static uint32_t pixels,flushes; static bool bus,fail_pixel;
static uint16_t panel[280][240], reference[280][240], completed[280][240];
static unsigned coverage[280][240], monitors; static uint32_t completed_tick; static int completed_style,completed_coord;
extern int diag_title_style(void),diag_title_coord(void);
static void frame_monitor(lv_disp_drv_t *d,uint32_t time,uint32_t px){memcpy(completed,panel,sizeof(panel));completed_tick=lv_tick_get();completed_style=diag_title_style();completed_coord=diag_title_coord();monitors++;}
static int vx1,vy1,vx2,vy2; static unsigned cursor;
static bool persistent_error,freeze_pixel_clock; static unsigned init_failures=2, oracle_checks;

static bool hold_panel, panel_entered, panel_release;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
static uint8_t reply[32]; static unsigned backlights;
extern void sm62_native_run_scanner(unsigned test,unsigned slot);
extern void sm62_native_memory_report(void);
void sm62_native_scan_edge(uint8_t r,uint8_t c,bool pressed){
 assert(!owner);if(!left)r+=5;
 if(pressed)rows[r]|=1U<<c;else rows[r]&=~(1U<<c);
 sm62_render_input(r,c,pressed);sm62_render_capture();
}
bool sm62_render_test_owner(void){return owner;}
void sm62_render_test_lock(void){assert(!owner);}
void sm62_render_test_unlock(void){assert(!owner);}
uint32_t timer_read32(void){return __atomic_load_n(&now,__ATOMIC_ACQUIRE);}
uint32_t timer_elapsed32(uint32_t t){return timer_read32()-t;}
uint32_t sm62_dc_test_us(void){return timer_read32()*1000U;}
bool is_keyboard_left(void){assert(!owner);return left;}
bool is_keyboard_master(void){assert(!owner);return master;}
uint8_t get_current_layer(void){assert(!owner);return 0;}
uint8_t get_cur_lang(void){assert(!owner);return 0;}
hid_data_t *get_hid_data(void){assert(!owner);return &native_hid;}
bool is_hid_active(void){assert(!owner);return true;}
bool is_hid_layout_valid(void){assert(!owner);return true;}
bool is_hid_time_active(void){assert(!owner);return true;}
bool is_hid_volume_active(void){assert(!owner);return true;}
uint8_t matrix_get_row(uint8_t r){assert(!owner);return rows[r];}
bool matrix_is_on(uint8_t r,uint8_t c){return matrix_get_row(r)&(1U<<c);}
uint16_t hid_sm62_default_keycode(uint8_t l,uint8_t r,uint8_t c){assert(!owner);return KC_A+(l+r+c)%26;}
uint16_t sm62_effective_keycode(uint8_t l,uint8_t r,uint8_t c){return hid_sm62_default_keycode(l,r,c);}
const char *sm62_effective_layer_name(uint8_t l){assert(!owner);return names[l];}
const char *layer_name(uint8_t l){return sm62_effective_layer_name(l);}
bool hid_app_layout_get_visual(uint8_t l,uint8_t c,uint8_t *v,uint8_t *s){assert(!owner);*v=0;*s=0;return false;}
uint32_t hid_app_layout_visual_generation(void){assert(!owner);return 1;}
const sm62_side_settings_t *sm62_local_display_settings(void){assert(!owner);return &settings[0];}
const sm62_side_settings_t *sm62_remote_display_settings(void){assert(!owner);return &settings[1];}
bool sm62_local_date_enabled(void){assert(!owner);return true;}
bool sm62_local_language_enabled(void){assert(!owner);return true;}
bool sm62_local_stars_enabled(void){assert(!owner);return true;}
bool sm62_local_screens_off(void){assert(!owner);return false;}
bool sm62_local_mode_transition_pending(void){assert(!owner);return false;}
uint16_t sm62_local_keymap_hold_ms(void){assert(!owner);return 100;}
uint32_t sm62_local_orbit_idle_ms(uint32_t idle){assert(!owner);return idle;}
uint32_t last_matrix_activity_time(void){assert(!owner);return 3000;}
uint32_t last_matrix_activity_elapsed(void){assert(!owner);return 0;}
uint32_t sm62_display_boot_elapsed(void){assert(!owner);return 3000;}
uint32_t get_split_lcd_timeout_ms(void){assert(!owner);return 0;}
uint8_t get_split_lcd_brightness(void){assert(!owner);return 8;}
uint8_t get_display_accent_red(void){assert(!owner);return 255;}
uint8_t get_display_accent_green(void){assert(!owner);return 80;}
uint8_t get_display_accent_blue(void){assert(!owner);return 20;}
uint8_t get_display_background_red(void){assert(!owner);return 0;}
uint8_t get_display_background_green(void){assert(!owner);return 0;}
uint8_t get_display_background_blue(void){assert(!owner);return 0;}
void backlight_init(void){assert(!owner);}
void backlight_level_noeeprom(uint8_t l){assert(!owner);backlights++;}
void sm62_diag_stage(uint8_t s){}
void sm62_diag_star(void){}
static painter_driver_t device={.validate_ok=true,.panel_width=240,.panel_height=280};
painter_device_t qp_devices[1]={&device};
painter_device_t qp_st7789_make_spi_device(unsigned w,unsigned h,unsigned cs,unsigned dc,unsigned rst,unsigned div,unsigned mode){assert(owner);return &device;}
void qp_set_viewport_offsets(painter_device_t d,unsigned x,unsigned y){assert(owner);}
bool qp_init(painter_device_t d,unsigned rotation){assert(owner);if(init_failures){init_failures--;return false;}return true;}
bool qp_power(painter_device_t d,bool on){assert(owner);return true;}
bool qp_comms_spi_transaction_begin(painter_device_t d){assert(owner&&!bus);bus=true;return true;}
bool qp_comms_spi_transaction_end(painter_device_t d){assert(owner&&bus);bus=false;return true;}
bool qp_viewport(painter_device_t d,int x1,int y1,int x2,int y2){assert(owner&&bus);assert(x1>=0&&y1>=0&&x2<240&&y2<280);vx1=x1;vy1=y1;vx2=x2;vy2=y2;cursor=0;return true;}
bool qp_pixdata(painter_device_t d,const void *p,uint32_t n){
 assert(owner&&bus);pixels+=n;if(!freeze_pixel_clock)__atomic_add_fetch(&now,1,__ATOMIC_RELEASE);
 pthread_mutex_lock(&gate);
 if(hold_panel){panel_entered=true;pthread_cond_broadcast(&cond);while(!panel_release)pthread_cond_wait(&cond,&gate);hold_panel=false;}
 pthread_mutex_unlock(&gate);
 if(fail_pixel||persistent_error){fail_pixel=false;return false;}
 const uint16_t *bytes=p;unsigned width=vx2-vx1+1;
 assert(cursor+n<=width*(unsigned)(vy2-vy1+1));
 for(unsigned i=0;i<n;i++,cursor++){panel[vy1+cursor/width][vx1+cursor%width]=bytes[i];coverage[vy1+cursor/width][vx1+cursor%width]++;}
 return true;
}
bool qp_flush(painter_device_t d){assert(owner);flushes++;return true;}
void qp_get_geometry(painter_device_t d,uint16_t*w,uint16_t*h,void*r,uint16_t*x,uint16_t*y){*w=240;*h=280;*x=*y=0;}
void qp_internal_animation_tick(void){}
bool sm62_spi_quiescent(void){assert(owner);return !bus;}
void raw_hid_send(uint8_t*d,uint8_t n){assert(!owner&&n==32);memcpy(reply,d,32);}
/* Flash guard native boundary: no flash operations in this renderer fixture. */
uint32_t sm62_flash_test_core(void){return owner?1:0;}
bool sm62_flash_test_irqs_enabled(void){return true;}
bool sm62_flash_test_in_handler(void){return false;}
bool sm62_flash_test_context_valid(void){return !owner;}
uint32_t sm62_flash_test_time_us(void){return sm62_dc_test_us();}
void sm62_flash_test_relax(void){sched_yield();}
void sm62_flash_test_irq_disable(void){}
void sm62_flash_test_irq_restore(void){}
static void step(void){owner=true;sm62_render_worker_step();owner=false;}
/* Up to 70 four-row parts per full frame, 4ms cooperative slices; permit
 * several complete frames including a failed frame + mandatory repaint. */
static void settle(void){for(unsigned i=0;i<200;i++){__atomic_add_fetch(&now,5,__ATOMIC_RELEASE);step();}if(!persistent_error){uint32_t f=sm62_render_frames();for(unsigned i=0;i<200&&sm62_render_frames()==f;i++){now+=5;step();}}}
static void *render(void*p){owner=true;sm62_render_worker_step();owner=false;return NULL;}

/* Compare the panel's accumulated hardware writes with a fresh complete repaint
 * of the SAME real LVGL scene, without advancing any timers/producers. This is
 * a recovery/composition oracle, not historical firmware pixel parity. */
static void dump_panel(const char *name,const void *data){char path[512];snprintf(path,sizeof(path),"%s/%u-%u-%u-%s.bin",getenv("SM62_DIAG_OUT"),left,master,oracle_checks,name);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,sizeof(panel),f)==sizeof(panel));fclose(f);}
extern int diag_title_style(void),diag_title_coord(void);
static void panel_oracle(const char *label){
 owner=true;
 /* The actual production frame must have reached its final hardware write.
  * No extra producer/timer callback may be inserted before taking actual. */
 while(qp_lvgl_refr_pending()) {now+=5;qp_internal_task();}
 lv_disp_t *d=lv_disp_get_default();
 printf("EXACT %s frames=%u monitors=%u tick=%u completion_tick=%u inv=%u pending=%u style=%d completion_style=%d coord=%d completion_coord=%d\n",label,sm62_render_frames(),monitors,lv_tick_get(),completed_tick,d->inv_p,qp_lvgl_refr_pending(),diag_title_style(),completed_style,diag_title_coord(),completed_coord);
 assert(monitors&&d->inv_p==0&&!qp_lvgl_refr_pending());
 assert(!memcmp(completed,panel,sizeof(panel)));
 if(sm62_diag_screen()==1)assert(diag_title_style()==completed_style&&diag_title_coord()==completed_coord);
 memcpy(reference,panel,sizeof(panel));dump_panel("actual",panel);
 uint32_t tick=lv_tick_get();int style=diag_title_style(),coord=diag_title_coord();
 bool (*slice)(void)=d->driver->refr_slice_cb;
 freeze_pixel_clock=true;
 for(unsigned control=0;control<2;control++){
  /* Both controls redraw every pixel, including untouched background. No
   * lv_refr_now here: its documented implementation executes animations. */
  memset(panel,0xA5,sizeof(panel));memset(coverage,0,sizeof(coverage));
  if(control)d->driver->refr_slice_cb=NULL;
  lv_obj_invalidate(lv_scr_act());_lv_disp_refr_timer(d->refr_timer);
  for(unsigned i=0;qp_lvgl_refr_pending()&&i<1000;i++)qp_lvgl_internal_tick();
  assert(!qp_lvgl_refr_pending());
  assert(lv_tick_get()==tick&&diag_title_style()==style);if(sm62_diag_screen()==1)assert(diag_title_coord()==coord);
  for(unsigned y=0;y<280;y++)for(unsigned x=0;x<240;x++)assert(coverage[y][x]);
  dump_panel(control?"synchronous-expected":"cooperative-expected",panel);
  if(memcmp(reference,panel,sizeof(panel))){unsigned n=0;for(unsigned y=0;y<280;y++)for(unsigned x=0;x<240;x++)n+=reference[y][x]!=panel[y][x];fprintf(stderr,"EXACT_MISMATCH %s control=%u pixels=%u\n",label,control,n);fflush(stdout);abort();}
  printf("ORACLE %s control=%s bytes=%zu equal\n",label,control?"synchronous":"cooperative",sizeof(panel));
  d->driver->refr_slice_cb=slice;
 }
 freeze_pixel_clock=false;owner=false;oracle_checks++;
}
static void populate(void){
 native_hid.hours=12;native_hid.minutes=34;native_hid.year=2026;native_hid.month=11;native_hid.day=3;native_hid.date_valid=true;
 native_hid.date_day=3;native_hid.date_month=11;native_hid.date_year=2026;
 native_hid.volume=67;native_hid.layout=1;
 strcpy(native_hid.media_title,"Populated production PLAYER title long enough to scroll across the panel repeatedly");
 strcpy(native_hid.media_artist,"Production artist");
 native_hid.media_position_seconds=42;native_hid.media_duration_seconds=260;native_hid.media_progress_time=now;native_hid.media_progress_valid=true;
 native_hid.time_changed=native_hid.date_changed=native_hid.volume_changed=native_hid.layout_changed=native_hid.media_title_changed=native_hid.media_artist_changed=true;
 for(unsigned i=0;i<16;i++)snprintf(names[i],23,"CUSTOM layer %u",i);
}

unsigned long probe_counts[6], probe_panel_flushes;
static void measure(const char *label, bool event, unsigned col, bool pressed){
 unsigned long before[6]; memcpy(before,probe_counts,sizeof(before));
 unsigned long pf=probe_panel_flushes;
 uint32_t f=flushes,p=pixels,m=monitors;unsigned calls=0;
 if(event){sm62_native_scan_edge(0,col,pressed);}else{
  owner=true;lv_obj_invalidate(lv_scr_act());owner=false;
 }
 for(unsigned i=0;i<400;i++){
  step();calls++;
  if(monitors>m&&!qp_lvgl_refr_pending())break;
  /* Deterministic host clock for scheduling; not physical milliseconds. */
  now+=5;
 }
 assert(monitors==m+1&&!qp_lvgl_refr_pending());
 printf("MEASURE {\"label\":\"%s\",\"flushes\":%u,\"pixels\":%u,\"completed_refreshes\":%u,\"worker_steps\":%u,\"functions\":[",label,flushes-f,pixels-p,monitors-m,calls);
 for(unsigned i=0;i<6;i++)printf("%s%lu",i?",":"",probe_counts[i]-before[i]);
 printf("]}\n");
 char path[1024];printf("PANEL %lu\n",probe_panel_flushes-pf);snprintf(path,sizeof(path),"%s/%s-%u-%u.bin",getenv("SM62_DIAG_OUT"),label,left,master);
 FILE*out=fopen(path,"wb");assert(out);assert(fwrite(panel,1,sizeof(panel),out)==sizeof(panel));fclose(out);
}
int main(int argc,char**argv){
 setbuf(stdout,NULL);assert(argc==3);left=atoi(argv[1]);master=atoi(argv[2]);
 settings[0].mode=3;settings[0].keymap_enabled=1;populate();
 sm62_render_ready();settle();assert(init_failures==0&&sm62_render_enabled());
 assert(sm62_diag_screen()==2);
 lv_disp_get_default()->driver->monitor_cb=frame_monitor;
 assert(lv_disp_get_default()->driver->draw_buf->size==240*QP_LVGL_BUFFER_ROWS);
 assert(lv_disp_get_default()->driver->refr_slice_cb);
 now=10000;measure("full-keymap",false,0,false);
 now=20000;measure("press-0-0",true,0,true);
 now=30000;measure("press-0-1-focus",true,1,true);
 printf("PASS rows=%u side=%u role=%u\n",QP_LVGL_BUFFER_ROWS,left,master);return 0;
}
