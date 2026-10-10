#define main fixture_main
#include "test_widescreen_native.c"
#undef main
#include "issd_camera_render.h"
static IssdCameraPiece pieces[4096];
static void replay_actor(unsigned mode,unsigned absent,int x) {
  fixture();word(0x70,mode);word(0x800,0x9000);word(0x802,0x60);
  word(0x808,(uint16_t)x);word(0x80c,80);word(0x814,0xa400);
  word(0x816,2);word(0x818,2);word(0x81e,absent);word(0x830,1);
  rom[0x12401]=0x90;rom[0x12403]=0x80;rom[0x12404]=0x96;
  rom[0xb0000]=32;rom[0xb0022]=32;
  memset(rom+0xb0002,0x11,32);memset(rom+0xb0024,0x22,32);
  rom[0x41000]=1;rom[0x41004]=0x10;
}
int main(void) {
  replay_actor(8,1,540);word(0x80c,280);
  IssdCameraView view={-250,-56,900,336};size_t count=0;
  uint8_t before[0x20000];uint16_t before_vram[0x8000];
  memcpy(before,ram,sizeof before);memcpy(before_vram,ppu.vram,sizeof before_vram);
  issd_widescreen_begin(&ppu,ram,rom,sizeof rom,124);
  assert(issd_widescreen_camera_pieces(&ppu,issd_widescreen_presented_ram(ram),
      rom,sizeof rom,&view,pieces,4096,&count));
  assert(count==1 && pieces[0].x==532 && pieces[0].y==272 && pieces[0].size==16);
  assert(!memcmp(before,ram,sizeof before));
  issd_widescreen_end(&ppu);
  assert(!memcmp(before_vram,ppu.vram,sizeof before_vram));
  replay_actor(8,1,-100);word(0x80c,(uint16_t)-40);
  issd_widescreen_begin(&ppu,ram,rom,sizeof rom,71);
  assert(issd_widescreen_camera_pieces(&ppu,issd_widescreen_presented_ram(ram),
      rom,sizeof rom,&view,pieces,4096,&count));
  assert(count==1 && pieces[0].x==-108 && pieces[0].y==-48);
  assert(!issd_widescreen_camera_pieces(&ppu,issd_widescreen_presented_ram(ram),
      rom,sizeof rom,&view,pieces,0,&count));
  issd_widescreen_end(&ppu);
  fixture();word(0x9a0,0x4040);word(0x9a8,48);word(0x9ac,(uint16_t)-8);
  word(0x9be,1);word(0x4040,1);word(0x6040,4);word(0x8040,5);word(0xa040,0);
  assert(issd_widescreen_camera_pieces(&ppu,ram,rom,sizeof rom,&view,pieces,4096,&count));
  assert(count==0);issd_widescreen_end(&ppu);
  /* Native admission establishes identity before a genuine vertical cull. */
  word(0x1d40,0x9a0);word(0x9ac,80);
  assert(issd_widescreen_camera_pieces(&ppu,ram,rom,sizeof rom,&view,pieces,4096,&count));
  assert(count==1);issd_widescreen_end(&ppu);
  word(0x1d40,0);word(0x9ac,(uint16_t)-40);
  assert(issd_widescreen_camera_pieces(&ppu,ram,rom,sizeof rom,&view,pieces,4096,&count));
  assert(count==1);issd_widescreen_end(&ppu);
  issd_widescreen_rebase(&ppu,ram);
  assert(issd_widescreen_camera_pieces(&ppu,ram,rom,sizeof rom,&view,pieces,4096,&count));
  assert(count==0);issd_widescreen_end(&ppu);
  return 0;
}
