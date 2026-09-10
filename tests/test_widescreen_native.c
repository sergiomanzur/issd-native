#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "snes/ppu.h"
#include "issd_widescreen.h"

static Ppu ppu;
static uint8_t ram[0x20000], rom[0x300000];
static void word(unsigned a, unsigned v) { ram[a] = v; ram[a+1] = v >> 8; }
/* Isolate game-specific preparation from the rasterizer in this unit test. */
void PpuSetExtraSpace(Ppu *p, uint16_t n) { p->extraLeftRight = p->extraLeftCur = p->extraRightCur = n; }
void PpuSetExtraSpaceCentered(Ppu *p, uint16_t n) { p->extraLeftRight = n; p->extraLeftCur = p->extraRightCur = 0; }
void PpuSetExtraSideSpace(Ppu *p,int l,int r,int b) { p->extraLeftCur=l;p->extraRightCur=r;p->extraBottomCur=b; }
void PpuSetWidescreenLayerClamp(Ppu *p, uint8_t n) { p->wsLayerClamp = n; }
void PpuSetWidescreenLayerMask(Ppu *p, uint8_t n) { p->wsLayerWidenMask = n; }
void PpuSetWidescreenWindowExpansion(Ppu *p, uint8_t l, uint8_t w) { p->wsWindowExpandLayers = l; p->wsWindowExpandWindows = w; }
void PpuSetWidescreenLayerClampBand(Ppu *p, uint8_t l, uint8_t a, uint8_t b) { p->wsClampY0[l] = a; p->wsClampY1[l] = b; }
void PpuWsSetOamLeftHints(Ppu *p, const uint8_t *h) { p->wsOamLeftHintStrict = h != NULL; if(h) memcpy(p->wsOamLeftHint,h,16); }
void PpuWsSetOamRightHints(Ppu *p, const uint8_t *h) { p->wsOamRightHintStrict = h != NULL; if(h) memcpy(p->wsOamRightHint,h,16); }

static void fixture(void) {
  memset(&ppu,0,sizeof(ppu)); memset(ram,0,sizeof(ram)); memset(rom,0,sizeof(rom));
  issd_widescreen_reset();
  /* $70 selects the subsystem action: 0x08 is live match play. Pitch detection
   * requires it, so a fixture that leaves it at 0 is a menu, not a pitch. */
  ram[0x32]=6; ram[0x70]=0x08; ram[0x50]=1;
  ppu.bgmode=1; ppu.bgXsc[0]=3; ppu.bgXsc[1]=0x13;
  ppu.hScroll[0]=0x110; ppu.hScroll[1]=0x100;
  ppu.vScroll[0]=0x110; ppu.vScroll[1]=0x100;
  word(0x1ffcc,0x200);
  /* Every world metatile is #1; its sixteen constituent tiles are distinct. */
  memset(ram+0x1d000,1,0x1000); memset(ram+0x1e000,1,0x1000);
  for(int i=0;i<16;i++) { word(0x18020+i*2,0x2000+i); word(0x1a020+i*2,0x2400+i); }
  for(int i=0;i<0x2000;i++) ppu.vram[i]=0xdead;
  for(int i=0;i<128;i++) ppu.oam[i*2]=0xf0f0;
}
int main(void) {
  fixture();
  assert(issd_widescreen_pitch_layout(&ppu,ram));
  ram[0x32]=1; assert(!issd_widescreen_pitch_layout(&ppu,ram)); ram[0x32]=6;
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  /* BG2 world (248,256) wraps to left nametable, expected last column of #1. */
  assert(ppu.vram[0x1800+31] == 0x2403);
  /* Center is never rewritten by presentation tile streaming. */
  assert(ppu.vram[0x1c00] == 0xdead);
  assert(ppu.wsLayerClamp & 4);
  assert(ppu.wsLayerWidenMask == 3);
  assert(ppu.wsWindowExpandLayers == 3);
  assert(ppu.wsWindowExpandWindows == 3);
  issd_widescreen_end(&ppu);
  assert(ppu.vram[0x1800+31] == 0xdead);
  assert(!issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),0));

  fixture();
  /* One ROM-backed ball/player part entirely beyond the original right edge. */
  word(0x1d40,0x500); word(0x500,0x9000); word(0x508,280); word(0x50c,80);
  rom[0x41000]=1; rom[0x41001]=0; rom[0x41002]=0;
  rom[0x41003]=0x24; rom[0x41004]=0x10;
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  int found=0;
  for(int i=0;i<128;i++) if ((ppu.oam[2*i]>>8)==72 && (ppu.oam[2*i]&255)==16) found++;
  assert(found == 1);
  issd_widescreen_end(&ppu);
  for(int i=0;i<128;i++) assert(ppu.oam[2*i]==0xf0f0);

  fixture(); ppu.bgmode=9;
  /* The center can be on the fifth world page despite a ten-bit PPU scroll. */
  word(0x13a0,0x510);word(0x13c0,0x500);
  memset(ram+0x1e000,0,0x1000);ram[0x1e000+0x200+4*64+7]=1;
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  assert(ppu.vram[0x1800+31]==0x2403);
  issd_widescreen_end(&ppu);

  fixture();
  /* Whole players omitted from the sorted native draw list still have poses. */
  word(0xd00,0x4040);word(0xd08,327);word(0xd0c,80);word(0xd1e,1);
  word(0x4040,1);word(0x6040,0);word(0x8040,0);word(0xa040,0x24);
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),95);
  found=0;
  for(int i=0;i<128;i++) if ((ppu.oam[2*i]>>8)==76 && (ppu.oam[2*i]&255)==67) found++;
  assert(found==1);
  assert(ram[0xd1e]==1);
  issd_widescreen_end(&ppu);

  fixture();
  /* Whole-object supplements must draw all intersecting pieces, even if an
   * omitted player's leftmost piece starts inside the native 4:3 columns. The
   * original draw list has no native copy to preserve in this case, so skipping
   * x<256 pieces makes players pop in at the 4:3 boundary. */
  word(0xd00,0x4040);word(0xd08,292);word(0xd0c,80);word(0xd1e,1);
  word(0x4040,1);word(0x6040,(uint16_t)-40);word(0x8040,0);word(0xa040,0x24);
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  found=0;
  for(int i=0;i<128;i++) if ((ppu.oam[2*i]>>8)==76 && (ppu.oam[2*i]&255)==248) found++;
  assert(found==1);
  assert(ram[0xd1e]==1);
  issd_widescreen_end(&ppu);

  /* The same omitted whole object moves smoothly through the left margin. */
  issd_widescreen_reset();
  word(0x6040,0);
  word(0xd08,(uint16_t)-48);
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  found=0;
  for(int i=0;i<128;i++) if ((ppu.oam[2*i]>>8)==76 && (ppu.oam[2*i]&255)==204) {
    assert(ppu.wsOamLeftHint[i/8] & (1 << (i%8))); found++;
  }
  assert(found==1);
  issd_widescreen_end(&ppu);

  fixture();
  /* Regression: the cartridge DMAs the OAM built by the PREVIOUS logic pass, so
   * the supplement must reconstruct from that same generation. An object listed
   * in the native draw list that sat in the margin last frame and stepped inside
   * the native columns this frame has no native copy to defer to: reconstructing
   * from this frame's records dropped it entirely and the ball blinked out. */
  word(0x1d40,0x500); word(0x500,0x9000); word(0x508,280); word(0x50c,80);
  rom[0x41000]=1; rom[0x41001]=0; rom[0x41002]=0;
  rom[0x41003]=0x24; rom[0x41004]=0x10;
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  issd_widescreen_end(&ppu);
  word(0x508,250);                       /* steps inside the native columns */
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  found=0;
  for(int i=0;i<128;i++) if ((ppu.oam[2*i]>>8)==72 && (ppu.oam[2*i]&255)==16) found++;
  assert(found==1);                      /* still drawn, at last frame's x=280 */
  issd_widescreen_end(&ppu);

  fixture();ppu.hScroll[0]=16;ppu.hScroll[1]=0;
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),95);
  assert(ppu.extraLeftCur==95 && ppu.extraRightCur==95);
  issd_widescreen_end(&ppu);
  puts("widescreen native tests passed");
}
