#include "issd_strength.h"
#include <assert.h>
#include <string.h>
static unsigned char rom[0x200000];
int main(void){
 IssdTeamStrength s;memset(rom,0,sizeof rom);
 for(int t=0;t<42;t++){
  rom[0x5ef48+t*2]=0;rom[0x5ef49+t*2]=0x80;
  for(int p=0;p<20;p++)memset(rom+0x50000+(t*20+p)*7,0x99,7);
 }
 for(int p=0;p<10;p++)rom[0x58000+21+p]=p<4?1:p<8?2:3;
 assert(issd_strength_calculate(rom,sizeof rom,0,&s));assert(s.overall==99&&s.stars==4);
 assert(s.attack==99&&s.midfield==99&&s.defence==99);
 /* Actual applied bytes, including added/all-star attribute records. */
 memset(rom+0x50000+41*140,0x22,140);
 assert(issd_strength_calculate(rom,sizeof rom,41,&s));assert(s.overall==0&&s.stars==1);
 assert(!issd_strength_calculate(rom,sizeof rom,42,&s));
 assert(!issd_strength_calculate(rom,0x50000,0,&s));
 assert(!issd_strength_calculate(rom,sizeof rom,-1,&s));
 rom[0x5ef49]=0;assert(!issd_strength_calculate(rom,sizeof rom,0,&s));
 return 0;
}
