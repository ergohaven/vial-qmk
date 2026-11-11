#!/usr/bin/env python3
"""Build real production HID/VIA/journal peer. No HID/device access.
Usage: run_resident100_peer.py OUT; OUT/resident100_peer FLASH_IMAGE
stdin: 64 hex characters/report. stdout: 64 hex response or '-' (no reply).
TIME <ms>: advances modeled time and actual task; stdout OK.
KEY <row> <col> <0|1>: actual production callback; stdout JSON.
STATE: actual live state JSON. BUDGET <IOs> <torn-byte-count>: fault injection.
All actual model flash mutations fflush+fsync before returning to parser.
Restart same executable/image for fresh C RAM recovery. stderr diagnostics only.
"""
from pathlib import Path
import subprocess,sys
R=Path(__file__).resolve().parents[2];O=Path(sys.argv[1]).resolve();O.mkdir(parents=True,exist_ok=True)
base=(R/'tests/ergohaven/run_compact_usb.py').read_text()
native=(R/'tests/ergohaven/resident100_native.c').read_text().split('static uint8_t raw[4560]')[0]
native=native.replace('static uint8_t disk[101][8192];','''#include <unistd.h>
static uint8_t disk[101][8192];
static FILE *flash_file;
static void persist(unsigned r,unsigned off,unsigned n) {
 assert(!fseek(flash_file,r*8192+off,SEEK_SET));
 assert(fwrite(disk[r]+off,1,n,flash_file)==n);assert(!fflush(flash_file));assert(!fsync(fileno(flash_file)));
}''')
native=native.replace('memset(disk[r],255,8192);return true;','memset(disk[r],255,8192);persist(r,0,8192);return true;')
native=native.replace('if(tear>=0)for(int i=0;i<tear;i++)disk[r][off+i]&=page[i];return false;','if(tear>=0){for(int i=0;i<tear;i++)disk[r][off+i]&=page[i];persist(r,off,256);}return false;')
native=native.replace('disk[r][off+i]&=page[i];return true;','disk[r][off+i]&=page[i];persist(r,off,256);return true;')
native+=r'''
int main(int argc,char **argv) {
 assert(argc==2);flash_file=fopen(argv[1],"r+b");
 if(!flash_file){flash_file=fopen(argv[1],"w+b");assert(flash_file);memset(disk,255,sizeof(disk));assert(fwrite(disk,1,sizeof(disk),flash_file)==sizeof(disk));assert(!fflush(flash_file));}
 else {assert(fread(disk,1,sizeof(disk),flash_file)==sizeof(disk));assert(fgetc(flash_file)==EOF);clearerr(flash_file);}
 char line[256];
 while(fgets(line,sizeof(line),stdin)) {
  unsigned ms,row,col,down;
  if(sscanf(line,"TIME %u",&ms)==1){now+=ms;hid_app_layout_task();puts("OK");}
  else if(sscanf(line,"BUDGET %d %d",&budget,&tear)==2){assert(budget>=-1&&tear>=-1&&tear<=256);puts("OK");}
  else if(sscanf(line,"KEY %u %u %u",&row,&col,&down)==3){bool handled=hid_app_layout_process_keyevent(row,col,down);printf("{\"handled\":%u,\"downs\":%u,\"ups\":%u,\"down_code\":%u,\"up_code\":%u}\n",handled,downs,ups,down_code,up_code);}
  else if(!strncmp(line,"STATE",5)){char name[32]={0};hid_app_layout_get_name(0,name,32);printf("{\"active\":%u,\"revision\":%u,\"key0\":%u,\"generation\":%u,\"writes\":%u,\"name_hex\":\"",app_layout_active,(unsigned)app_layout_revision,app_layout_keycodes[0][0],(unsigned)app_layout_visual_generation,writes);for(unsigned i=0;name[i];i++)printf("%02x",(uint8_t)name[i]);puts("\"}");}
  else {assert(strcspn(line,"\r\n")==64);uint8_t p[32];for(unsigned i=0;i<32;i++){unsigned v;assert(sscanf(line+2*i,"%2x",&v)==1);p[i]=v;}unsigned n=replies;receive(p);assert(replies-n<=1);if(replies==n)puts("-");else{for(unsigned i=0;i<32;i++)printf("%02x",response[i]);puts("");}}
  assert(!fflush(stdout));
 }
 assert(feof(stdin));assert(!fclose(flash_file));return 0;
}
'''
script=base[:base.index("main=r'''")]+"main=r'''\n"+native+"'''\n"+base[base.index('text=prefix+prod'):base.index('# Baseline mode')]+"subprocess.run(cmd,check=True)\n"
script=script.replace("R=Path(__file__).resolve().parents[2]",'R=Path('+repr(str(R))+')').replace("exe=O/'compact_receiver'","exe=O/'resident100_peer'")
runner=O/'peer_build.py';runner.write_text(script)
subprocess.run([sys.executable,str(runner),str(O)],check=True)
print(O/'resident100_peer')
