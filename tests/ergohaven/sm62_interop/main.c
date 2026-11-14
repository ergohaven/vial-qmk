
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
extern unsigned bridge_resident_key(unsigned,unsigned);
extern unsigned bridge_resident_visual(unsigned,unsigned);
extern unsigned bridge_resident_active(void);
extern void bridge_resident_tick(void);
int main(int argc,char**argv){
 assert(argc==2);assert(dynamic_keymap_macro_get_count()==109 && dynamic_keymap_macro_get_buffer_size()==6066 && dynamic_keymap_get_layer_count()==16 && MATRIX_ROWS==10 && MATRIX_COLS==7 && VIAL_TAP_DANCE_ENTRIES==0);
 int fd=open(argv[1],O_RDWR|O_CREAT,0600);assert(fd>=0);
 struct stat st;assert(!fstat(fd,&st));bool fresh=st.st_size==0;
 assert(fresh||st.st_size==4194304+8192);if(fresh)assert(!ftruncate(fd,4194304+8192));
 void*p=mmap((void*)0x10000000,4194304,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_FIXED,fd,0);assert(p==(void*)0x10000000);
 if(fresh){memset(p,255,4194304);memset(eeprom,0,8192);assert(pwrite(fd,eeprom,8192,4194304)==8192);}else assert(pread(fd,eeprom,8192,4194304)==8192);
 char line[512];while(fgets(line,sizeof(line),stdin)){
  line[strcspn(line,"\r\n")]=0;
  if(!strcmp(line,"INFO")){printf("{\"macro_count\":%u,\"macro_size\":%u,\"macro_eeprom_addr\":%u,\"macro_eeprom_size\":%u,\"layers\":%u,\"rows\":%u,\"cols\":%u,\"controls\":62,\"resident_slots\":100,\"td\":%u,\"combo\":%u,\"ko\":%u,\"alt_repeat\":%u,\"nor_writes\":%u,\"eeprom_writes\":%u}\n",dynamic_keymap_macro_get_count(),dynamic_keymap_macro_get_buffer_size(),(unsigned)DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR,(unsigned)DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE,dynamic_keymap_get_layer_count(),MATRIX_ROWS,MATRIX_COLS,VIAL_TAP_DANCE_ENTRIES,VIAL_COMBO_ENTRIES,VIAL_KEY_OVERRIDE_ENTRIES,VIAL_ALT_REPEAT_KEY_ENTRIES,io_calls,eeprom_writes);fflush(stdout);continue;}
  unsigned l,c;if(sscanf(line,"KEY %u %u",&l,&c)==2){printf("{\"key\":%u,\"visual\":%u,\"active\":%u}\n",bridge_resident_key(l,c),bridge_resident_visual(l,c),bridge_resident_active());fflush(stdout);continue;}
  unsigned id;if(sscanf(line,"PLAY %u",&id)==1){output_chars=0;dynamic_keymap_macro_send(id);printf("{\"output_chars\":%u}\n",output_chars);fflush(stdout);continue;}
  unsigned mr,mask;if(sscanf(line,"MATRIX %u %u",&mr,&mask)==2){assert(mr<10&&mask<128);bridge_matrix[mr]=mask;vial_unlock_task();puts("{}");fflush(stdout);continue;}
  unsigned ms;if(sscanf(line,"TICK %u",&ms)==1){bridge_now+=ms;vial_unlock_task();bridge_resident_tick();puts("{}");fflush(stdout);continue;}
  uint64_t aligned[4]={0};uint8_t*q=(uint8_t*)aligned;size_t n=strlen(line);assert(n==64||n==66);unsigned shift=n==66?2:0;if(shift)assert(line[0]=='0'&&line[1]=='0');
  for(unsigned i=0;i<32;i++){unsigned x;assert(sscanf(line+shift+2*i,"%2x",&x)==1);q[i]=x;}
  unsigned before=io_calls,ew=eeprom_writes;sent=0;raw_hid_receive(q,32);
  if(ew!=eeprom_writes)assert(pwrite(fd,eeprom,8192,4194304)==8192);
  printf("{\"responses\":[");for(unsigned j=0;j<sent;j++){printf("%s\"",j?",":"");for(unsigned i=0;i<32;i++)printf("%02x",reports[j][i]);putchar('"');}
  printf("],\"nor_writes\":%u,\"eeprom_writes\":%u}\n",io_calls-before,eeprom_writes-ew);fflush(stdout);
 }
 assert(!msync(p,4194304,MS_SYNC));assert(!munmap(p,4194304));close(fd);return 0;
}
