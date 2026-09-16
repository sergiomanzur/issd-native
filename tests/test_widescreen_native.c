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
void PpuSetWidescreenLayerRepeat(Ppu *p, uint8_t n) { p->wsLayerRepeat = n; }
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

static void test_offscreen_player_graphics(void) {
  fixture();
  /* $84E6C7 records the animation descriptor before skipping an offscreen
   * player's pose and two graphics uploads. VRAM still contains menu tiles. */
  word(0x800, 0); word(0x802, 0x0060);
  word(0x808, (uint16_t)-48); word(0x80c, 80);
  word(0x814, 0xa400); word(0x81e, 1); word(0x830, 1);
  rom[0x12400]=0x00; rom[0x12401]=0x90; /* bank $88 pose $9000 */
  rom[0x12402]=0x00; rom[0x12403]=0x80; rom[0x12404]=0x96;
  rom[0xb0000]=32; rom[0xb0022]=32; /* two length-prefixed tile rows */
  memset(rom+0xb0002, 0x12, 32); memset(rom+0xb0024, 0x34, 32);
  rom[0x41000]=1; rom[0x41003]=0; rom[0x41004]=0x10;
  ppu.vram[0x6600]=0xbeef; ppu.vram[0x6700]=0xcafe;
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(ppu.vram[0x6600]==0x1212 && ppu.vram[0x6700]==0x3434);
  assert((ppu.oam[0] & 255)==200 && (ppu.oam[0] >> 8)==72);
  assert((ppu.oam[1] & 255)==0x60);
  assert(ram[0x800]==0 && ram[0x81e]==1); /* presentation only */
  issd_widescreen_end(&ppu);
  assert(ppu.vram[0x6600]==0xbeef && ppu.vram[0x6700]==0xcafe);

  /* Real player descriptors use decoded RAM geometry. The descriptor's new
   * pose must replace the stale pose, and kit details follow the same frame. */
  static const int extras[] = {32,51,71,95};
  for (unsigned i=0;i<sizeof(extras)/sizeof(extras[0]);i++) {
    issd_widescreen_reset();
    word(0x808,(uint16_t)-20); word(0x800,0x9000); word(0x830,2);
    rom[0x12400]=0x40; rom[0x12401]=0x40; rom[0x12405]=0x81;
    word(0x4040,0xff01); word(0x6040,0); word(0x8040,0); word(0xa040,0);
    rom[0xce8a]=0x00; rom[0xce8b]=0x90;
    memset(rom+0xc1000,0x56,32); /* bank $98:$9000 */
    assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),extras[i]));
    assert(ppu.vram[0x6770]==0x5656);
    assert((ppu.oam[0]&255)==228 && (ppu.oam[0]>>8)==72);
    assert(ppu.highOam[0]&2);
    issd_widescreen_end(&ppu);
  }

  /* Type 8 has an additional shared detail upload ($84E842). */
  issd_widescreen_reset(); word(0x830,8); rom[0x12405]=2;
  rom[0xceee]=0x00; rom[0xceef]=0x90;
  memset(rom+0x121000,0x78,64); /* bank $A4:$9000 */
  ppu.vram[0x7fe0]=0xabcd;
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  assert(ppu.vram[0x7fe0]==0x7878);
  issd_widescreen_end(&ppu);
  assert(ppu.vram[0x7fe0]==0xabcd);

  /* A rejected second row must neither partially upload nor draw stale tiles. */
  issd_widescreen_reset(); rom[0xb0023]=2;
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  assert(ppu.vram[0x6600]==0xbeef && ppu.oam[0]==0xf0f0);
  issd_widescreen_end(&ppu);

  issd_widescreen_reset();
  assert(!issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),0));
  assert(ppu.vram[0x6600]==0xbeef && ppu.oam[0]==0xf0f0);
}

static void test_padded_stadium_edge(void) {
  static const int extras[] = {32,51,71,95};
  for (unsigned width=0;width<sizeof(extras)/sizeof(extras[0]);width++) {
  fixture();
  /* The page stride includes zero padding after the authored stadium. Its
   * blank metatile must not turn the added corner view into a green block. */
  for (unsigned layer=0; layer<2; layer++) {
    memset(ram+0x1d000+layer*0x1000,0,0x1000);
    for (unsigned y=0;y<4;y++)
      memset(ram+0x1d000+layer*0x1000+y*0x200,1,128);
    ppu.hScroll[layer]=256; ppu.vScroll[layer]=256;
  }
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),extras[width]));
  assert(ppu.vram[0x800]==0x2003); /* world x512: extend outer tile column */
  assert(ppu.vram[0x1800]==0x2403);
  assert(ppu.vram[0xc00]==0xdead); /* native columns unchanged */
  issd_widescreen_end(&ppu);
  assert(ppu.vram[0x800]==0xdead && ppu.vram[0x1800]==0xdead);
  }

  fixture();
  /* Leading padding, including negative world coordinates at the left edge. */
  for (unsigned layer=0;layer<2;layer++) {
    for(unsigned page=0;page<8;page++)
      for(unsigned row=0;row<8;row++) {
        ram[0x1d000+layer*0x1000+page*0x200+row*8]=0;
        ram[0x1d001+layer*0x1000+page*0x200+row*8]=0;
      }
    ppu.hScroll[layer]=64; ppu.vScroll[layer]=256;
  }
  issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71);
  assert(ppu.vram[0xc1f]==0x2000 && ppu.vram[0x1c1f]==0x2400);
  assert(ppu.vram[0x808]==0xdead);
  issd_widescreen_end(&ppu);
}
int main(void) {
  fixture();
  assert(issd_widescreen_pitch_layout(&ppu,ram));
  ram[0x32]=1; assert(!issd_widescreen_pitch_layout(&ppu,ram)); ram[0x32]=6;
  /* The coin toss runs on the pitch, so stride, $50, the BG mode and both
   * tilemap bases all look exactly like live play. Only the submode tells
   * them apart, and treating it as a pitch built side margins out of
   * metatile maps the game had not populated yet. */
  ram[0x70]=0x1C; assert(!issd_widescreen_pitch_layout(&ppu,ram));
  assert(!issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));  /* pillarboxed */
  assert(ppu.extraLeftCur==0 && ppu.extraRightCur==0);          /* black bars */
  ram[0x70]=0x08; issd_widescreen_reset();
  assert(issd_widescreen_pitch_layout(&ppu,ram));

  /* Menus: the wallpaper on BG2 is repeated into the margins instead of
   * pillarboxing them. It must trigger whether the screen composites BG2
   * through the main screen (main menu) or the sub screen (scenario select,
   * which runs main=0x10 sub=0x07 for colour math), and must never trigger
   * on the title screen, whose HDMA mode 3 split is not a menu. */
  ram[0x70]=0x00; ram[0x32]=6;
  word(0x1ffcc, 0);            /* a menu has no stadium loaded */
  ppu.screenEnabled[0]=0x17; ppu.screenEnabled[1]=0x00;
  assert(issd_widescreen_menu_layout(&ppu,ram));
  ppu.screenEnabled[0]=0x10; ppu.screenEnabled[1]=0x07;
  assert(issd_widescreen_menu_layout(&ppu,ram));
  ppu.screenEnabled[0]=0x11; ppu.screenEnabled[1]=0x00;   /* no BG2 at all */
  assert(!issd_widescreen_menu_layout(&ppu,ram));
  ppu.screenEnabled[0]=0x17;
  ram[0x32]=1; assert(!issd_widescreen_menu_layout(&ppu,ram));  /* title */
  ram[0x32]=0; assert(!issd_widescreen_menu_layout(&ppu,ram));  /* boot */
  /* The pre-match presentation is mode 6 with BG2 and is not a pitch, but
   * it does have a stadium loaded and only its framed centre should show.
   * Treating it as a menu repeated grass into its margins. */
  word(0x1ffcc, 0x200);
  ram[0x70]=0x0F; assert(!issd_widescreen_pitch_layout(&ppu,ram));
  assert(!issd_widescreen_menu_layout(&ppu,ram));
  /* The title screen takes its own backdrop colour in the margins rather
   * than black bars. Every layer is clamped so the HDMA mode 3 split and the
   * windowed photo frames are left exactly as the cartridge drew them, and it
   * must never be mistaken for a menu or a pitch. */
  ram[0x32]=1; ram[0x70]=0x00; word(0x1ffcc, 0);
  assert(issd_widescreen_title_layout(&ppu,ram));
  assert(!issd_widescreen_menu_layout(&ppu,ram));
  assert(!issd_widescreen_pitch_layout(&ppu,ram));
  issd_widescreen_reset();
  assert(!issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(ppu.extraLeftCur==71 && ppu.extraRightCur==71);  /* widened, not barred */
  assert(ppu.wsLayerClamp == 0x0F);                       /* nothing extends */
  ram[0x32]=6;
  assert(!issd_widescreen_title_layout(&ppu,ram));        /* menus are not it */
  word(0x1ffcc, 0x200);        /* restore the stadium the fixture set up */
  issd_widescreen_reset();

  ram[0x32]=6; ram[0x70]=0x08;
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
  test_offscreen_player_graphics();
  test_padded_stadium_edge();
  puts("widescreen native tests passed");
}
