
typedef uint16_t matrix_row_t;
static matrix_row_t matrix_get_row(unsigned r){(void)r;return 0;}
static uint32_t timer_read32(void){return 0;}
static uint32_t via_get_layout_options(void){return 0;}
static void via_set_layout_options(uint32_t x){(void)x;assert(0);}
static void raw_hid_receive_kb(uint8_t*p,unsigned n){(void)p;(void)n;assert(0);}
static void raw_hid_send(uint8_t*p,unsigned n){assert(n==32);memcpy(reply,p,32);sent++;}
static uint16_t dynamic_keymap_get_keycode(unsigned l,unsigned r,unsigned c){(void)l;(void)r;(void)c;assert(0);return 0;}
static void dynamic_keymap_set_keycode(unsigned l,unsigned r,unsigned c,unsigned x){(void)l;(void)r;(void)c;(void)x;assert(0);}
bool dynamic_keymap_reset_checked(void){assert(0);return false;}
static void via_eeprom_set_valid(bool v){(void)v;assert(0);}
static unsigned dynamic_keymap_get_layer_count(void){return 16;}
static void dynamic_keymap_get_buffer(unsigned o,unsigned n,uint8_t*p){(void)o;(void)n;(void)p;assert(0);}
static void dynamic_keymap_set_buffer(unsigned o,unsigned n,uint8_t*p){(void)o;(void)n;(void)p;assert(0);}
static void wait_ms(unsigned x){(void)x;assert(0);}
static void bootloader_jump(void){assert(0);}
static const uint8_t keyboard_definition[32]={0};
static uint8_t vial_unlock_counter;
static bool vial_unlock_holding;
#define VIAL_RAW_EPSIZE 32
#define VIAL_KEYBOARD_UID {1,2,3,4,5,6,7,8}
#define VIAL_PROTOCOL_VERSION 6
#define VIAL_UNLOCK_NUM_KEYS 2
#define VIAL_UNLOCK_COUNTER_MAX 50
#define VIAL_TAP_DANCE_ENTRIES 0
#define VIAL_COMBO_ENTRIES 32
#define VIAL_KEY_OVERRIDE_ENTRIES 32
#define VIAL_ALT_REPEAT_KEY_ENTRIES 32
#define memcpy_P memcpy
static void vial_get_unlock_combo_coords(uint8_t*r,uint8_t*c,unsigned n){(void)r;(void)c;(void)n;assert(0);}
static void send_string(const char*p){(void)p;assert(0);}
static void send_string_with_delay(const char*p,unsigned n){assert(n==1 && strlen(p)==1);output_chars++;}
static void vial_keycode_tap(uint16_t k){(void)k;assert(0);}
static void vial_keycode_down(uint16_t k){(void)k;assert(0);}
static void vial_keycode_up(uint16_t k){(void)k;assert(0);}
#define DYNAMIC_KEYMAP_MACRO_DELAY 1

