#!/usr/bin/env python3
"""SM62 source-backed split rejoin tests; no device access."""
from pathlib import Path
import re,subprocess,sys,json
R=Path(__file__).resolve().parents[2];O=Path(sys.argv[1]);O.mkdir(parents=True,exist_ok=True)
def body(path,signature):
 s=(R/path).read_text();hits=list(re.finditer(re.escape(signature)+r'[^;{}]*\{',s));assert len(hits)==1,(path,signature,len(hits));a=hits[0].start();b=hits[0].end();d=1;i=b
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]+'\n'
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#define KEYBOARD_ergohaven_sm62
#define SPLIT_WATCHDOG_TIMEOUT 2100
#define PUT_WATCHDOG 0
static uint32_t now,split_watchdog_started;
static bool split_watchdog_done,master,connected=true,ping,deliver=true;
static unsigned resets,writes;
typedef uint8_t matrix_row_t;
static struct{bool watchdog_pinged;} shared;
#define split_shmem (&shared)
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
bool is_transport_connected(void){return connected;}
bool is_keyboard_master(void){return master;}
void mcu_reset(void){resets++;}
bool transport_write(int id,const void*p,unsigned n){assert(id==0&&n==1);writes++;if(deliver)ping=*(const bool*)p;return deliver;}
'''
prod=''.join(body('quantum/split_common/split_util.c',s) for s in ['void split_watchdog_init(', 'void split_watchdog_update(', 'bool split_watchdog_check(', 'void split_watchdog_task('])
prod+=body('quantum/split_common/transactions.c','static bool watchdog_handlers_master(')+body('quantum/split_common/transactions.c','static void watchdog_handlers_slave(')
main=r'''
int main(void){
 master=true;assert(watchdog_handlers_master(NULL,NULL));assert(writes==1&&ping);bool saved_master_done=split_watchdog_done;
 /* Peer reboots while master still regards transport connected. */
 master=false;split_watchdog_done=false;shared.watchdog_pinged=false;now=100;split_watchdog_init();
 master=true;split_watchdog_done=saved_master_done;now=600;ping=false;assert(watchdog_handlers_master(NULL,NULL));
 bool new_ping=ping;saved_master_done=split_watchdog_done;
 master=false;split_watchdog_done=false;shared.watchdog_pinged=new_ping;watchdog_handlers_slave(NULL,NULL);now=2300;split_watchdog_task();
 printf("rejoin master_connected=%u ping_writes=%u peer_resets=%u\n",connected,writes,resets);assert(new_ping&&resets==0);
 /* No per-scan heartbeat flood. */
 master=true;split_watchdog_done=saved_master_done;unsigned before=writes;
 for(unsigned t=2301;t<2500;t++){now=t;assert(watchdog_handlers_master(NULL,NULL));}
 assert(writes-before<=1);
 /* Real disconnected/failed write is still propagated, healthy retry works. */
 connected=false;deliver=false;assert(!watchdog_handlers_master(NULL,NULL));assert(!split_watchdog_done);
 connected=true;deliver=true;assert(watchdog_handlers_master(NULL,NULL));assert(split_watchdog_done);
 puts("PASS peer reboot/rejoin without disconnect threshold; bounded heartbeat; failed write/retry preserved");return 0;
}
'''
c=O/'watchdog.c';c.write_text(pre+prod+main);exe=O/'watchdog';cmd=['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=undefined',str(c),'-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=5);(O/'result.log').write_text(p.stdout+p.stderr);(O/'result.json').write_text(json.dumps({'compile_exit':0,'test_exit':p.returncode,'source':'actual production watchdog+transactions handlers; clock/transport/reset modeled'},indent=2));print(p.stdout+p.stderr);raise SystemExit(0 if p.returncode==0 else 1)
