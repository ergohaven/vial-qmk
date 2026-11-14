from pathlib import Path
import subprocess,json,sys
import argparse
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]);a=p.parse_args()
F=a.source.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
if F==O or F in O.parents:raise SystemExit('output must be outside firmware')
def function(s,n):
 start=s.index(n+'(');start=s.rfind('\n',0,start)+1;b=s.index('{',start);depth=1;e=b+1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[start:e]+'\n'
s=(F/'tmk_core/protocol/chibios/usb_main.c').read_text(); raw=function(s,'raw_hid_task')
h=(F/'keyboards/ergohaven/hid.c').read_text(); classifier=function(h,'hid_app_layout_packet_safe_during_refresh') if 'bool hid_app_layout_packet_safe_during_refresh(' in h else ''
# Production classifier uses production protocol defines, not fixture opcode policy.
defines='\n'.join(l.strip().replace('#    define','#define').replace('#        define','#define') for l in h.splitlines() if l.strip().startswith('#') and 'define EH_APP_LAYOUT_' in l and any(x in l for x in ['BEGIN ','KEYCODES ','COMMIT ','LAYER_NAME ','KEEPALIVE ','EVENT_POLL ','EVENT_REQUEST ','VISUALS ','EXECUTION_STATE ','ENCODER_STACK ','DEACTIVATE ']))
prefix='''
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#define RAW_EPSIZE 32
#define USB_ENDPOINT_OUT_RAW 1
#define EH_FAST_UPLOAD_ENABLE
#define FAST_EPSIZE 32
#define USB_ENDPOINT_OUT_FAST 2
#define USB_ENDPOINT_IN_FAST 3
static unsigned fast_queued,fast_received,fast_parsed,fast_replies;
static bool eh_background_process_hid(uint8_t *b,unsigned n){(void)b;assert(n==32);fast_parsed++;return true;}
static void send_report(unsigned ep,uint8_t *b,unsigned n){(void)b;assert(ep==3&&n==32);fast_replies++;}
#define SM62_RAW_HID_FRAME_GATE
#define EH_APP_LAYOUT_PROTOCOL_VERSION 8
static bool pending;
static bool lv_disp_refr_pending(void *d){(void)d;return pending;}
#define SM62_RAW_REFRESH_PENDING() lv_disp_refr_pending(NULL)
static unsigned queued,received,parsed,settings_calls;
static uint8_t queue[2048][32],seen[2048][32];
static bool receive_report(unsigned ep,uint8_t *b,unsigned n){assert(n==32);if(ep==2){if(fast_received==fast_queued)return false;fast_received++;memset(b,0,n);return true;}assert(ep==1);if(received==queued)return false;memcpy(b,queue[received++],32);return true;}
static void raw_hid_receive(uint8_t *b,unsigned n){assert(n==32&&((uintptr_t)b%8)==0);memcpy(seen[parsed++],b,32);if(b[0]==0x44)settings_calls++;}
bool hid_app_layout_packet_safe_during_refresh(const uint8_t*,uint8_t);
'''
main='''
int main(int argc,char**argv){
 assert(argc==2);unsigned mode=(unsigned)argv[1][0]-'0';
 if(mode==0){queued=1024;for(unsigned i=0;i<queued;i++){queue[i][0]=0xe5;queue[i][1]=8;}raw_hid_task();printf("bounded received=%u parsed=%u remaining=%u\\n",received,parsed,queued-received);assert(parsed>0&&parsed<=8&&received<=8);}
 if(mode==1){queued=32;for(unsigned i=0;i<queued;i++){queue[i][0]=0xe5;queue[i][1]=8;queue[i][3]=i;}queue[3][0]=0x44;pending=true;raw_hid_task();printf("pending received=%u parsed=%u settings=%u\\n",received,parsed,settings_calls);assert(received==4&&parsed==3&&settings_calls==0);for(unsigned i=0;i<100;i++)raw_hid_task();assert(received==4&&parsed==3);pending=false;while(parsed<queued)raw_hid_task();assert(settings_calls==1&&memcmp(queue,seen,sizeof(queue[0])*queued)==0);puts("FIFO recovery byte-identical; no overtaking/drop/duplicate");}
 if(mode==2){queued=2;queue[0][0]=0xe1;queue[0][1]=7;queue[1][0]=0xe5;queue[1][1]=8;pending=true;raw_hid_task();assert(received==1&&parsed==0);pending=false;raw_hid_task();assert(parsed==2&&memcmp(queue,seen,64)==0);puts("wrong-version head deferred intact");}
 if(mode==4){fast_queued=1024;pending=true;raw_hid_task();assert(fast_received==0&&fast_parsed==0);pending=false;raw_hid_task();assert(fast_received==8&&fast_parsed==8&&fast_replies==8);puts("FAST pending gate and shared count budget qualified");}
 if(mode==3){queued=2;queue[0][0]=0xe6;queue[0][1]=8;queue[1][0]=0xe1;queue[1][1]=8;pending=true;raw_hid_task();assert(received==1&&parsed==0);pending=false;raw_hid_task();assert(parsed==2&&memcmp(queue,seen,64)==0);puts("EVENT_POLL deferred intact");}
 if(mode==5){queued=16;for(unsigned i=0;i<queued;i++){queue[i][0]=0xe5;queue[i][1]=8;}queue[0][0]=0xeb;queue[0][2]=0xa5;queue[0][3]=1;queue[1][0]=0xec;queue[2][0]=0xed;queue[3][0]=0xee;queue[5][0]=0x44;pending=true;raw_hid_task();assert(parsed==5&&received==6&&settings_calls==0);raw_hid_task();assert(parsed==5&&received==6);pending=false;while(parsed<queued)raw_hid_task();assert(settings_calls==1&&!memcmp(queue,seen,32*queued));puts("compact capability/envelope admitted; retained FIFO ordering and eight-report budget unchanged");}
}
'''
(O/'fifo.c').write_text(prefix+defines+'\n'+classifier+raw+main)
cmd=['gcc','-include',str(F/'quantum/sm62_trace.h'),'-std=gnu11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',str(O/'fifo.c'),'-o',str(O/'fifo')]
b=subprocess.run(cmd,capture_output=True,text=True);assert b.returncode==0,b.stderr
results=[]
for m in range(6):
 p=subprocess.run(['stdbuf','-o0',str(O/'fifo'),str(m)],capture_output=True,text=True);results.append(dict(mode=m,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));print(results[-1])
name='fifo-results.json';(O/name).write_text(json.dumps(results,indent=2))
sys.exit(any(x['exit']!=0 for x in results))
