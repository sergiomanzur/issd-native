/* Compare editor-authored identity with the actual compositor's key reader. */
#include "../ISSDNative/issd_hd.c"
#include <assert.h>
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  FILE *file=fopen(argv[1],"rb");if(!file)return 2;
  uint16_t words[32];if(fread(words,sizeof words,1,file)!=1){fclose(file);return 2;}fclose(file);
  Ppu *ppu=calloc(1,sizeof *ppu);assert(ppu);
  memcpy(ppu->vram+0x4000,words,32);memcpy(ppu->cgram+32,words+16,32);
  uint64_t key=hd_tile_key(ppu,0x2000,512,4,32);
  uint64_t expected=strtoull(argv[2],NULL,16);
  free(ppu);assert(key==expected);puts("Editor HD identity matches native compositor");return 0;
}
