/* Execute original cartridge scenery callbacks through the production65816
 * interpreter. Fixtures set coordinates; no rewritten predicate model. */
#include "interp816.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char rom[0x200000],ram[0x20000];
static unsigned points[1024];
static unsigned char read_bus(void *unused,unsigned address) {
  (void)unused;
  unsigned bank=address>>16,low=address&65535;
  if(bank==0x7e||bank==0x7f) return ram[(bank-0x7e)*65536+low];
  if((bank&0x7f)<0x40&&low<0x2000) return ram[low];
  if(low>=0x8000) return rom[((address&0x7f0000)>>1)|(address&0x7fff)];
  return 0;
}
static void write_bus(void *unused,unsigned address,unsigned char value) {
  (void)unused;
  unsigned bank=address>>16,low=address&65535;
  if(bank==0x7e||bank==0x7f) ram[(bank-0x7e)*65536+low]=value;
  else if((bank&0x7f)<0x40&&low<0x2000) ram[low]=value;
}
int interp816_opcode_hook(unsigned address) { (void)address;return 0; }
static void put(unsigned address,int value) {ram[address]=(unsigned char)value;ram[address+1]=(unsigned char)(value>>8);}
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  FILE *file=fopen(argv[1],"rb");if(!file)return 2;
  if(fread(rom,1,sizeof rom,file)!=sizeof rom){fclose(file);return 2;}fclose(file);
  file=fopen(argv[2],"r");if(!file)return 2;
  unsigned count=0;while(count<1024&&fscanf(file,"%u",&points[count])==1)count++;
  unsigned extra;if(count<2||fscanf(file,"%u",&extra)==1){fclose(file);return 2;}fclose(file);
  static const unsigned lengths[]={1792,1856,1984,2048,1920,1920,1792,2176};
  static const unsigned widths[]={576,640,704,640,640,576,704,704};
  Interp816 *cpu=interp816_init(NULL,read_bus,write_bus);if(!cpu)return 2;
  unsigned long cases=0;
  for(unsigned base=0;base<8;base++)for(unsigned length=1536;length<=lengths[base];length+=32)
    for(unsigned xi=0;xi<count;xi++)for(unsigned yi=0;yi<count;yi++) {
      int x=(short)points[xi],y=(short)points[yi];unsigned width=widths[base];
      if(y<0||y>(int)width||x<-72||x>(int)length+72)continue;
      if((x<0||x>(int)length)&&(y<(int)width/2-48||y>(int)width/2+48))continue;
      for(unsigned direction=0;direction<4;direction++) {
        memset(ram,0,0x500);put(0x86,(int)base);put(0x410,0);
        put(0x42a,x);put(0x42c,y);put(0x424,direction&1?1:-1);put(0x428,direction&2?1:-1);
        put(0x1fe,0x7fff);
        interp816_reset(cpu);cpu->e=cpu->mf=cpu->xf=false;cpu->k=0x83;cpu->db=0;
        cpu->pc=0xa07c;cpu->sp=0x1fd;
        unsigned step;
        for(step=0;step<256&&cpu->pc!=0x8000;step++) {
          if(cpu->k!=0x83||cpu->pc==0xa820||cpu->pc==0xa84a) {
            fprintf(stderr,"collision base%u length%u x%d y%d direction%u pc%02x%04x\n",base,length,x,y,direction,cpu->k,cpu->pc);
            interp816_free(cpu);return 1;
          }
          interp816_runOpcode(cpu);
        }
        if(step==256){fprintf(stderr,"Callback did not return\n");interp816_free(cpu);return 1;}
        cases++;
      }
    }
  interp816_free(cpu);printf("%lu original-interpreter scenery cases passed\n",cases);return 0;
}
