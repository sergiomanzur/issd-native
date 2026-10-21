/* Exercise the actual PPU rasterizer with the runner's standalone stubs. */
#include "snes/ppu.h"
#include "snes/snes.h"
#include "snes/ws_shadow.h"
Snes *g_snes;
int snes_frame_counter;
unsigned char g_snesrecomp_last_hdmaen;
/* Shadow maps are a separate runner feature, disabled in this fixture. */
uint16_t WsShadowTile(int layer,int x,uint32_t y,uint16_t scroll,
                      uint16_t address,uint16_t tile) {
  (void)layer;(void)x;(void)y;(void)scroll;(void)address;return tile;
}
bool WsShadowLayerActive(int layer) { (void)layer;return false; }
uint32_t WsShadowWorldX(int layer) { (void)layer;return 0; }
uint32_t WsShadowPresentWorldY(int layer,int x) { (void)layer;(void)x;return 0; }
uint32_t WsShadowScrollY(int layer) { (void)layer;return 0; }
void WsShadowOnVramWrite(uint16_t address,uint16_t value) { (void)address;(void)value; }
#include "../deps/snesrecomp/runner/src/snes/ppu.c"
#include <assert.h>

static Ppu p;
static PpuPixelPrioBufs output;

static void draw(unsigned y) {
  memset(&output, 0, sizeof(output));
  PpuDrawBackground_2bpp(&p, &output, y, false, 2, 0x100, 0x100);
}

static unsigned ink(int x) { return output.data[x + kPpuExtraLeftRight] & 255; }

static void test_weather(void) {
  memset(&p, 0, sizeof(p));
  p.screenEnabled[0] = 4;
  p.bgXsc[2] = 0x10; /* BG3 map at word $1000, graphics at zero */
  PpuSetExtraSpace(&p, 71);
  PpuSetWidescreenLayerClamp(&p, 4);
  for (unsigned y=0;y<8;y++) {
    p.vram[0x1f0 * 8 + y] = 0xff; /* rain: color 1 */
    p.vram[0x1ea * 8 + y] = 0xff00; /* HUD: color 2 */
  }
  for (unsigned row=0;row<32;row++) for (unsigned col=0;col<32;col++)
    p.vram[0x1000 + row*32 + col] = col & 1 ? 0x1ea : 0x1f0;
  draw(40);
  assert(ink(-16)==0 && ink(272)==0 && ink(0)==1 && ink(8)==2);
  PpuSetWidescreenMarginTiles(&p, 2, 0x1f0, 0x1f9);
  draw(40);
  assert(ink(-16)==1 && ink(272)==1);
  assert(ink(-8)==0 && ink(264)==0); /* no duplicated HUD glyphs */
  assert(ink(0)==1 && ink(8)==2); /* authentic center unchanged */
  /* Top and bottom HUD bands borrow weather rows only in the margins. */
  for (unsigned row=0;row<32;row++) for (unsigned col=0;col<32;col++)
    p.vram[0x1000+row*32+col] = row>=4 && row<=19 ? 0x1f0 : 0x1ea;
  PpuSetWidescreenMarginRows(&p, 4, 19);
  draw(0); assert(ink(-16)==1 && ink(272)==1 && ink(0)==2);
  draw(208); assert(ink(-16)==1 && ink(272)==1 && ink(0)==2);
  PpuSetWidescreenMarginTiles(&p, -1, 0, 0);
  draw(40); assert(ink(-16)==0 && ink(272)==0);
}

static bool masked(const PpuWindows *w, int x) {
  for (unsigned i=0;i<w->nr;i++)
    if (x>=w->edges[i] && x<w->edges[i+1]) return (w->bits & (1u<<i))!=0;
  return false;
}

static void test_banners(void) {
  memset(&p,0,sizeof(p));
  PpuSetExtraSpace(&p,71);
  p.windowsel=(uint32_t)kWindow1Enabled << 20;
  p.window1left=1; p.window1right=255;
  PpuWindows w;
  PpuWindows_CalcWithExtra(&w,&p,5,50,71,71);
  assert(!masked(&w,-20) && !masked(&w,280));
  assert(masked(&w,1) && masked(&w,255));
  p.window1left=0;
  PpuWindows_CalcWithExtra(&w,&p,5,50,71,71);
  assert(masked(&w,-20) && masked(&w,280)); /* full-screen effects still widen */
}

int main(void) {
  test_weather(); test_banners();
  puts("widescreen weather and announcement raster checks passed");
  return 0;
}
