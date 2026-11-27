"""Production mode handlers with simulated clock/settings; no hardware or firmware build."""
import os
from pathlib import Path
import subprocess
import unittest
from test_phenom_encoder_step_only import function, ROOT, SOURCE

class NormalActions(unittest.TestCase):
    def test_runtime(self):
        source = SOURCE.read_text()
        header = SOURCE.with_suffix('.h').read_text()
        enum = header[header.index('\nenum {') + 1:header.index('\n};') + 3]
        # Candidate wire IDs let RED execute the existing handler, not fail compilation.
        enum = enum.replace('    // Append after', '    // Append after')
        prefix = '''#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define EH_KEYBOARD_SPLIT_POINTING_V2
#define QK_USER 0x7e40
#define EH_RSRV2 0x7e33
#define EH_RSRV7 0x7e38
typedef enum { POINTING_MODE_NORMAL, POINTING_MODE_SNIPER, POINTING_MODE_SCROLL, POINTING_MODE_TEXT, POINTING_MODE_USR1, POINTING_MODE_USR2, POINTING_MODE_USR3 } pointing_mode_t;
typedef enum { SPLIT_POINTING_SIDE_LEFT, SPLIT_POINTING_SIDE_RIGHT, SPLIT_POINTING_SIDE_COUNT } split_pointing_side_t;
typedef struct { struct { bool pressed; } event; } keyrecord_t;
static struct { uint8_t mode[2]; bool sticky_mode[2]; unsigned sentinel; } kb_settings_phenom_devices;
static unsigned writes, now;
static bool get_split_pointing_side_sticky_mode(unsigned side) { return kb_settings_phenom_devices.sticky_mode[side]; }
static void set_split_pointing_side_sticky_mode(unsigned side, bool value) { writes++; kb_settings_phenom_devices.sticky_mode[side] = value; }
static uint16_t timer_read(void) { return now; }
static uint16_t timer_elapsed(uint16_t start) { return now - start; }
static unsigned get_tapping_term(uint16_t k, keyrecord_t *r) { (void)k; (void)r; return 200; }
static bool is_mouse_record_user(uint16_t k, keyrecord_t *r) { (void)k; (void)r; return false; }
'''
        runtime = source[source.index('#ifdef EH_KEYBOARD_SPLIT_POINTING_V2\n// split-mode-restore'):source.index('\npointing_mode_t get_split_pointing_side_mode')]
        production = '\n'.join(function(source, n) for n in ('get_split_pointing_side_mode', 'process_pointing_sticky_key', 'process_record_pointing', 'is_mouse_record_kb'))
        cases = r'''
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static bool key(unsigned k, bool down) { keyrecord_t r = {.event.pressed=down}; return process_record_pointing(k,&r); }
static void reset(unsigned base, bool sticky) { for(unsigned s=0;s<2;s++) { kb_settings_phenom_devices.mode[s]=base; kb_settings_phenom_devices.sticky_mode[s]=sticky; split_pointing_mode_override_active[s]=false; } writes=0; now+=1000; }
int main(void) {
#ifdef KEYBOARD_ergohaven_hpd_rev3
 reset(3,false);
 for(unsigned k=0x7e49;k<=0x7e4b;k++) { CHECK(key(k,true)); CHECK(key(k,false)); keyrecord_t r={0}; CHECK(!is_mouse_record_kb(k,&r)); }
 CHECK(get_split_pointing_side_mode(0)==3 && get_split_pointing_side_mode(1)==3 && writes==0);
 puts("PASS HPD rev3 does not implement Phenom Normal actions"); return 0;
#endif
 const unsigned normals[]={0x7e49,0x7e4a,0x7e4b};
 for(unsigned n=0;n<3;n++) for(unsigned base=0;base<4;base++) for(unsigned sticky=0;sticky<2;sticky++) for(unsigned prior=0;prior<4;prior++) {
  reset(base,sticky);
  if(prior) for(unsigned s=0;s<2;s++) set_split_pointing_side_mode_override(s,prior);
  unsigned before[2]={get_split_pointing_side_mode(0),get_split_pointing_side_mode(1)};
  CHECK(!key(normals[n],true)); CHECK(!key(normals[n],false));
  for(unsigned s=0;s<2;s++) CHECK(get_split_pointing_side_mode(s)==((n==1 || n==s*2)?0:before[s]));
  CHECK(writes==0); CHECK(kb_settings_phenom_devices.mode[0]==base && kb_settings_phenom_devices.mode[1]==base);
  keyrecord_t r={0}; CHECK(is_mouse_record_kb(normals[n],&r));
 }
 const unsigned older[]={EH_SNP,EH_SCR,EH_TXT,EH_L_SNP,EH_L_SCR,EH_L_TXT,EH_USR1,EH_USR2,EH_USR3};
 for(unsigned n=0;n<3;n++) for(unsigned k=0;k<9;k++) for(unsigned sticky=0;sticky<2;sticky++) for(unsigned duration=0;duration<2;duration++) {
  reset(3,sticky); CHECK(!key(older[k],true)); CHECK(!key(normals[n],true)); CHECK(!key(normals[n],false)); now+=duration?300:10; CHECK(!key(older[k],false));
  for(unsigned s=0;s<2;s++) if(n==1 || n==s*2) CHECK(get_split_pointing_side_mode(s)==0);
  CHECK(writes==0);
 }
 // A newer press must not reactivate a held key cancelled by Normal.
 // Cover both/left/right mode groups, side-scoped cancellation, sticky taps
 // versus holds, and either release order without writing saved settings.
 const unsigned groups[3][3]={{EH_SNP,EH_SCR,EH_TXT},{EH_L_SNP,EH_L_SCR,EH_L_TXT},{EH_USR1,EH_USR2,EH_USR3}};
 for(unsigned group=0;group<3;group++) for(unsigned old=0;old<3;old++) for(unsigned newer=0;newer<3;newer++) {
  if(old==newer) continue;
  for(unsigned n=0;n<3;n++) for(unsigned sticky=0;sticky<2;sticky++) for(unsigned duration=0;duration<2;duration++) for(unsigned order=0;order<2;order++) {
   reset(POINTING_MODE_TEXT,sticky);
   key(groups[group][old],true); now+=300;
   key(normals[n],true); key(normals[n],false);
   key(groups[group][newer],true); now+=duration?300:10;
   if(order) key(groups[group][newer],false);
   key(groups[group][old],false);
   for(unsigned s=0;s<2;s++) if((group==0 || group==s+1) && (n==1 || n==s*2)) {
    unsigned expected=order?(sticky?(duration?POINTING_MODE_NORMAL:POINTING_MODE_SNIPER+newer):POINTING_MODE_TEXT):POINTING_MODE_SNIPER+newer;
    CHECK(get_split_pointing_side_mode(s)==expected);
   }
   if(!order) key(groups[group][newer],false);
   CHECK(writes==0);
   CHECK(kb_settings_phenom_devices.mode[0]==POINTING_MODE_TEXT && kb_settings_phenom_devices.mode[1]==POINTING_MODE_TEXT);
  }
 }
 // Other side's held key must still release normally after a one-sided Normal.
 for(unsigned order=0;order<2;order++) {
  reset(3,false); key(EH_L_SCR,true); key(EH_USR1,true); key(0x7e49,true); key(0x7e49,false); now+=300;
  key(order?EH_L_SCR:EH_USR1,false); key(order?EH_USR1:EH_L_SCR,false);
  CHECK(get_split_pointing_side_mode(0)==0 && get_split_pointing_side_mode(1)==3);
 }
 puts("PASS production Normal actions: bases/sticky/prior overrides, stale held release, side independence, mouse classification, no settings writes");
 return 0;
}
'''
        cache = Path(os.environ['AGENT_BUILD_CACHE']) / 'phenom-normal-actions'
        cache.mkdir(exist_ok=True)
        probe = cache / 'probe.c'
        probe.write_text(prefix + enum + '\n' + function(source,'sanitize_split_pointing_mode') + '\n' + runtime + '\n' + production + cases)
        for model in ('phenom','phenom_mini','phenom_micro','hpd'):
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',f'-DKEYBOARD_ergohaven_{model}_rev'+('3' if model=='hpd' else '1'),str(probe),'-o',str(cache/'probe')],check=True)
            result=subprocess.run([str(cache/'probe')],capture_output=True,text=True)
            print(model,result.stdout,result.stderr,flush=True)
            self.assertEqual(result.returncode,0)

if __name__ == '__main__': unittest.main()
