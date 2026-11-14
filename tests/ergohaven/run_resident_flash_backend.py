#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys
R=Path(__file__).resolve().parents[2];O=Path(sys.argv[1]);O.mkdir(parents=True,exist_ok=True)
# Isolate production backend translation unit with only SDK/guard boundaries.
inc=O/'include';(inc/'hardware').mkdir(parents=True,exist_ok=True)
(inc/'vial_extension.h').write_bytes((R/'keyboards/ergohaven/sm62/vial_extension.h').read_bytes())
(inc/'compiler_support.h').write_text('#define STATIC_ASSERT(x,m) _Static_assert(x,m)\n')
(inc/'flash_guard.h').write_text('bool sm62_flash_begin(void);void sm62_flash_end(void);\n')
(inc/'hardware/flash.h').write_text('#define XIP_BASE 0x10000000u\n#define FLASH_SECTOR_SIZE 4096u\n#define FLASH_PAGE_SIZE 256u\nvoid flash_range_erase(unsigned,unsigned);void flash_range_program(unsigned,const uint8_t*,unsigned);\n')
(inc/'hardware/sync.h').write_text('uint32_t save_and_disable_interrupts(void);void restore_interrupts(uint32_t);\n')
# Copy exact bytes; relative include resolution must use these boundary headers.
src=R/'keyboards/ergohaven/sm62'
(O/'resident_cache_flash.c').write_bytes((src/'resident_cache_flash.c').read_bytes())
(O/'resident_cache_flash.h').write_bytes((src/'resident_cache_flash.h').read_bytes())
(O/'layout_reserve.h').write_bytes((src/'layout_reserve.h').read_bytes())
main=r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "resident_cache_flash.h"
static bool permit;static unsigned begins,ends,locks,restores,erases,programs,last,size;
bool sm62_flash_begin(void){begins++;return permit;}
void sm62_flash_end(void){ends++;}
uint32_t save_and_disable_interrupts(void){locks++;return 7;}
void restore_interrupts(uint32_t s){assert(s==7);restores++;}
void flash_range_erase(unsigned a,unsigned n){erases++;last=a;size=n;}
void flash_range_program(unsigned a,const uint8_t*p,unsigned n){assert(p);programs++;last=a;size=n;}
int main(void){
 uint8_t page[256]={0};
 assert(!sm62_cache_flash_record(101));assert(!sm62_cache_flash_erase(101));assert(!sm62_cache_flash_program(101,0,page));
 assert(!sm62_cache_flash_program(0,1,page));assert(!sm62_cache_flash_program(0,8192,page));assert(!sm62_cache_flash_program(0,~0u,page));assert(!begins);
 assert(!sm62_cache_flash_erase(0));assert(!sm62_cache_flash_program(0,0,page));assert(begins==2&&!erases&&!programs&&!locks&&!ends);
 permit=true;
 for(unsigned r=0;r<101;r++){
  assert((uintptr_t)sm62_cache_flash_record(r)==0x10200000u+r*8192);
  assert(sm62_cache_flash_erase(r));assert(last==0x200000u+r*8192&&size==8192);
  assert(sm62_cache_flash_program(r,7936,page));assert(last==0x200000u+r*8192+7936&&size==256);
 }
 assert(erases==101&&programs==101&&ends==202&&locks==202&&restores==202);
 puts("PASS production RP2040 backend: bounds, 101x8192 partition, failed guard no IO, successful guard/IRQ balanced");
}
'''
(O/'main.c').write_text(main)
cmd=['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-DPICO_FLASH_SIZE_BYTES=4194304','-DSM62_CODE_FLASH_LIMIT=0x1f2000','-fsanitize=undefined','-I'+str(inc),str(O/'resident_cache_flash.c'),str(O/'main.c'),'-o',str(O/'backend')]
p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
out=p.stdout
if p.returncode==0:
 p=subprocess.run([str(O/'backend')],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);out+=p.stdout
(O/'result.log').write_text(out+f'EXIT={p.returncode}\n');print(out);raise SystemExit(p.returncode)
