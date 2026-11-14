static unsigned sent,io_calls,irq_depth,output_chars;
static uint8_t reports[8][32];
uint32_t bridge_now=100;
static unsigned eeprom_writes;
uint32_t sm62_flash_test_time_us(void){return bridge_now*1000;}
bool sm62_flash_test_context_valid(void){return true;}
static uint8_t eeprom_read_byte(const void *a){uintptr_t o=(uintptr_t)a;assert(o<8192);return eeprom[o];}
static void eeprom_update_byte(void *a,uint8_t v){uintptr_t o=(uintptr_t)a;assert(o<8192);if(eeprom[o]!=v){eeprom[o]=v;eeprom_writes++;}}
static void eeprom_read_block(void *p,const void *a,size_t n){uintptr_t o=(uintptr_t)a;assert(o+n<=8192);memcpy(p,eeprom+o,n);}
static void eeprom_write_block(const void *p,void *a,size_t n){const uint8_t*s=p;for(size_t i=0;i<n;i++)eeprom_update_byte((uint8_t*)a+i,s[i]);}
void eeprom_update_block(const void *p,void *a,size_t n){eeprom_write_block(p,a,n);}
uint32_t save_and_disable_interrupts(void){assert(!irq_depth);irq_depth=1;return 0;}
void restore_interrupts(uint32_t i){assert(i==0&&irq_depth==1);irq_depth=0;}
void flash_range_erase(uint32_t off,unsigned n){assert(irq_depth==1&&off>=0x1f2000&&off+n<=4194304&&off%4096==0&&n%4096==0);io_calls++;memset((void*)(uintptr_t)(0x10000000u+off),255,n);}
void flash_range_program(uint32_t off,const uint8_t*p,unsigned n){assert(irq_depth==1&&off>=0x1f2000&&off+n<=4194304&&off%256==0&&n%256==0);io_calls++;for(unsigned i=0;i<n;i++)((uint8_t*)0x10000000)[off+i]&=p[i];}
void raw_hid_send(uint8_t*p,unsigned n){assert(n==32&&sent<8);memcpy(reply,p,32);memcpy(reports[sent++],p,32);}
