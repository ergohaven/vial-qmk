#!/usr/bin/env python3
"""Actual split metadata/serial receiver bounds and healthy recovery; software only."""
from pathlib import Path
import re,subprocess,sys,json
R=Path(__file__).resolve().parents[2];O=Path(sys.argv[1]);O.mkdir(parents=True,exist_ok=True)
def body(path,sig):
 s=(R/path).read_text();m=list(re.finditer(re.escape(sig)+r'[^;{}]*\{',s));assert len(m)==1;(a,b)=(m[0].start(),m[0].end());i=b;d=1
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]+'\n'
t=(R/'quantum/split_common/transactions.c').read_text();state='static bool sm62_rpc_info_valid;\n' if 'static bool sm62_rpc_info_valid;' in t else ''
preamble=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "crc.h"
#define KEYBOARD_ergohaven_sm62
#define NUM_TOTAL_TRANSACTIONS 8
#define PUT_RPC_REQ_DATA 1
#define GET_RPC_RESP_DATA 2
#define RPC_M2S_BUFFER_SIZE 40
#define RPC_S2M_BUFFER_SIZE 32
#define HIGHPRIO 10
#define unlikely(x) (x)
#define THD_WORKING_AREA(n,s) char n[s]
#define THD_FUNCTION(n,a) void n(void*a)
#define serial_dprintf(...) ((void)0)
void chRegSetThreadName(const char*n){}
void chThdCreateStatic(void*w,size_t n,int p,void(*f)(void*),void*a){}
void serial_transport_driver_slave_init(void){}
void serial_transport_driver_master_init(void){}
void serial_transport_driver_clear(void){}
static int locked;
static void unlock(int*p){(void)p;locked--;}
#define split_shared_memory_lock_autounlock() int guard __attribute__((cleanup(unlock)))=(++locked)
typedef void(*callback_t)(uint8_t,const void*,uint8_t,void*);
typedef struct{uint8_t initiator2target_buffer_size;uint16_t initiator2target_offset;uint8_t target2initiator_buffer_size;uint16_t target2initiator_offset;callback_t slave_callback;} split_transaction_desc_t;
static split_transaction_desc_t split_transaction_table[NUM_TOTAL_TRANSACTIONS];
typedef struct{uint8_t checksum;struct{int8_t transaction_id;uint8_t m2s_length,s2m_length;}payload;}rpc_sync_info_t;
static struct{rpc_sync_info_t rpc_info;union{struct{uint8_t rpc_m2s_buffer[40],rpc_s2m_buffer[32];};uint8_t memory[512];};}shared;
#define split_shmem (&shared)
#define split_trans_initiator2target_buffer(t) (&shared.memory[(t)->initiator2target_offset])
#define split_trans_target2initiator_buffer(t) (&shared.memory[(t)->target2initiator_offset])
static uint8_t incoming_id=PUT_RPC_REQ_DATA;
static bool missing;
static unsigned requested,callbacks;
bool serial_transport_receive_blocking(uint8_t*d,size_t n){assert(n==1);*d=incoming_id;return true;}
bool serial_transport_send(const uint8_t*d,size_t n){return true;}
bool serial_transport_receive(uint8_t*d,size_t n){requested=n;if(missing)return false;memset(d,0x5a,n);return true;}
void custom(uint8_t a,const void*p,uint8_t b,void*q){assert(a==40&&b==32);callbacks++;}
'''
proto='\n'.join(l for l in (R/'platforms/chibios/drivers/serial_protocol.c').read_text().splitlines() if not l.startswith('#include'))
prod=state+body('quantum/split_common/transactions.c','void slave_rpc_info_callback(')+body('quantum/split_common/transactions.c','void slave_rpc_exec_callback(')
main=r'''
static void info(int id,unsigned in,unsigned out,bool badcrc){shared.rpc_info.payload.transaction_id=id;shared.rpc_info.payload.m2s_length=in;shared.rpc_info.payload.s2m_length=out;shared.rpc_info.checksum=crc8(&shared.rpc_info.payload,sizeof(shared.rpc_info.payload))^(badcrc?1:0);slave_rpc_info_callback(sizeof(rpc_sync_info_t),&shared.rpc_info,0,NULL);}
int main(void){
 setbuf(stdout,NULL);split_transaction_table[3].slave_callback=custom;
 /* Valid metadata plus interrupted DATA stays within its real40-byte buffer. */
 info(3,40,32,false);missing=true;assert(!react_to_transaction());assert(!locked&&requested==40);missing=false;
 /* Invalid metadata may never resize a receiver beyond its allocated buffers. */
 for(unsigned which=0;which<5;which++){
  info(3,40,32,false);memset(shared.memory,0xa5,sizeof(shared.memory));
  if(which==0)info(3,255,255,true);
  if(which==1)info(3,41,32,false);
  if(which==2)info(3,40,33,false);
  if(which==3)info(-1,40,32,false);
  if(which==4)info(GET_RPC_RESP_DATA,40,32,false);
  requested=0;assert(react_to_transaction());assert(!locked);
  unsigned changed=0;for(unsigned i=40;i<512;i++)changed+=shared.memory[i]!=0xa5;
  printf("case=%u request=%u cross_buffer_changes=%u\n",which,requested,changed);
  assert(requested<=40&&changed==0);
  unsigned c=callbacks;slave_rpc_exec_callback(1,NULL,0,NULL);assert(callbacks==c);
 }
 /* New healthy metadata/data/execute recovers normally, no latch or reset. */
 info(3,40,32,false);assert(react_to_transaction());slave_rpc_exec_callback(1,NULL,0,NULL);assert(callbacks==1);
 puts("PASS CRC/length/ID rejection before receiver sizing; no cross-buffer writes; healthy recovery");return 0;
}
'''
c=O/'test.c';c.write_text(preamble+proto+prod+main);cmd=['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=undefined','-I'+str(R/'quantum'),str(c),str(R/'quantum/crc.c'),'-o',str(O/'test')];r=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
r=subprocess.run([str(O/'test')],capture_output=True,text=True,timeout=5);(O/'result.log').write_text(r.stdout+r.stderr);(O/'result.json').write_text(json.dumps({'compile_exit':0,'test_exit':r.returncode,'boundary':'actual receiver/callbacks/CRC; transport/RTOS memory modeled'},indent=2));print(r.stdout+r.stderr);raise SystemExit(0 if r.returncode==0 else 1)
