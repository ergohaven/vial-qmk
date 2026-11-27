"""Execute actual timeout storage/default/sanitizer/get/set with simulated apply."""
import os, re, subprocess, unittest, json
from pathlib import Path
from test_phenom_encoder_step_only import ROOT, SOURCE, function

VALUES=[50,100,150,200,250,300,350,400,450,500,750,1000,1250,1500,1750,2000,2250,2500,2750,3000]
class Timeout20(unittest.TestCase):
 def test_storage(self):
  s=SOURCE.read_text(); h=SOURCE.with_suffix('.h').read_text()
  struct=h[h.index('typedef struct {'):h.index('} kb_settings_split_pointing_t;')+len('} kb_settings_split_pointing_t;')]
  constants=s[s.index('#define PHENOM_AUTO_MOUSE_SUPPORTED_MODES_MASK'):s.index('static uint8_t phenom_auto_mouse_mode_mask')]
  constants=re.sub(r'^static const uint8_t module_encoder_interval_ms_table.*\n','',constants,flags=re.M)
  prefix='''#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#define EH_KEYBOARD_SPLIT_POINTING_V2
#define EH_SPLIT_POINTING_INVERT_AXES
#define AUTO_MOUSE_DEFAULT_LAYER 4
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define SPLIT_POINTING_DEVICE_COUNT 4
#define SPLIT_POINTING_SIDE_COUNT 2
#define SPLIT_POINTING_SIDE_RIGHT 1
enum {ROT_0,ROT_90,ROT_180,ROT_270};
enum {POINTING_MODE_NORMAL,POINTING_MODE_SNIPER,POINTING_MODE_SCROLL,POINTING_MODE_TEXT};
static bool get_sticky_mode(void){return true;}
'''
  boundary='''static kb_settings_split_pointing_t kb_settings_phenom_devices;
static unsigned applied;
static void kb_settings_split_pointing_update(kb_settings_split_pointing_t c){kb_settings_phenom_devices=c; applied=phenom_auto_mouse_timeout_ms[c.auto_mouse_timeout_idx];}
'''
  prod='\n'.join(function(s,n) for n in ['get_split_pointing_settings_default','config_all_zero','kb_settings_split_pointing_sanitize','get_split_pointing_auto_mouse_timeout_idx','set_split_pointing_auto_mouse_timeout_idx'])
  cases='''
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\\n",__LINE__,#x);return 1;}}while(0)
int main(void){
const unsigned expected[]={'''+','.join(map(str,VALUES))+'''};
CHECK(sizeof(kb_settings_split_pointing_t)==40);
CHECK(PHENOM_SPLIT_POINTING_SETTINGS_VERSION==9);
CHECK(offsetof(kb_settings_split_pointing_t,auto_mouse_timeout_idx)==35);
CHECK(get_split_pointing_settings_default().auto_mouse_timeout_idx==10);
for(unsigned i=0;i<256;i++){
kb_settings_phenom_devices=get_split_pointing_settings_default();
set_split_pointing_auto_mouse_timeout_idx(i);
unsigned idx=i<(EXPECT_EXTENDED?20:14)?i:10;
CHECK(get_split_pointing_auto_mouse_timeout_idx()==idx);
CHECK(applied==expected[idx]);
kb_settings_phenom_devices.auto_mouse_timeout_idx=i;
kb_settings_phenom_devices=kb_settings_split_pointing_sanitize(kb_settings_phenom_devices);
CHECK(get_split_pointing_auto_mouse_timeout_idx()==idx);
CHECK(phenom_auto_mouse_timeout_ms[idx]==expected[idx]);
}
for(unsigned i=0;i<6;i++) {
kb_settings_split_pointing_t old=get_split_pointing_settings_default();
old.version=PHENOM_SPLIT_POINTING_PREVIOUS_VERSION; old.auto_mouse_timeout_idx=i;
old=kb_settings_split_pointing_sanitize(old);
CHECK(phenom_auto_mouse_timeout_ms[old.auto_mouse_timeout_idx]==(i+1)*250);
}
kb_settings_split_pointing_t zero={0};
CHECK(kb_settings_split_pointing_sanitize(zero).auto_mouse_timeout_idx==10);
puts("PASS timeout indices/default/sanitize, 40-byte version9 store");return 0;}
'''
  cache=Path(os.environ['AGENT_BUILD_CACHE'])/'phenom-timeout20';cache.mkdir(exist_ok=True)
  (cache/'probe.c').write_text(prefix+struct+'\n'+constants+boundary+prod+cases)
  for model in ('phenom','phenom_mini','phenom_micro','hpd'):
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',f'-DKEYBOARD_ergohaven_{model}_rev'+('3' if model=='hpd' else '1'),f'-DEXPECT_EXTENDED={int(model!="hpd")}',str(cache/'probe.c'),'-o',str(cache/'probe')],check=True)
   r=subprocess.run([str(cache/'probe')],capture_output=True,text=True);print(model,r.stdout,r.stderr);self.assertEqual(r.returncode,0)
 def test_descriptors(self):
  for model in ('phenom','phenom_mini','phenom_micro'):
   d=json.loads(subprocess.check_output(['cpp','-P','-x','c','-I',str(ROOT),str(ROOT/f'keyboards/ergohaven/{model}/keymaps/v1/vial.json')],text=True))
   timeout=[f for g in d['settings'] for f in g['fields'] if f['qsid']==324]
   self.assertEqual(timeout[0]['variants'],[f'{v} ms' for v in VALUES])
   keys=d['customKeycodes'];self.assertEqual([k['name'] for k in keys[73:]],['EH_L_NORMAL','EH_NORMAL','EH_R_NORMAL'])
   self.assertEqual([k['shortName'] for k in keys[73:]],['Left\nNormal','Normal\nmode','Right\nNormal'])
if __name__=='__main__': unittest.main()
