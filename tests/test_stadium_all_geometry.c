#include "issd_stadium_geometry.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  uint8_t *rom=malloc(0x200000);if(!rom)return 2;
  FILE *file=fopen(argv[1],"rb");if(!file)return 2;
  if(fread(rom,1,0x200000,file)!=0x200000){fclose(file);free(rom);return 2;}fclose(file);
  for(unsigned base=0;base<8;base++) {
    IssdStadiumGeometry original,output;
    if(!issd_stadium_read_template(rom,0x200000,base,&original)){free(rom);return 1;}
    for(unsigned length=1536;length<=original.length;length+=32) {
      IssdStadiumProfile profile={.version=1,.base_layout=(uint16_t)base,
        .length_units=(uint16_t)length,.width_units=original.width};
      if(!issd_stadium_geometry_compile(&original,&profile,&output)) {
        char rejected[1024];int size=snprintf(rejected,sizeof rejected,"%s/%u-%u.rejected",argv[2],base,length);
        if(size<0||(size_t)size>=sizeof rejected){free(rom);return 2;}
        file=fopen(rejected,"wb");if(!file){free(rom);return 2;}fclose(file);continue;
      }
      char path[1024];int size=snprintf(path,sizeof path,"%s/%u-%u.bin",argv[2],base,length);
      if(size<0||(size_t)size>=sizeof path){free(rom);return 2;}
      file=fopen(path,"wb");if(!file){free(rom);return 2;}
      for(unsigned layer=0;layer<2;layer++) {
        if(fwrite(output.metatiles[layer],1,8192,file)!=8192||fwrite(output.world_maps[layer],1,4096,file)!=4096){fclose(file);free(rom);return 2;}
      }
      fclose(file);
    }
  }
  free(rom);puts("All native geometry outcomes recorded");return 0;
}
