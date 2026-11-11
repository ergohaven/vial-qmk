/* Native hardware boundary only; settings and physical-side policy are production. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define KEYBOARD_ergohaven_sm62
#define KB_SETTINGS_SM62_SIZE 18
#define KB_SETTINGS_LCD_SIZE 3
/* Native EEPROM placement, not MCU addresses. */
#define KB_SETTINGS_SM62_OFFSET 32
#define KB_SETTINGS_LCD_OFFSET 29
#define RPC_SM62_SETTINGS 3
static uint32_t now;
static bool left, master;
static unsigned writes, sends;
static uint8_t eeprom[128];
static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t t) { return now - t; }
static uint32_t sync_timer_read32(void) { return now; }
static bool is_keyboard_left(void) { return left; }
static bool is_keyboard_master(void) { return master; }
static bool sm62_flash_begin(void) { return true; }
static bool sm62_flash_end(void) { return true; }
static void eeconfig_read_kb_datablock(void *p, unsigned o, size_t n) {
    assert(o + n <= sizeof(eeprom)); memcpy(p, eeprom + o, n);
}
static void eeconfig_update_kb_datablock(const void *p, unsigned o, size_t n) {
    assert(o + n <= sizeof(eeprom)); memcpy(eeprom + o, p, n); ++writes;
}
static void transaction_register_rpc(int id, void (*cb)(uint8_t, const void *, uint8_t, void *)) {
    assert(id == RPC_SM62_SETTINGS && cb);
}
static bool transaction_rpc_send(int id, uint8_t n, const void *p) {
    assert(id == RPC_SM62_SETTINGS && n && p); ++sends; return true;
}
typedef struct { uint16_t qsid; } qmk_settings_proto_t;
