#define main fixture_main
#include "test_widescreen_native.c"
#undef main
#include "issd_camera_render.h"
static uint32_t pixels[756*336];
static uint32_t art[64];
static const uint32_t *lookup(const Ppu *p,unsigned b,unsigned c,unsigned pal,int *size) {
  (void)p;(void)b;(void)c;(void)pal;*size=8;return art;
}
int main(void) {
  fixture();ppu.inidisp=15;ppu.screenEnabled[0]=1;ppu.bgTileAdr=2;
  memset(ram+0x1d000,1,0x1000);
  for(int i=0;i<16;i++) word(0x18020+i*2,1);
  for(int row=0;row<8;row++) ppu.vram[0x2010+row]=0x00ff;
  ppu.cgram[1]=31;
  /* A distinct world column 512 pixels away catches map-ring aliasing. */
  ram[0x1d000+2*64]=2;
  for(int i=0;i<16;i++) word(0x18040+i*2,2);
  for(int row=0;row<8;row++) ppu.vram[0x2020+row]=0xff00;
  ppu.cgram[2]=31<<10;
  word(0x13a0,0);word(0x13b0,0);ppu.hScroll[0]=ppu.vScroll[0]=0;
  uint8_t before[0x20000];uint16_t vram[0x8000];
  memcpy(before,ram,sizeof before);memcpy(vram,ppu.vram,sizeof vram);
  IssdCameraView view={0,0,756,336};
  assert(issd_camera_render_world(&ppu,ram,&view,pixels,756));
  assert(pixels[0]==0xff0000);
  assert(pixels[512]==0x0000ff);
  assert(pixels[300*756]==0xff0000);
  assert(!memcmp(before,ram,sizeof before));
  assert(!memcmp(vram,ppu.vram,sizeof vram));
  /* Horizontal flip and palette selection preserve individual texels. */
  ppu.vram[0x2010]=0x0080;
  word(0x18020,1|0x4000);
  assert(issd_camera_render_world(&ppu,ram,&view,pixels,756));
  assert(pixels[7]==0xff0000 && pixels[0]==0);
  /* The pitch uses an empty window with math restricted to its interior. */
  word(0x18020,1);ppu.vram[0x2010]=0xff;
  ppu.cgadsub=0x41;ppu.fixedColor=31<<10;ppu.cgwsel=0x10;
  ppu.windowsel=0x200000;ppu.window1left=1;ppu.window1right=0;
  assert(issd_camera_render_world(&ppu,ram,&view,pixels,756));
  assert(pixels[0]==0xff0000 && pixels[512]==0x0000ff);
  /* Sprite-based flags and radar dots remain at native HUD size/position. */
  ppu.screenEnabled[0]|=16;ppu.oam[0]=(5u<<8)|20;ppu.oam[1]=1;ppu.oam[2]=(24u<<8)|160;ppu.oam[3]=1;
  memset(ppu.vram+16,0,16*sizeof(uint16_t));
  for(int row=0;row<8;row++) ppu.vram[16+row]=0xff;
  ppu.cgram[129]=31<<10;
  assert(issd_camera_prepare(&ppu,ram,rom,sizeof rom,ISSD_CAMERA_TACTICAL,398,224));
  assert(issd_camera_compose(&ppu,ram,pixels,NULL,398,224));
  assert(pixels[5*398+91]==0x0000ff);
  assert(pixels[24*398+231]==0x0000ff);
  ppu.screenEnabled[0]=1;
  static uint32_t reference[498*280],composed[398*224];
  IssdCameraView tactical=issd_camera_view(398,224,ISSD_CAMERA_TACTICAL);
  assert(issd_camera_render_world(&ppu,ram,&tactical,reference,498));
  assert(issd_camera_compose(&ppu,ram,composed,NULL,398,224));
  for(int y=0;y<224;y++) for(int x=0;x<398;x++)
    assert(composed[y*398+x]==reference[(y*280/224)*498+x*498/398]);
  ppu.screenWindowed[0]=1;ppu.windowsel=2;ppu.window1left=64;ppu.window1right=128;
  ppu.cgwsel=0;ppu.cgadsub=0;
  assert(issd_camera_render_world(&ppu,ram,&view,pixels,756));
  assert(pixels[80]==0 && pixels[160]==0xff0000);
  ppu.screenWindowed[0]=0;
  for(int i=0;i<64;i++) art[i]=0xff00ff00;
  issd_camera_set_art_lookup(lookup);ppu.cgwsel=0xc0;
  assert(issd_camera_compose(&ppu,ram,composed,NULL,398,224));
  assert(composed[100*398+160]==0);
  issd_camera_set_art_lookup(NULL);ppu.cgwsel=0;ppu.screenEnabled[0]=16;
  ppu.cgram[129]=31;ppu.cgram[130]=31<<10;
  for(int row=0;row<8;row++) {ppu.vram[16+row]=0xff;ppu.vram[32+row]=0xff00;}
  ppu.oam[0]=ppu.oam[2]=(80u<<8)|80;ppu.oam[1]=0x1001;ppu.oam[3]=0x3002;
  assert(issd_camera_compose(&ppu,ram,composed,NULL,398,224));
  assert(composed[89*398+163]==0xff0000);
  /* A native slot preceding a matched actor still wins the overlap. */
  word(0x400,0x4040);word(0x402,0x3000);word(0x408,84);word(0x40c,84);
  word(0x4040,1);word(0x6040,0);word(0x8040,0);word(0xa040,2);
  ppu.oam[1]=0x3001;ppu.oam[3]=0x3002;
  assert(issd_camera_prepare(&ppu,ram,rom,sizeof rom,ISSD_CAMERA_TACTICAL,398,224));
  assert(issd_camera_compose(&ppu,ram,composed,NULL,398,224));
  assert(composed[89*398+163]==0xff0000);
  issd_widescreen_end(&ppu);
  fixture();ppu.inidisp=15;ppu.screenEnabled[0]=16;
  memset(ppu.vram,0,128*sizeof(uint16_t));
  word(0x400,0x4040);word(0x402,0x3000);word(0x408,0);word(0x40c,88);word(0x412,88);
  word(0x4040,0x8001);word(0xa040,2);
  word(0x4a0,0x4050);word(0x4a2,0x3000);word(0x4a8,(uint16_t)-9);word(0x4ac,90);word(0x4b2,90);
  word(0x4050,0x8001);word(0xa050,4);
  word(0x1d40,0x400);word(0x1d42,0x4a0);
  ppu.cgram[129]=31;ppu.cgram[130]=31<<10;
  for(int row=0;row<8;row++) {
    ppu.vram[32+row]=ppu.vram[48+row]=0xff00;
    ppu.vram[64+row]=ppu.vram[80+row]=0xff;
  }
  ppu.oam[0]=(80u<<8)|248;ppu.oam[1]=0x3002;ppu.highOam[0]=3;
  assert(issd_camera_prepare(&ppu,ram,rom,sizeof rom,ISSD_CAMERA_TACTICAL,398,224));
  assert(issd_camera_compose(&ppu,ram,composed,NULL,398,224));
  assert(composed[90*398+94]==0xff0000);
  issd_widescreen_end(&ppu);
  /* An enabled math region still samples the subscreen after the fast path. */
  fixture();ppu.inidisp=15;ppu.bgTileAdr=0x22;
  ppu.screenEnabled[0]=1;ppu.screenEnabled[1]=2;
  ppu.cgwsel=2;ppu.cgadsub=1;
  memset(ppu.vram+0x2000,0,48*sizeof(uint16_t));
  for(int row=0;row<8;row++) {ppu.vram[0x2010+row]=0xff;ppu.vram[0x2020+row]=0xff00;}
  ppu.cgram[1]=31;ppu.cgram[2]=31<<10;
  for(int i=0;i<16;i++) {word(0x18020+i*2,1);word(0x1a020+i*2,2);}
  assert(issd_camera_prepare(&ppu,ram,rom,sizeof rom,ISSD_CAMERA_TACTICAL,398,224));
  assert(issd_camera_compose(&ppu,ram,composed,NULL,398,224));
  assert(composed[100*398+200]==0xff00ff);
  issd_widescreen_end(&ppu);
  /* Dedicated penalty and management layouts retain original scanout. */
  word(0x70,0x0c);ppu.bgmode=9;ppu.bgXsc[0]=1;ppu.bgXsc[1]=0x10;
  assert(!issd_camera_prepare(&ppu,ram,rom,sizeof rom,ISSD_CAMERA_TACTICAL_WIDE,398,224));
  word(0x70,0x14);ppu.bgmode=1;
  assert(!issd_camera_prepare(&ppu,ram,rom,sizeof rom,ISSD_CAMERA_TACTICAL,398,224));
  return 0;
}
