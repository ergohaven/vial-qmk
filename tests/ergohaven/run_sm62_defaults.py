#!/usr/bin/env python3
"""Compile production SM62 settings with fake EEPROM/timer/split hardware.

No resets on hardware. --source replays the same assertions on a frozen tree.
"""
import argparse
import re
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
root = a.source.resolve()

def stripped(path):
    return re.sub(r'^#(?:include[^\n]*|pragma once)', '', (root / path).read_text(), flags=re.M)

pre = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define KB_SETTINGS_SM62_SIZE 18
#define KB_SETTINGS_LCD_SIZE 3
#define KB_SETTINGS_SM62_OFFSET 32
#define KB_SETTINGS_LCD_OFFSET 29
#define RPC_SM62_SETTINGS 3
typedef struct {uint16_t qsid;} qmk_settings_proto_t;
static uint8_t eeprom[128];
static unsigned writes;
static uint32_t now;
static uint8_t side;
static uint32_t timer_read32(void) {return now;}
static uint32_t sync_timer_read32(void) {return now;}
static uint32_t timer_elapsed32(uint32_t t) {return now-t;}
static uint8_t sm62_local_side_index(void) {return side;}
static bool is_keyboard_master(void) {return false;}
static void eeconfig_read_kb_datablock(void *p, unsigned offset, size_t n) {
    assert(offset+n<=sizeof(eeprom));memcpy(p,eeprom+offset,n);
}
static void eeconfig_update_kb_datablock(const void *p, unsigned offset, size_t n) {
    assert(offset+n<=sizeof(eeprom));memcpy(eeprom+offset,p,n);writes++;
}
static void transaction_register_rpc(int id,void(*cb)(uint8_t,const void*,uint8_t,void*)) {(void)id;(void)cb;}
static bool transaction_rpc_send(int id,uint8_t n,const void *p) {(void)id;(void)n;(void)p;return true;}
'''
test = r'''
static unsigned failures;
static void check(bool ok, const char *name) {
    printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok;
}
static uint16_t get_hold(uint8_t i) {
    qmk_settings_proto_t p={242+i};uint8_t data[2]={0};
    assert(sm62_setting_get(&p,data,2)==0);
    return data[0]|((uint16_t)data[1]<<8);
}
static void set_hold(uint8_t i,uint16_t value) {
    qmk_settings_proto_t p={242+i};uint8_t data[2]={value&255,value>>8};
    assert(sm62_setting_set(&p,data,2)==0);
}
int main(void) {
    memset(eeprom,0xff,sizeof(eeprom));kb_settings_extra_init();
    check(get_hold(0)==350 && get_hold(1)==350,"blank EEPROM initializes both QSIDs to 350 milliseconds");
    for(side=0;side<2;side++)
        check(sm62_local_keymap_hold_ms()==350,"local display consumes milliseconds unchanged");
    unsigned before=writes;kb_settings_extra_init();
    check(writes==before,"fresh-record reload does not write EEPROM");
    /* Existing users, including an old default of 300, retain their values.
     * Full EEPROM comparison also guards unrelated display flags and bytes. */
    const uint16_t values[][2]={{125,2500},{0,10000},{300,300},{200,350}};
    for(unsigned version=3;version<=5;version++) {
        for(unsigned n=0;n<sizeof(values)/sizeof(values[0]);n++) {
            saved.version=version;saved.side[0].mode=1;saved.side[1].mode=3;
            saved.date_enabled[0]=0;saved.date_enabled[1]=1;
            saved.keymap_hold_ms[0]=values[n][0];saved.keymap_hold_ms[1]=values[n][1];
            persist();uint8_t expected[sizeof(eeprom)];memcpy(expected,eeprom,sizeof(expected));
            expected[KB_SETTINGS_SM62_OFFSET+1]=5;before=writes;kb_settings_extra_init();
            check(get_hold(0)==values[n][0] && get_hold(1)==values[n][1],"v3-v5 stored hold values preserved on init");
            check(memcmp(eeprom,expected,sizeof(eeprom))==0,"only legacy version byte may migrate; unrelated EEPROM preserved");
            check(writes==before+(version<5?2:0),"current records never rewritten; legacy migration only once");
            before=writes;kb_settings_extra_init();check(writes==before,"migrated record reload idempotent");
        }
    }
    for(unsigned version=1;version<=2;version++) {
        saved.version=version;saved.keymap_hold_ms[0]=0xffff;saved.keymap_hold_ms[1]=0xffff;
        persist();kb_settings_extra_init();
        check(get_hold(0)==350 && get_hold(1)==350,"v1-v2 absent hold fields initialize to 350 milliseconds");
        check(saved.side[0].mode==1 && saved.side[1].mode==3,"old-format extension preserves unrelated modes");
    }
    before=writes;set_hold(0,175);set_hold(1,925);
    check(get_hold(0)==175 && get_hold(1)==925,"explicit setters and GET remain independently configurable");
    check(writes==before,"SET does not synchronously write flash");
    now+=4000;sm62_settings_sync_task();kb_settings_extra_init();
    check(get_hold(0)==175 && get_hold(1)==925,"explicit user edits persist and reload without default enforcement");
    printf("failures=%u\n",failures);return failures?1:0;
}
'''
src = a.output / 'settings_probe.c'
src.write_text(pre + stripped('keyboards/ergohaven/sm62/sm62_settings.h') + stripped('keyboards/ergohaven/sm62/sm62_settings.c') + test)
exe = a.output / 'settings_probe'
subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',str(src),'-o',str(exe)],check=True)
r = subprocess.run([str(exe.resolve())],capture_output=True,text=True)
(a.output/'results.log').write_text(r.stdout+r.stderr)
print(r.stdout+r.stderr,end='')
raise SystemExit(r.returncode)
