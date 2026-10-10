#define main scenery_harness_main
#include "test_stadium_scenery_envelope.c"
#undef main
#include "issd_stadium_rom.h"
#include <assert.h>
static Interp816 *prepare(Interp816 *cpu,unsigned pc,unsigned length,int x,int y,int z) {
  memset(ram,0,sizeof ram);put(0x1fe,0x7fff);put(0x12a2,(int)length);put(0x12a4,576);
  put(0x42a,x);put(0x42c,y);put(0x410,z);put(0x424,1);put(0x428,1);
  put(0x19ec,x);put(0x19ee,y);put(0x1c80,232);put(0x1c82,240);
  put(0x1c84,336);put(0x1c86,344);put(0x1c8a,(int)length+72);put(0x1c8c,(int)length+56);
  interp816_reset(cpu);cpu->e=cpu->mf=cpu->xf=false;cpu->k=0x83;cpu->db=0;
  cpu->pc=(uint16_t)pc;cpu->sp=0x1fd;return cpu;
}
static int field_route(Interp816 *cpu) {
  for(unsigned step=0;step<256;step++) {
    if(cpu->k!=0x83)return -1;
    switch(cpu->pc) {
      case 0x8000:return 0;case 0x8a94:return 1; /* throw-in decision */
      case 0x8ddc:return 2; /* original corner/goal-kick decision */
      case 0x8da0:return 3; /* goal aperture/depth/height accepted */
      case 0x8b75:case 0x8d23:return 4; /* original crossbar response */
      default:interp816_runOpcode(cpu);break;
    }
  }
  return -1;
}
int main(int argc,char **argv) {
  if(argc!=2)return 2;
  FILE *file=fopen(argv[1],"rb");assert(file);assert(fread(rom,1,sizeof rom,file)==sizeof rom);fclose(file);
  unsigned char *canonical=malloc(sizeof rom);assert(canonical);memcpy(canonical,rom,sizeof rom);
  Interp816 *cpu=interp816_init(NULL,read_bus,write_bus);assert(cpu);
  for(unsigned length=1536;length<=1792;length+=32) {
    IssdStadiumProfile profile={.version=1,.base_layout=0,.length_units=(uint16_t)length,.width_units=576};
    IssdStadiumRom view={0};assert(issd_stadium_rom_build(&view,canonical,sizeof rom,&profile));
    memcpy(rom,view.data,sizeof rom);issd_stadium_rom_clear(&view);
    int xs[]={-1,0,383,384,(int)length-385,(int)length-384,(int)length-1,(int)length};
    int ys[]={127,128,511,512};
    for(unsigned x=0;x<8;x++)for(unsigned y=0;y<4;y++) {
      prepare(cpu,0x8eb5,length,xs[x],ys[y],0);
      for(unsigned n=0;n<128&&cpu->pc!=0x8000;n++)interp816_runOpcode(cpu);
      assert(cpu->pc==0x8000);
      bool expected=xs[x]>=0&&xs[x]<(int)length&&(xs[x]<384||xs[x]>=(int)length-384)&&ys[y]>=128&&ys[y]<512;
      assert(cpu->c==expected);
    }
    /* The restart side split is a baked immediate in the original routine.
     * Execute it from the actual selected ROM view on/on either side of half. */
    int split_y[]={231,232,343,344};
    for(unsigned side=0;side<2;side++)for(unsigned yi=0;yi<4;yi++) {
      prepare(cpu,0x8e12,length,(int)length/2-(side?0:1),split_y[yi],0);
      unsigned expected=yi==0?(side?0x8e51:0x8e57):
                        yi==3?(side?0x8e57:0x8e51):0x8e35;
      for(unsigned step=0;step<64&&cpu->pc!=0x8e35&&cpu->pc!=0x8e51&&cpu->pc!=0x8e57;step++)
        interp816_runOpcode(cpu);
      assert(cpu->pc==expected);
    }
    int cases[][4]={{100,579,0,0},{100,580,0,1},{100,-4,0,0},{100,-5,0,1},
      {(int)length+3,288,0,0},{(int)length+4,288,0,3},
      {(int)length+4,200,0,2},{-4,288,0,0},{-5,288,0,3},
      {(int)length+71,288,0,3},{(int)length+72,288,0,2},{-63,288,0,3},{-64,288,0,2},
      {(int)length+4,288,-65,2},{(int)length+4,288,-64,4},
      {(int)length+4,288,-57,4},{(int)length+4,288,-56,3}};
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
      prepare(cpu,0x8abf,length,cases[i][0],cases[i][1],cases[i][2]);
      int actual=field_route(cpu);
      if(actual!=cases[i][3]){fprintf(stderr,"length%u case%u route%d expected%d pc%04x\n",length,i,actual,cases[i][3],cpu->pc);return 1;}
    }
  }
  free(canonical);interp816_free(cpu);puts("Original interpreter profile predicates passed");return 0;
}
