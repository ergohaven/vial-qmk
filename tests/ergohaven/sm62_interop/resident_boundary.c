
/* Diagnostic inspection only; never used to manufacture protocol replies. */
unsigned bridge_resident_key(unsigned l,unsigned c){assert(l<16&&c<62);return app_layout_keycodes[l][c];}
unsigned bridge_resident_visual(unsigned l,unsigned c){assert(l<16&&c<62);return app_layout_visuals[l][c];}
unsigned bridge_resident_active(void){return app_layout_active;}

void bridge_resident_tick(void){hid_app_layout_task();}
