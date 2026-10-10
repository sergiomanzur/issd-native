#include "issd_widescreen.h"
#include "issd_pose_history.h"
#include "issd_animation.h"
#include "issd_camera.h"
#include "snes/ppu.h"
#include <string.h>

/* The original streamers ($8B85E3/$8B86E9) maintain a 512x512 ring
 * with only 32 pixels of lookahead. Reconstruct its additional visible
 * columns from the same decompressed 32x32 world metatiles. The transaction
 * never changes WRAM, camera bounds, activation, or game-visible VRAM/OAM. */
static struct {
  Ppu *owner;
  uint16_t vram[0x8000], oam[0x100];
  uint8_t high_oam[0x20];
  uint8_t ram[0x20000];
  uint16_t descriptors[22];
  uint16_t replay_directory;
} frame;

static uint16_t word(const uint8_t *p, unsigned a) {
  return (uint16_t)(p[a] | p[a + 1] << 8);
}

bool issd_widescreen_pitch_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  /* $80846C: 3=demo, 6=menus/game. $50 enables the match OAM builder.
   * Submode in $70, measured across a full 6000 frame match:
   *   0x00..0x07  menus and stadium select
   *   0x08        live match play (5173 of 5236 in-match frames)
   *   0x09, 0x0B  brief in-match states, left enabled
   *   0x1C        pre-match coin toss
   *
   * The coin toss happens on the pitch, so every other pitch signal is set
   * and it passed the >= 0x08 test, leaving the reconstruction to build side
   * margins from metatile maps the game has not populated yet: broken grass
   * either side. It is pillarboxed by submode instead. */
  unsigned mode = word(ram, 0x32);
  unsigned submode = word(ram, 0x70);
  if (mode != 3 && mode != 6) return false;
  if (submode < 0x08) return false;
  /* Pre-match presentation: the stadium fly-in, the coin toss and the
   * framed pitch view. It all happens on the pitch, so stride, $50, the BG
   * mode and both tilemap bases read exactly like live play, but the world
   * metatile maps hold nothing for the widened columns yet. Reconstructing
   * them painted grass and crowd rows either side of the framed view.
   *
   * Measured across a full Open Game from boot to kickoff, 0x0F covers 1061
   * frames of that sequence and 0x1C one more; an earlier fix excluded only
   * 0x1C, which is why the coin toss stayed broken.
   * Submode 0x12 is the half-time stats screen (CODE_80B2C2). It uses mode 6
   * and stadium stride, but is a framed stats card where metatiles should not
   * leak into the side margins as green lines. */
  if (submode == 0x0F || submode == 0x10 || submode == 0x12 || submode == 0x1C) return false;
  /* A previous guard tested $38 == 0xC4E4 && $3A == 0x8B here. Neither holds
   * during the coin toss: $38 is 0 throughout and $3A is a frame counter, so
   * the guard never once fired. Removed rather than left as reassurance. */

  unsigned stride = word(ram, 0x1ffcc);
  return word(ram, 0x50) != 0 &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 3 &&
         ppu->bgXsc[1] == 0x13 && stride >= 0x80 &&
         stride <= 0x340 && (stride & 63) == 0;
}

/* Goal-facing penalties use their own static background layout, not the
 * stadium world maps: BG1 is the one goal/net, BG2 wraps the authored crowd
 * and grass, and BG3 holds the team/name/status panels. Match only this
 * verified layout so pre-match cards and ordinary pitch scenes stay guarded. */
static bool penalty_layout(const Ppu *ppu, const uint8_t *ram) {
  unsigned mode = word(ram, 0x32);
  unsigned submode = word(ram, 0x70);
  return (mode == 3 || mode == 6) &&
         (submode == 0x0c || submode == 0x11) &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 1 &&
         ppu->bgXsc[1] == 0x10 && ppu->bgXsc[2] == 9 &&
         ppu->bgTileAdr == 0x4522;
}

/* CODE_83B165 loads the introduction stadium ($86=8), whose sky, stands
 * and close-up coin pitch are authored tile backgrounds. Retail banner/coin captures
 * share this layout within the cartridge presentation stages $72=9..$1C. They have no live world-map continuation:
 * BG1 owns both the crowd and the single TV; BG2 is its underlying pitch.
 * Continue only BG1's outer crowd columns, retaining the 256-pixel clip on
 * the TV/pitch, OBJ and BG3. Repeating a whole background clones the TV. */
bool issd_widescreen_coin_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  unsigned stage = word(ram, 0x72);
  return word(ram, 0x32) == 6 && word(ram, 0x70) == 0x0f &&
         word(ram, 0x86) == 8 && stage >= 9 && stage <= 0x1c &&
         word(ram, 0x1ffcc) == 0x80 &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 3 &&
         ppu->bgXsc[1] == 0x13 && ppu->bgXsc[2] == 0x58 &&
         ppu->bgTileAdr == 0x4522 && ppu->hScroll[0] == 128 &&
         ((ppu->screenEnabled[0] | ppu->screenEnabled[1]) & 3) != 0;
}

/* Authored BG1 map coordinates, independent of its vertical camera scroll:
 * TV/wall rows5..23 occupy cols19..45, leaving three crowd tiles left and
 * two right. Use only two left tiles too: the third reaches the TV shadow.
 * Wall rows12..15 also put shadow in the second left tile, so continue only
 * their first plain8px wall tile. Other rows admit128px strips. Close fans
 * rows24..28 use96px
 * strips containing four complete24px people, with the left phase joining
 * the native partial person at x0 (1B9/1BA/1BB and its following body rows).
 * Read every source from the untouched native cols16..47; only host margin
 * columns change, and End restores the private scanout transaction. */
static void fill_coin_crowd(Ppu *ppu) {
  static const uint16_t tv_edge[18]={0x11d,0x110,0x126,0x126,0x129,0x12a,
    0x12a,0x126,0x137,0x126,0x12a,0x12a,0x129,0x126,0x126,0x12a,0x129,0x151};
  /* This ring previously contains clouds/floodlights at these same rows.
   * Classify streamed artwork, never the row number alone. Ordered fan body
   * and head triplets distinguish close people from reused sky tile IDs. */
  bool fans_loaded=true;
  for (unsigned i=0;i<3;i++) {
    fans_loaded &= (ppu->vram[24*32+18+i]&0x3ff)==0x1a1+i;
    fans_loaded &= (ppu->vram[25*32+18+i]&0x3ff)==0x1a4+i;
  }
  for (unsigned y=0; y<64; y++) {
    unsigned row=(y&31)*32+(y>>5)*0x800;
    uint16_t native[32];
    for (unsigned x=0;x<32;x++)
      native[x]=ppu->vram[row+((x+16)&31)+((x+16)>>5)*0x400];
    for (unsigned x=0; x<16; x++) {
      unsigned left=x, right=16+x;
      bool tv_loaded=(y>=5 && y<=22 &&
        (native[3]&0x3ff)==tv_edge[y-5]) ||
        (y==23 && (ppu->vram[22*32+19]&0x3ff)==0x151);
      if (tv_loaded) {
        left=(y>=12 && y<=15) ? 0 : x%2;
        right=30+x%2;
      }
      else if (fans_loaded && y>=24 && y<=28) { left=14+x%12; right=20+x%12; }
      ppu->vram[row+x]=native[left];
      ppu->vram[row+0x400+16+x]=native[right];
    }
  }
}

/* $8BAB77/$8BAC27 load the stadium and put the statistics on BG3.
 * The native ring has no guaranteed lookahead beyond its centered view.
 * Extend its outer scenery tiles, leaving the single stats card and OBJ
 * clipped. This does not admit extra players or modify the match state. */
bool issd_widescreen_stats_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  unsigned stage = word(ram, 0x72);
  return word(ram, 0x32) == 6 && word(ram, 0x70) == 0x12 &&
         stage >= 3 && stage <= 8 &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 3 &&
         ppu->bgXsc[1] == 0x13 && ppu->bgXsc[2] == 0x5a &&
         ppu->bgTileAdr == 0x4522 && ppu->hScroll[0] == 0 &&
         ppu->hScroll[1] == 0 &&
         ((ppu->screenEnabled[0] | ppu->screenEnabled[1]) & 3) == 3;
}

static void fill_stats_edges(Ppu *ppu) {
  for (unsigned layer = 0; layer < 2; layer++) {
    unsigned base = PPU_bgTilemapAdr(ppu, layer);
    for (unsigned y = 0; y < 64; y++) {
      unsigned row = base + (y & 31) * 32 + (y >> 5) * 0x800;
      uint16_t left = ppu->vram[row], right = ppu->vram[row + 31];
      for (unsigned x = 0; x < 16; x++) {
        ppu->vram[row + 0x400 + x] = right;
        ppu->vram[row + 0x400 + 16 + x] = left;
      }
    }
  }
}

/* Menus are pillarboxed unless they are one of the screens built the way the
 * cartridge builds all of them: BG2 a tiling wallpaper, BG1 the panels and
 * BG3 or sprites the text. Verified by isolating layers on the main menu and
 * the scenario select, which share the layering exactly.
 *
 * Only BG2 is extended, and only by repeating what is already on screen. The
 * wallpaper is a repeating pattern, so it tiles into the margins seamlessly;
 * nothing is scaled and no artwork is invented. Panels and text stay at their
 * authored positions in the middle 256 pixels. */
bool issd_widescreen_menu_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  unsigned mode = word(ram, 0x32);
  /* Modes 0 and 1 are boot, the Konami logo and the title screen. The title
   * is an HDMA mode 3 split with windowed photo frames; widening its layers
   * would fight that, and it is not a menu. */
  if (mode != 5 && mode != 6) return false;
  if (issd_widescreen_pitch_layout(ppu, ram)) return false;
  /* Campaign/tournament splash cards reuse the menu allocation, but BG2
   * contains the reflected lettering instead of wallpaper. Retail World
   * Series: mode $0C, BG1/BG2/OBJ enabled ($13), BG3 disabled, black backdrop.
   * Repeating BG2 duplicates that foreground into the widened black sides.
   * BG3 remains enabled on wallpaper menus even during their black fades. */
  if (ppu->bgTileAdr == 0x4422 && ppu->cgram[0] == 0 &&
      ((ppu->screenEnabled[0] | ppu->screenEnabled[1]) & 6) == 2)
    return false;
  /* Main, formation and squad menus retain the preceding stadium allocation.
   * Their actual graphics layout, rather than stale world-map stride, owns
   * the blue wallpaper. The close-up stadium uses different tile banks. */
  if ((ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 1 &&
      ppu->bgXsc[1] == 0x10 && ppu->bgXsc[2] == 9 &&
      ppu->bgTileAdr == 0x4422)
    return ((ppu->screenEnabled[0] | ppu->screenEnabled[1]) & 2) != 0;
  /* A menu has no stadium loaded. The pre-match presentation does: it runs
   * on the pitch with a valid metatile stride, and only the framed centre is
   * meant to be visible. Without this it is not a pitch (its submode is
   * excluded above) and it does have BG2, so it fell through to the menu
   * path and had grass and crowd repeated into its margins - the same broken
   * green the pitch reconstruction had been painting there. */
  {
    unsigned stride = word(ram, 0x1ffcc);
    if (stride >= 0x80 && stride <= 0x340 && (stride & 63) == 0) return false;
  }
  /* Some screens composite the wallpaper through the sub screen for colour
   * math and leave only sprites on the main screen: the scenario select runs
   * main=0x10, sub=0x07. Checking the main screen alone missed those. */
  return ((ppu->screenEnabled[0] | ppu->screenEnabled[1]) & (1u << 1)) != 0;
}

/* The title screen is an HDMA mode 3 split with windowed photo frames, so
 * extending its layers would fight the split. It does not need extending:
 * with every layer disabled the picture still renders white, which means the
 * white is the CGRAM backdrop rather than any layer. Widening the picture
 * while clamping every layer to the middle 256 pixels therefore fills the
 * margins with the screen own backdrop colour and leaves the split, the
 * windows and the sprites exactly where the cartridge put them. */
bool issd_widescreen_title_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  return word(ram, 0x32) == 1;
}

/* The stride is a page allocation, not the authored map width. Stadium maps
 * end partway through their last page; the remaining zero columns are blank
 * metatiles (solid green on BG1). Find the occupied horizontal extent per
 * layer and repeat its outermost 8-pixel tile column when a corner exposes
 * that padding. Repeating the whole 32-pixel metatile repeats diagonal wall
 * transitions as a checkerboard instead of continuing the outer surface. */
static void world_bounds(const uint8_t *ram, unsigned layer,
                         int *first, int *last, int *top, int *bottom) {
  unsigned stride = word(ram, 0x1ffcc);
  *first = (int)(stride / 64) * 256;
  *last = -32;
  *top = 0x10000; *bottom = -32;
  if (!stride) return;
  for (unsigned i = 0; i < 0x1000; i++) {
    if (!ram[0x1d000 + layer * 0x1000 + i]) continue;
    unsigned column = (i % stride) / 64 * 8 + (i & 7);
    int x = (int)column * 32;
    int y = (int)(i / stride) * 256 + (int)((i & 63) >> 3) * 32;
    if (x < *first) *first = x;
    if (x > *last) *last = x;
    if (y < *top) *top = y;
    if (y > *bottom) *bottom = y;
  }
  if (*last < *first) { *first = 0; *last = (int)(stride / 64) * 256 - 32; }
  if (*bottom < *top) { *top = 0; *bottom = (int)(0x1000 / stride) * 256 - 32; }
}

static bool world_tile(const uint8_t *ram, unsigned layer,
                       int x, int y, int first, int last, int top, int bottom,
                       uint16_t *tile) {
  unsigned stride = word(ram, 0x1ffcc);
  /* $8B87E7: each 256x256 world page is an 8x8 byte block.
   * Every world row contains stride/64 pages.
   * Clamp view coordinates into valid world stadium map bounds so
   * viewports extending past sidelines or penalty areas sample the valid
   * stadium boundary metatiles (outer grandstands, hoardings, walls)
   * instead of failing and creating black void margins. */
  if (stride < 64 || (stride & 63) != 0) return false;
  if (x < first) x = first + (x & 7);
  else if (x >= last + 32) x = last + 24 + (x & 7);
  /* The page budget also includes empty rows beyond the authored stadium.
   * Near the goal area, continue the outer tile row rather than blank green
   * metatiles. As with x, this only samples added presentation columns. */
  if (y < top) y = top + (y & 7);
  else if (y >= bottom + 32) y = bottom + 24 + (y & 7);

  if (y < 0) y = 0;
  unsigned page_y = (unsigned)y >> 8;
  unsigned index = page_y * stride +
                   ((y & 0xe0) >> 2) + ((unsigned)x >> 8) * 64 +
                   ((x & 255) >> 5);
  if (index >= 0x1000) {
    /* Clamp y within the 4096-byte metatile page budget */
    unsigned max_pages_y = 0x1000 / stride;
    if (max_pages_y > 0) {
      y = (int)(max_pages_y * 256) - 1;
      index = ((unsigned)y >> 8) * stride +
              ((y & 0xe0) >> 2) + ((unsigned)x >> 8) * 64 +
              ((x & 255) >> 5);
    }
    if (index >= 0x1000) index = 0x0FFF;
  }
  unsigned metatile = ram[0x1d000 + layer * 0x1000 + index];
  unsigned definition = 0x18000 + layer * 0x2000 + metatile * 32;
  *tile = word(ram, definition + ((y & 31) >> 3) * 8 + ((x & 31) >> 3) * 2);
  return true;
}

bool issd_widescreen_world_layer(const uint8_t *ram,unsigned layer,IssdWorldLayer *out) {
  if(!ram || !out || layer>1) return false;
  unsigned stride=word(ram,0x1ffcc);
  if(stride<0x80 || stride>0x340 || (stride&63)) return false;
  out->layer=layer;
  world_bounds(ram,layer,&out->first,&out->last,&out->top,&out->bottom);
  return true;
}
bool issd_widescreen_world_sample(const uint8_t *ram,const IssdWorldLayer *layer,
                                 int x,int y,uint16_t *tile) {
  if(!ram || !layer || !tile || layer->layer>1) return false;
  return world_tile(ram,layer->layer,x,y,layer->first,layer->last,layer->top,layer->bottom,tile);
}

static void fill_pitch(Ppu *ppu, const uint8_t *ram, int left, int right) {
  for (unsigned layer = 0; layer < 2; layer++) {
    int first, last, top, bottom;
    world_bounds(ram, layer, &first, &last, &top, &bottom);
    /* PPU scroll registers retain ten bits. Recover the current world page
     * from WRAM, retaining the actual scanout offset (vertical is minus one). */
    int sx = (word(ram,0x13a0+layer*32)&~1023) | ppu->hScroll[layer];
    int sy = (word(ram,0x13b0+layer*32)&~1023) | ppu->vScroll[layer];
    int first_x = (sx - left) & ~7, last_x = (sx + 255 + right) & ~7;
    for (int y = sy & ~7; y <= ((sy + 223) & ~7); y += 8) {
      for (int x = first_x; x <= last_x; x += 8) {
        /* Shared partial edge tiles already belong to the original view. */
        if (x + 7 >= sx && x < sx + 256) continue;
        unsigned tx = ((unsigned)x >> 3) & 63, ty = ((unsigned)y >> 3) & 63;
        unsigned address = layer * 0x1000 + (ty & 31) * 32 +
                           (tx & 31) + (tx >> 5) * 0x400 + (ty >> 5) * 0x800;
        uint16_t tile;
        if (!world_tile(ram, layer, x, y, first, last, top, bottom, &tile)) {
          ppu->vram[address] = 0;
          continue;
        }
        ppu->vram[address] = tile;
      }
    }
  }
}

static const uint8_t *rom_span(const uint8_t *rom, size_t size,
                               unsigned address, unsigned count) {
  unsigned offset = ((address >> 16) & 0x7f) * 0x8000 + (address & 0x7fff);
  if (!rom || (address & 0xffff) < 0x8000 ||
      count > 0x10000 - (address & 0xffff) ||
      offset > size || count > size - offset) return NULL;
  return rom + offset;
}

/* $84E6C7 updates object+$14 even when $1E suppresses the pose and graphics
 * DMA. The descriptor in bank $82 pairs the actual pose with two
 * length-prefixed graphics rows. Replaying only OAM (or a remembered pose)
 * interprets old menu tiles or a different animation's tiles as a player.
 * Reproduce those uploads in the presentation transaction, including the
 * uniform/number tile selected by $84E77D. Each player owns its tile slots. */
static uint16_t prepare_player(Ppu *ppu, const uint8_t *ram, unsigned object,
                               uint16_t descriptor,
                               const uint8_t *rom, size_t rom_size) {
  const uint8_t *desc = rom_span(rom, rom_size,
      0x820000 | descriptor, 6);
  if (!desc || !ram[object + 0x30]) return 0;
  unsigned pose = word(desc, 0);
  if (!pose) return 0;
  if (pose & 0x8000) {
    const uint8_t *geometry = rom_span(rom, rom_size, 0x880000 | pose, 1);
    if (!geometry || !geometry[0] || geometry[0] > 64 ||
        !rom_span(rom, rom_size, 0x880000 | pose, 1 + geometry[0] * 4)) return 0;
  } else if (!ram[pose] || ram[pose] > 64 || pose + ram[pose] * 2 > 0xa000) {
    return 0;
  }
  unsigned source = desc[2] | desc[3] << 8 | desc[4] << 16;
  const uint8_t *rows[2];
  unsigned lengths[2];
  for (unsigned row = 0; row < 2; row++) {
    const uint8_t *header = rom_span(rom, rom_size, source, 2);
    if (!header) return 0;
    lengths[row] = word(header, 0);
    if (!lengths[row] || lengths[row] > 0x100 || (lengths[row] & 1)) return 0;
    rows[row] = rom_span(rom, rom_size, source + 2, lengths[row]);
    if (!rows[row]) return 0;
    source += 2 + lengths[row];
  }
  unsigned dest = 0x6000 + (word(ram, object + 2) & 0x1ff) * 16;
  if (dest + 0x180 > 0x8000) return 0;
  for (unsigned row = 0; row < 2; row++)
    for (unsigned j = 0; j < lengths[row]; j += 2)
      ppu->vram[dest + row * 0x100 + j / 2] = word(rows[row], j);

  unsigned type = ram[object + 0x30];
  if (type == 8) {
    const uint8_t *entry = rom_span(rom, rom_size, 0x81ceec + desc[5], 2);
    const uint8_t *pixels = entry ?
        rom_span(rom, rom_size, 0xa40000 | word(entry, 0), 64) : NULL;
    if (pixels)
      for (unsigned j = 0; j < 64; j += 2)
        ppu->vram[0x7fe0 + j / 2] = word(pixels, j);
  }
  if (type > 1 && type != 9) {
    unsigned tile = 0xf014; /* blank uniform detail */
    unsigned detail = desc[5];
    if (type != 7 && type != 8 && ram[object + 0x57] != 0x54) {
      if (detail & 0x80) {
        const uint8_t *entry = rom_span(rom, rom_size,
            0x81ce8a + ram[object + 0x57], 2);
        if (entry) {
          tile = word(entry, 0);
          if (tile != 0xf014) tile += ((detail & 0x7f) - 1) * 32;
        }
      } else if (detail && detail <= 12 && !(detail & 1)) {
        if ((detail == 4 || detail == 6) && (ram[object + 4] & 0x40))
          detail ^= 2;
        const uint8_t *entry = rom_span(rom, rom_size, 0x81cede + detail, 2);
        if (entry) {
          tile = word(entry, 0);
          if (detail < 10) tile += ram[object + 0x56] * 16;
        }
      }
    }
    const uint8_t *pixels = rom_span(rom, rom_size, 0x980000 | tile, 32);
    if (pixels)
      for (unsigned j = 0; j < 32; j += 2)
        ppu->vram[dest + 0x170 + j / 2] = word(pixels, j);
  }
  return word(desc, 0);
}

typedef struct {
  unsigned object;
  bool missing_native_copy;
} ObjectEntry;

enum { REPLAY_SLOT_SIZE=448, REPLAY_ACTOR_SIZE=20 };
static uint8_t replay_history[512u*REPLAY_SLOT_SIZE];
static uint16_t replay_directory=0xffff,previous_replay_directory=0xffff;
static void replay_put(uint8_t *p,unsigned a,unsigned value) {
  p[a]=(uint8_t)value;p[a+1]=(uint8_t)(value>>8);
}
static uint32_t replay_hash(const uint8_t *ram,unsigned start,unsigned end) {
  uint32_t hash=2166136261u;
  for(unsigned i=start;i<end;i++) hash=(hash^ram[0x10000+i])*16777619u;
  return hash;
}
static bool replay_matches(const uint8_t *slot,const uint8_t *ram,unsigned directory) {
  unsigned start=word(slot,0),end=word(slot,2);
  if(directory>=0x400 || (directory&1) || start<0x400 || end<=start || end>0x7700 ||
      word(ram,0x10000+directory)!=start) return false;
  uint32_t hash=word(slot,4)|((uint32_t)word(slot,6)<<16);
  return hash==replay_hash(ram,start,end);
}
void issd_replay_reset(void) {
  memset(replay_history,0,sizeof replay_history);
  replay_directory=previous_replay_directory=0xffff;
}
void issd_replay_save_state(uint8_t *state) {
  if(state) {
    replay_put(state,0,replay_directory);replay_put(state,2,0);
    memcpy(state+4,replay_history,sizeof replay_history);
  }
}
bool issd_replay_validate_state(const uint8_t *state,size_t size) {
  if(!state || size!=ISSD_REPLAY_HISTORY_STATE_SIZE || word(state,2)) return false;
  unsigned directory=word(state,0);
  if(directory!=0xffff && (directory>=0x400 || (directory&1))) return false;
  for(unsigned i=0;i<512;i++) {
    const uint8_t *slot=state+4+i*REPLAY_SLOT_SIZE;
    unsigned start=word(slot,0),end=word(slot,2);
    if(!start) {
      for(unsigned j=0;j<REPLAY_SLOT_SIZE;j++) if(slot[j]) return false;
      continue;
    }
    if(start<0x400 || end<=start || end>0x7700) return false;
    for(unsigned p=0;p<22;p++) if(word(slot,8+p*REPLAY_ACTOR_SIZE+18)>1) return false;
  }
  return true;
}
bool issd_replay_load_state(const uint8_t *state,size_t size) {
  if(!issd_replay_validate_state(state,size)) return false;
  memcpy(replay_history,state+4,sizeof replay_history);
  replay_directory=word(state,0);previous_replay_directory=replay_directory;return true;
}
void issd_replay_observe(const uint8_t *ram,uint32_t pc) {
  pc|=0x800000;
  if(pc==0x8ba997) {issd_replay_reset();return;}
  if(ram && pc==0x8bae80) {replay_directory=word(ram,0x18aa);return;}
  if(!ram || pc!=0x8baafd) return;
  unsigned next=word(ram,0x18a4);
  if(next>=0x400 || (next&1)) return;
  unsigned directory=(next+0x3fe)&0x3ff;
  uint8_t *slot=replay_history+(directory/2)*REPLAY_SLOT_SIZE;
  memset(slot,0,REPLAY_SLOT_SIZE);
  unsigned start=word(ram,0x10000+directory),end=word(ram,0x18a0);
  if(start<0x400 || end<=start || end>0x7700) return;
  replay_put(slot,0,start);replay_put(slot,2,end);
  uint32_t hash=replay_hash(ram,start,end);
  replay_put(slot,4,hash);replay_put(slot,6,hash>>16);
  for(unsigned i=0;i<22;i++) {
    unsigned object=0x500+i*0x100;
    uint8_t *actor=slot+8+i*REPLAY_ACTOR_SIZE;
    memcpy(actor,ram+object,6);
    replay_put(actor,6,word(ram,object+8));
    replay_put(actor,8,word(ram,object+12));
    replay_put(actor,10,word(ram,object+16));
    replay_put(actor,12,word(ram,object+20));
    replay_put(actor,14,word(ram,object+0x30));
    replay_put(actor,16,word(ram,object+0x56));
    unsigned type=word(ram,object+0x30);
    replay_put(actor,18,type && !(type&0x8000) && (word(ram,object+20)&0x8000));
  }
}
/* Bind each resolved live pose to the same native recording generation as
 * its OAM. Playback must never run the live animation predictor a second time. */
static void replay_presented_pose(const uint8_t *ram,unsigned object,uint16_t descriptor) {
  if(word(ram,0x70)==0x13 || object<0x500 || object>=0x1b00 || (object&255)) return;
  unsigned directory=(word(ram,0x18a4)+0x3fe)&0x3ff;
  uint8_t *slot=replay_history+(directory/2)*REPLAY_SLOT_SIZE;
  if(replay_matches(slot,ram,directory))
    replay_put(slot,8+((object-0x500)/0x100)*REPLAY_ACTOR_SIZE+12,descriptor);
}
static void replay_restore_edges(uint8_t *ram,unsigned directory) {
  if(word(ram,0x70)!=0x13) return;
  if(directory>=0x400 || (directory&1)) return;
  const uint8_t *slot=replay_history+(directory/2)*REPLAY_SLOT_SIZE;
  if(!replay_matches(slot,ram,directory)) return;
  for(unsigned i=0;i<22;i++) {
    unsigned object=0x500+i*0x100;
    const uint8_t *actor=slot+8+i*REPLAY_ACTOR_SIZE;
    if(!word(ram,object+0x1e) || !word(actor,18)) continue;
    memcpy(ram+object,actor,6);
    replay_put(ram,object+8,word(actor,6));replay_put(ram,object+12,word(actor,8));
    replay_put(ram,object+16,word(actor,10));replay_put(ram,object+18,word(actor,8));
    replay_put(ram,object+20,word(actor,12));replay_put(ram,object+0x30,word(actor,14));
    replay_put(ram,object+0x56,word(actor,16));replay_put(ram,object+0x1e,0);
  }
}

/* $8BAAC6 records only admitted players. During replay $98F279 marks all
 * players absent and $98F291 clears $1E only for recorded entries. An absent
 * record retains its old coordinates/descriptor, not a replayable pose. */
static bool replay_player_absent(const uint8_t *ram, unsigned object) {
  return word(ram, 0x70) == 0x13 && object >= 0x500 && object < 0x1b00 &&
         !(object & 255) && word(ram, object + 0x1e) != 0;
}

static bool camera_piece(const Ppu *ppu,const uint8_t *ram,const uint8_t *rom,
    size_t rom_size,unsigned object,unsigned pose,unsigned part,IssdCameraPiece *out) {
  static const uint8_t sizes[8][2]={{8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}};
  bool packed=(pose&0x8000)!=0,large;int dx,dy;unsigned tile,attr;
  if(packed) {
    const uint8_t *geometry=rom_span(rom,rom_size,0x880000|pose,1);
    if(!geometry || !geometry[0] || geometry[0]>64 || part>=geometry[0]) return false;
    geometry=rom_span(rom,rom_size,0x880000|pose,1+geometry[0]*4);
    if(!geometry) return false;
    const uint8_t *data=geometry+1+part*4;
    dy=(int8_t)data[0];dx=(int8_t)data[1];tile=data[2];attr=data[3];large=(attr&0x10)!=0;
  } else {
    if(!pose || pose>=0xa000 || !ram[pose] || ram[pose]>64 || part>=ram[pose]) return false;
    unsigned index=pose+part*2;
    if(index>=0xa000) return false;
    dx=(int16_t)word(ram,0x2000+index);dy=(int16_t)word(ram,0x4000+index);
    tile=ram[0x6000+index];attr=ram[0x6001+index];large=(ram[index+1]&0x80)!=0;
  }
  unsigned props=ram[object+3]|((((ram[object+5]&0x80 ? ram[object+5] : ram[0x7c])&0x30)|ram[object+4])&0xf0);
  unsigned color=(attr&0xc1)^props;
  if(attr&0x20) color=color&8 ? color|2 : (color&~4)|8;
  out->x=(int16_t)word(ram,object+8)-(large?8:4)+((props&0x40)?-dx:dx);
  out->y=(int16_t)word(ram,object+12)+(int16_t)word(ram,object+16)-(large?8:4)+dy;
  out->size=sizes[ppu->obsel>>5][large];out->tile=(uint8_t)(tile+ram[object+2]);
  out->attributes=(uint8_t)color;
  return true;
}

static uint16_t camera_aux_identity[0xd00];

bool issd_widescreen_camera_pieces(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,
    size_t rom_size,const IssdCameraView *view,IssdCameraPiece *pieces,size_t capacity,size_t *count) {
  if(!ppu || !ram || !rom || !view || !pieces || !count || !issd_widescreen_pitch_layout(ppu,ram)) return false;
  *count=0;
  /* Keep uploads private even at 4:3, where the ordinary wider transaction is absent. */
  if(!frame.owner) {
    frame.owner=ppu;memcpy(frame.vram,ppu->vram,sizeof frame.vram);
    memcpy(frame.oam,ppu->oam,sizeof frame.oam);memcpy(frame.high_oam,ppu->highOam,sizeof frame.high_oam);
  }
  if(ram==frame.ram) replay_restore_edges(frame.ram,frame.replay_directory);
  unsigned objects[128],n=0;
  for(unsigned i=0;i<48;i++) {
    unsigned object=word(ram,0x1d40+i*2);if(!object) break;
    if(object>=0x400 && object<=0x1c40) {
      objects[n++]=object;
      if(object<0xd00 && (object&255)) camera_aux_identity[object]=word(ram,object);
    }
  }
  for(unsigned object=0x400;object<0x1b00;object+=0x100) {
    if(object>=0x500 && (!ram[object+0x30] || !(word(ram,object+0x14)&0x8000))) continue;
    bool exists=false;for(unsigned i=0;i<n;i++) if(objects[i]==object) exists=true;
    if(!exists) objects[n++]=object;
  }
  /* Active auxiliary records have their coordinates maintained by $83D01E/$83D057.
   * Replay auxiliaries without native admission are excluded until recorded evidence exists. */
  if(word(ram,0x70)!=0x13) for(unsigned base=0x400;base<0xd00;base+=0x100)
    for(unsigned offset=0xa0;offset<=0xd0;offset+=0x30) {
      if(offset==0xd0 && base<0x800) continue;
      unsigned object=base+offset;
      if(!word(ram,object)) {camera_aux_identity[object]=0;continue;}
      bool exists=false;for(unsigned i=0;i<n;i++) if(objects[i]==object) exists=true;
      if(!exists) {
        int x=(int16_t)word(ram,object+8),y=(int16_t)word(ram,object+12);
        bool native_y=base<0x800 ? (unsigned)(y+32)<0x140u : (unsigned)y<0x140u;
        bool horizontal_cull=base<0x800 ? (unsigned)(x+32)>=0x140u : (unsigned)(x+64)>=0x180u;
        /* A stale center record is not evidence of a drawable auxiliary.
         * Vertical expansion requires an identity previously admitted by the cartridge. */
        if((native_y && horizontal_cull) ||
            (!native_y && camera_aux_identity[object]==word(ram,object))) objects[n++]=object;
      }
    }
  for(unsigned i=1;i<n;i++) {
    unsigned object=objects[i],j=i;
    while(j && (int16_t)word(ram,objects[j-1]+0x12)>(int16_t)word(ram,object+0x12)) {
      objects[j]=objects[j-1];j--;
    }
    objects[j]=object;
  }
  while(n) {
    unsigned object=objects[--n];
    if(replay_player_absent(ram,object)) continue;
    int x=(int16_t)word(ram,object+8),y=(int16_t)word(ram,object+12)+(int16_t)word(ram,object+16);
    if(x+64<=view->x || x-64>=view->x+view->w || y+64<=view->y || y-64>=view->y+view->h) continue;
    unsigned pose=word(ram,object);
    if(object>=0x500 && object<0x1b00 && !(object&255) && ram[object+0x30]) {
      uint16_t descriptor=frame.descriptors[(object-0x500)/256];
      if(!descriptor) {
        descriptor=word(ram,object+0x14);
        if(word(ram,0x70)!=0x13) {
          int ground_y=(int16_t)word(ram,object+12);
          issd_pose_history_observe_world(object,x,ground_y,
              x+word(ram,0x13a0),ground_y+word(ram,0x13b0),descriptor);
          if(!issd_animation_pose(object,ram,rom,rom_size,
              issd_pose_history_live_window(x,ground_y),&descriptor))
            descriptor=issd_pose_history_pose(object,x,ground_y,descriptor);
          replay_presented_pose(ram,object,descriptor);
        }
        frame.descriptors[(object-0x500)/256]=descriptor;
      }
      pose=prepare_player(ppu,ram,object,descriptor,rom,rom_size);
    }
    for(unsigned part=0;part<64;part++) {
      IssdCameraPiece piece;
      if(!camera_piece(ppu,ram,rom,rom_size,object,pose,part,&piece)) break;
      if(piece.x+piece.size<=view->x || piece.x>=view->x+view->w ||
          piece.y+piece.size<=view->y || piece.y>=view->y+view->h) continue;
      if(*count==capacity) return false;
      pieces[(*count)++]=piece;
    }
  }
  return true;
}

/* A slot may only be reused when the hardware cannot draw it on any visible
 * line. Sprite rows are fetched as row = (uint8_t)(line - y), so for y >= 224
 * the first candidate row is 256 - y and the sprite still appears along the top
 * edge whenever 256 - y < height. Treating every y >= 224 as parked therefore
 * overwrote live sprites straddling the top of the screen - most visibly the
 * ball on its way up, which vanished mid-flight and only returned once the
 * keeper caught it and its y dropped back into range. */
static inline bool is_oam_slot_free(const Ppu *ppu, unsigned slot) {
  static const uint8_t sizes[8][2] = {
    {8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}
  };
  unsigned y = ppu->oam[slot*2] >> 8;
  if (y < 224) return false;
  unsigned large = (ppu->highOam[slot/4] >> ((slot%4)*2 + 1)) & 1;
  return y + sizes[PPU_objSize(ppu)][large] <= 256;
}

/* Supplemental OAM comes from the same sorted draw list ($8095E0), not from
 * interpolated/history sprites. If the native list already contains the object,
 * include only pieces the original horizontal clipping rejected. If the native
 * list omitted the whole object, every piece intersecting the widened viewport
 * needs a supplemental copy because there is no native OAM to preserve. */
static void fill_objects(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                         size_t rom_size, int left_extra, int right_extra) {
  static const uint8_t sizes[8][2] = {
    {8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}
  };
  uint8_t left[16] = {0}, right[16] = {0};
  int free_slot = 0;
  /* Existing negative sprites are genuine edge pieces; parked entries
   * never receive a hint. Positive 9-bit sprites require an explicit hint. */
  unsigned left_threshold = 512u - (unsigned)(left_extra + 32);
  if (left_threshold > 512u - 64u) left_threshold = 512u - 64u;
  for (unsigned slot=0; slot<128; slot++) {
    if (is_oam_slot_free(ppu, slot)) continue;
    unsigned raw = (ppu->oam[slot*2] & 255) |
      ((ppu->highOam[slot/4] >> ((slot%4)*2)) & 1) * 256;
    if (raw >= left_threshold) left[slot/8] |= 1 << (slot%8);
  }
  ObjectEntry objects[128];
  unsigned count = 0;
  while (count < 48 && word(ram, 0x1d40 + count*2)) {
    objects[count].object = word(ram,0x1d40+count*2);
    objects[count].missing_native_copy = false;
    count++;
  }
  /* Include players whose animation descriptor advanced while their native
   * pose/graphics upload was culled. Zero-pose inactive records stay inactive. */
  for (unsigned object=0x400;object<0x1b00;object+=0x100) {
    unsigned pose = word(ram, object);
    /* Inactive records never reach the drawing loop, but their animation
     * evidence must still expire before this object slot is reused. */
    if (object >= 0x500 && (!ram[object + 0x30] ||
        !(word(ram, object + 0x14) & 0x8000) || replay_player_absent(ram, object))) {
      issd_pose_history_observe(object, 0, 0, 0);
      issd_animation_forget(object);
    }
    if (replay_player_absent(ram, object)) continue;
    if (!pose && !(object >= 0x500 && ram[object + 0x30] &&
                   (word(ram, object + 0x14) & 0x8000))) continue;
    bool exists=false;
    for (unsigned i=0;i<count;i++) {
      if (objects[i].object==object) { exists=true; break; }
    }
    if (!exists && count < 128) {
      objects[count].object=object;
      objects[count].missing_native_copy=true;
      count++;
    }
  }
  /* Auxiliary draw records: officials, and the field objects enumerated by
   * $809B28/$809B4A. A zero pose is the inactive marker.
   *
   * This is a scan rather than a list of known ids because hardcoding three
   * of them left five live records with no supplemental copy at all: over a
   * 6000 frame match, $09A0, $09D0, $0AA0, $0AD0 and $0BA0 were live and
   * inside the widescreen margin for 3290 frames with nothing drawing them,
   * which is what made objects wink in and out at the edges. One of the three
   * hardcoded ids, $08A0, was never live at all. */
  for (unsigned base=0x400; base<0xd00; base+=0x100) {
   for (unsigned offset=0xa0; offset<=0xd0; offset+=0x30) {
    if (offset==0xd0 && base<0x800) continue;
    unsigned object = base + offset;
    if (!word(ram, object)) continue;
    /* Widen horizontal visibility only. The original camera transform
     * ($83D01E/$83D057) intentionally culls these auxiliary classes vertically,
     * even if stale geometry would wrap into the top of the screen. */
    unsigned y = word(ram, object + 12);
    if ((base < 0x800 && (uint16_t)(y + 32) >= 0x140) ||
        (base >= 0x800 && base < 0xa00 && y >= 0x140)) continue;
    bool exists=false;
    for (unsigned i=0;i<count;i++) {
      if (objects[i].object==object) { exists=true; break; }
    }
    /* Missing center-origin auxiliary records may retain poses across menu
     * return and buffered admission changes. Reconstruct only a genuine
     * original horizontal cull, never expand the original vertical/admission
     * behavior inside its existing viewport. Existing list entries still
     * receive their horizontally clipped edge pieces below. */
    unsigned x = word(ram, object + 8);
    if ((base < 0x800 && (uint16_t)(x + 32) < 0x140) ||
        (base >= 0x800 && base < 0xa00 && (uint16_t)(x + 64) < 0x180)) continue;
    if (!exists && count < 128) {
      objects[count].object=object;
      objects[count].missing_native_copy=true;
      count++;
    }
   }
  }
  for (unsigned i=1;i<count;i++) {
    ObjectEntry entry=objects[i];
    unsigned object=entry.object,j=i;
    while (j && (int16_t)word(ram,objects[j-1].object+0x12) > (int16_t)word(ram,object+0x12)) {
      objects[j]=objects[j-1]; j--;
    }
    objects[j]=entry;
  }
  /* Nearer objects appear later in the game's list and have OAM priority. */
  while (count) {
    ObjectEntry entry = objects[--count];
    unsigned object = entry.object;
    if (object < 0x400 || object > 0x1c40) continue;
    if (replay_player_absent(ram, object)) continue;
    unsigned pose = word(ram, object);
    int ox = (int16_t)word(ram, object + 8);
    int oy = (int16_t)word(ram, object + 12);
    bool player = object >= 0x500 && object < 0x1b00 &&
        !(object & 255) && ram[object + 0x30] &&
        (word(ram, object + 0x14) & 0x8000);
    uint16_t descriptor = word(ram, object + 0x14);
    if (player && word(ram,0x70)!=0x13) {
      /* Learn complete descriptors, not geometry addresses: two animation
       * steps may share a pose while using different graphics rows. */
      issd_pose_history_observe_world(object, ox, oy,
          ox + word(ram, 0x13a0), oy + word(ram, 0x13b0), descriptor);
      if (!issd_animation_pose(object, ram, rom, rom_size,
          issd_pose_history_live_window(ox, oy), &descriptor))
        descriptor = issd_pose_history_pose(object, ox, oy, descriptor);
      replay_presented_pose(ram,object,descriptor);
    }
    if(player) frame.descriptors[(object-0x500)/256]=descriptor;
    if (entry.missing_native_copy && player) {
      /* Player geometry fits within 64 pixels of its origin. Avoid uploading
       * invisible players, especially the shared type-8 detail tile. */
      if (ox + 64 <= -left_extra || ox - 64 >= 256 + right_extra) continue;
      pose = prepare_player(ppu, ram, object, descriptor, rom, rom_size);
      if (!pose) continue;
    }
    /* Geometry and graphics follow the same observed cartridge descriptor. */
    if (!pose) continue;
    bool packed = (pose & 0x8000) != 0;
    for (unsigned part=0; part<64; part++) {
      IssdCameraPiece piece;
      if (!camera_piece(ppu,ram,rom,rom_size,object,pose,part,&piece)) break;
      int x=piece.x,y=piece.y,size=piece.size;
      bool large=size==sizes[ppu->obsel>>5][1];
      int native_left = !packed && large ? -32 : -16;
      bool native_part_visible = x >= native_left && x < 256;
      if ((!entry.missing_native_copy && native_part_visible) ||
          x + size <= -left_extra || x >= 256+right_extra ||
          y+size <= 0 || y >= 224) continue;
      /* These are hardware parked entries, never an arbitrary visible slot. */
      while (free_slot<128 && !is_oam_slot_free(ppu, (unsigned)free_slot)) free_slot++;
      if (free_slot == 128) goto done;
      unsigned color=piece.attributes;
      ppu->oam[free_slot*2]=(uint8_t)x | ((uint16_t)(uint8_t)y<<8);
      ppu->oam[free_slot*2+1]=piece.tile | ((uint16_t)color<<8);
      unsigned shift=(free_slot%4)*2;
      ppu->highOam[free_slot/4]=(ppu->highOam[free_slot/4]&~(3u<<shift)) |
        ((((unsigned)x>>8)&1) | (large?2:0))<<shift;
      (x<0?left:right)[free_slot/8] |= 1 << (free_slot%8);
      free_slot++;
    }
  }
done:
  PpuWsSetOamLeftHints(ppu,left);
  PpuWsSetOamRightHints(ppu,right);
}

/* The cartridge double buffers its sprite list: every NMI first DMAs the OAM
 * built by the previous logic pass, then rebuilds it for the next one. Measured
 * on a live match, PPU OAM and the latched PPU scroll registers at frame N are
 * exactly the object records and camera as they stood at frame N-1.
 *
 * The supplement must therefore reconstruct from that same generation. Reading
 * this frame's fresh records emitted geometry one frame ahead of the native
 * sprites it was completing, so an object crossing the native clip edge was
 * either dropped (the supplement deferred to a native copy that was never
 * built) or drawn twice at two positions, and a camera crossing a 1024 pixel
 * page boundary mixed this frame's high scroll bits with last frame's low bits
 * and slewed the whole reconstructed margin by a full page for one frame. */
static uint8_t s_prev_ram[0x20000];
static bool s_prev_ram_valid = false;
static bool s_presented_ram_valid = false;
static Ppu *s_snapshot_ppu;
static int s_ws_extra = 0;

void issd_widescreen_reset(void) {
  if (frame.owner) issd_widescreen_end(frame.owner);
  memset(camera_aux_identity,0,sizeof camera_aux_identity);
  s_prev_ram_valid = false;
  s_presented_ram_valid = false;
  s_snapshot_ppu = NULL;
  s_ws_extra = 0;
  issd_pose_history_reset();
  issd_animation_reset();
}

/* Rebase frame generation after a checked load without discarding the restored
 * presentation animation. The saved frame has already latched current WRAM. */
void issd_widescreen_rebase(Ppu *ppu, const uint8_t *ram) {
  memset(camera_aux_identity,0,sizeof camera_aux_identity);
  if (frame.owner) issd_widescreen_end(frame.owner);
  s_presented_ram_valid = false;
  s_snapshot_ppu = ppu;
  s_ws_extra = 0;
  s_prev_ram_valid = ram != NULL;
  if (ram) memcpy(s_prev_ram, ram, sizeof(s_prev_ram));
}

const uint8_t *issd_widescreen_presented_ram(const uint8_t *current) {
  return s_presented_ram_valid ? frame.ram : current;
}

static void remember_ram(const uint8_t *ram) {
  if (!ram) return;
  memcpy(s_prev_ram, ram, sizeof(s_prev_ram));
  previous_replay_directory=replay_directory;
  s_prev_ram_valid = true;
}

bool Issd_IsWidescreenActive(void) {
  return s_ws_extra > 0;
}

bool issd_widescreen_begin(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                          size_t rom_size, int extra) {
  if (frame.owner) issd_widescreen_end(frame.owner);
  if (!ppu || !ram) {
    issd_widescreen_reset();
    if (ppu) {
      PpuSetExtraSpace(ppu, 0);
      PpuWsSetOamLeftHints(ppu, NULL);
      PpuWsSetOamRightHints(ppu, NULL);
    }
    return false;
  }
  bool is_pitch = issd_widescreen_pitch_layout(ppu, ram);
  /* Never reconstruct a new scene using the preceding scene's objects or
   * metatile allocation. Mode/submode cuts also cover replay and set pieces;
   * an explicit reset covers loads which keep the same scene identifiers. */
  if (s_prev_ram_valid && (s_snapshot_ppu != ppu ||
      word(ram, 0x32) != word(s_prev_ram, 0x32) ||
      word(ram, 0x70) != word(s_prev_ram, 0x70) ||
      word(ram, 0x1ffcc) != word(s_prev_ram, 0x1ffcc) ||
      is_pitch != issd_widescreen_pitch_layout(ppu, s_prev_ram)))
    issd_widescreen_reset();
  s_snapshot_ppu = ppu;
  memcpy(frame.ram, s_prev_ram_valid ? s_prev_ram : ram, sizeof(frame.ram));
  memset(frame.descriptors,0,sizeof frame.descriptors);
  frame.replay_directory=s_prev_ram_valid ? previous_replay_directory : replay_directory;
  s_presented_ram_valid = true;
  if (extra < 0) extra=0;
  if (extra > ISSD_WIDESCREEN_MAX_EXTRA) extra=ISSD_WIDESCREEN_MAX_EXTRA;
  PpuWsSetOamLeftHints(ppu,NULL); PpuWsSetOamRightHints(ppu,NULL);
  PpuSetWidescreenLayerClamp(ppu,0);
  PpuSetWidescreenLayerRepeat(ppu,0);
  for (int l=0;l<4;l++) PpuSetWidescreenLayerClampBand(ppu,l,0,0);
  /* Classic 4:3 takes none of the presentation branches: no VRAM/OAM
   * transaction, no supplemental sprites, no reconstructed margins. */
  if (!extra) {
    s_ws_extra = 0;
    PpuSetExtraSpace(ppu,0);
    remember_ram(ram);
    return false;
  }
  if (!is_pitch) {
    s_ws_extra = 0;
    unsigned submode = word(ram, 0x70);
    bool is_pillarboxed = (submode == 0x0F || submode == 0x10 || submode == 0x12 || submode == 0x1C);
    if (issd_widescreen_coin_layout(ppu, ram)) {
      PpuSetExtraSpace(ppu, (uint16_t)extra);
      PpuSetExtraSideSpace(ppu, extra, extra, 0);
      frame.owner=ppu;
      memcpy(frame.vram,ppu->vram,sizeof(frame.vram));
      memcpy(frame.oam,ppu->oam,sizeof(frame.oam));
      memcpy(frame.high_oam,ppu->highOam,sizeof(frame.high_oam));
      fill_coin_crowd(ppu);
      PpuSetWidescreenLayerMask(ppu, 1);
      PpuSetWidescreenLayerClamp(ppu, 0x1e);
    } else if (issd_widescreen_stats_layout(ppu, ram)) {
      PpuSetExtraSpace(ppu, (uint16_t)extra);
      PpuSetExtraSideSpace(ppu, extra, extra, 0);
      frame.owner = ppu;
      memcpy(frame.vram, ppu->vram, sizeof(frame.vram));
      memcpy(frame.oam, ppu->oam, sizeof(frame.oam));
      memcpy(frame.high_oam, ppu->highOam, sizeof(frame.high_oam));
      fill_stats_edges(ppu);
      PpuSetWidescreenLayerMask(ppu, 3);
      PpuSetWidescreenLayerClamp(ppu, 0x1c);
    } else if (is_pillarboxed && !issd_widescreen_menu_layout(ppu, ram)) {
      PpuSetExtraSpaceCentered(ppu, (uint16_t)extra);
    } else {
      PpuSetExtraSpace(ppu, (uint16_t)extra);
      PpuSetExtraSideSpace(ppu, extra, extra, 0);
      if (penalty_layout(ppu, ram)) {
        /* BG2's native tilemap repeats the stands/grass seamlessly. Leave
         * its per-scanline scroll and colour math intact, and never replay
         * the goal, HUD or native OAM into the new side columns. No game
         * memory transaction or wider simulation activation is needed. */
        PpuSetWidescreenLayerMask(ppu, 2);
        PpuSetWidescreenLayerClamp(ppu, 0x0d);
      } else if (issd_widescreen_menu_layout(ppu, ram)) {
        PpuSetWidescreenLayerRepeat(ppu, 1u << 1);                     /* BG2 */
        PpuSetWidescreenLayerClamp(ppu, 0x1d); /* panels, text and explicit OBJ clip */
      } else {
        PpuSetWidescreenLayerClamp(ppu, 0x0F);   /* all layers clamped, backdrop fills margins */
      }
    }
    remember_ram(ram);
    return false;
  }
  s_ws_extra = extra;
  frame.owner=ppu;
  memcpy(frame.vram,ppu->vram,sizeof(frame.vram));
  memcpy(frame.oam,ppu->oam,sizeof(frame.oam));
  memcpy(frame.high_oam,ppu->highOam,sizeof(frame.high_oam));
  PpuSetExtraSpace(ppu,(uint16_t)extra);
  PpuSetExtraSideSpace(ppu, extra, extra, 0);
  /* The pitch lives on BG1/BG2. BG3 carries score, clock, radar and player
   * labels; leaving it eligible for side-margin rendering repeats the 4:3 HUD
   * into the widened field. Keep only BG1/BG2 in the expanded columns and
   * widen pitch-layer windows so box/goal-area line masks follow the new view. */
  PpuSetWidescreenLayerMask(ppu, 3);
  PpuSetWidescreenWindowExpansion(ppu, 3, 3);
  PpuSetWidescreenLayerClamp(ppu, 4);
  /* Reconstruct against the WRAM generation that produced the native OAM and
   * the currently latched scroll registers, not this frame's fresh one. */
  const uint8_t *rec = frame.ram;
  fill_pitch(ppu, rec, extra, extra);
  replay_restore_edges(frame.ram,s_prev_ram_valid ? previous_replay_directory : replay_directory);
  fill_objects(ppu, rec, rom, rom_size, extra, extra);
  remember_ram(ram);
  return true;
}

void issd_widescreen_end(Ppu *ppu) {
  if (frame.owner != ppu || !ppu) return;
  memcpy(ppu->vram,frame.vram,sizeof(frame.vram));
  memcpy(ppu->oam,frame.oam,sizeof(frame.oam));
  memcpy(ppu->highOam,frame.high_oam,sizeof(frame.high_oam));
  frame.owner=NULL;
}

