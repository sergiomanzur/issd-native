#include "issd_camera_render.h"
#include "issd_widescreen.h"
#include "snes/ppu.h"
#include <string.h>

enum { CAMERA_MAX_WIDTH=504,CAMERA_MAX_HEIGHT=224,CAMERA_MAX_PIECES=4096 };

typedef struct { uint8_t palette,layer,priority; } CameraPixel;
static CameraPixel camera_main[CAMERA_MAX_WIDTH*CAMERA_MAX_HEIGHT];
static CameraPixel camera_obj[CAMERA_MAX_WIDTH*CAMERA_MAX_HEIGHT];
static CameraPixel camera_sub[CAMERA_MAX_WIDTH*CAMERA_MAX_HEIGHT];
static IssdCameraPiece camera_pieces[CAMERA_MAX_PIECES];
static size_t camera_piece_count;
static int16_t camera_native_slots[CAMERA_MAX_PIECES];
static IssdCameraPiece camera_extra_pieces[128];
static uint8_t camera_extra_slots[128];
static size_t camera_extra_count;
static IssdCameraView camera_view;
static int camera_output_width,camera_output_height;
static bool camera_prepared;
static IssdCameraArtLookup camera_art_lookup;
static uint32_t camera_art[CAMERA_MAX_WIDTH*CAMERA_MAX_HEIGHT];
typedef struct { unsigned key; const uint32_t *pixels; int size; bool valid; } CameraArtTile;
static CameraArtTile art_tiles[8192];
typedef struct {int x,y;uint16_t tile;bool valid;} CameraWorldTile;
static CameraWorldTile world_tiles[2];
void issd_camera_set_art_lookup(IssdCameraArtLookup lookup) {camera_art_lookup=lookup;}
static IssdCameraPiece camera_hud_pieces[128];
static size_t camera_hud_count;
bool issd_camera_active(void) {return camera_prepared;}
static unsigned word(const uint8_t *p,unsigned a) {return p[a]|(unsigned)p[a+1]<<8;}
static unsigned tile_pixel(const Ppu *p,unsigned base,unsigned tile,int x,int y) {
  unsigned bit=7-(x&7),address=(base+(tile&1023)*16+(y&7))&0x7fff;
  unsigned a=p->vram[address],b=p->vram[(address+8)&0x7fff];
  return ((a>>bit)&1)|(((a>>(bit+8))&1)<<1)|(((b>>bit)&1)<<2)|(((b>>(bit+8))&1)<<3);
}
static CameraPixel backdrop(void) {CameraPixel p={0,5,0};return p;}
static bool camera_window(const Ppu *p,unsigned layer,int x);
static CameraPixel world_pixel(const Ppu *p,const uint8_t *ram,
    const IssdWorldLayer layers[2],int x,int y,unsigned screen,uint32_t *art) {
  CameraPixel pixel=backdrop();
  for(unsigned l=0;l<2;l++) {
    if(!(p->screenEnabled[screen]&(1u<<l)) ||
        ((p->screenWindowed[screen]&(1u<<l)) && camera_window(p,l,x))) continue;
    int wx=((word(ram,0x13a0+l*32)&~1023u)|p->hScroll[l])+x;
    int wy=((word(ram,0x13b0+l*32)&~1023u)|p->vScroll[l])+y;
    CameraWorldTile *cached=world_tiles+l;
    int tx=wx>>3,ty=wy>>3;
    if(!cached->valid || cached->x!=tx || cached->y!=ty) {
      if(!issd_widescreen_world_sample(ram,layers+l,wx,wy,&cached->tile)) continue;
      cached->x=tx;cached->y=ty;cached->valid=true;
    }
    uint16_t tile=cached->tile;
    unsigned color=tile_pixel(p,PPU_bgTileAdr(p,l),tile,
        tile&0x4000 ? 7-(wx&7) : wx,tile&0x8000 ? 7-(wy&7) : wy);
    if(!color) continue;
    unsigned priority=(tile&0x2000 ? 12u : 8u)-l;
    if(priority>pixel.priority) {
      pixel.palette=((tile>>10)&7)*16+color;pixel.layer=l;pixel.priority=priority;
      if(art) {
        *art=0;
        if(camera_art_lookup) {
          unsigned base=PPU_bgTileAdr(p,l),pal=((tile>>10)&7)*16;
          unsigned key=base|((tile&1023)<<16)|((pal/16)<<26);
          CameraArtTile *entry=art_tiles+((base/16+(tile&1023)+pal*23)&8191);
          if(!entry->valid || entry->key!=key) {
            entry->key=key;entry->valid=true;
            entry->pixels=camera_art_lookup(p,base,tile&1023,pal,&entry->size);
          }
          if(entry->pixels && entry->size>=8) {
            int px=tile&0x4000 ? 7-(wx&7) : wx&7,py=tile&0x8000 ? 7-(wy&7) : wy&7;
            *art=entry->pixels[(size_t)(py*entry->size/8)*entry->size+px*entry->size/8];
          }
        }
      }
    }
  }
  return pixel;
}
static bool camera_window(const Ppu *p,unsigned layer,int x) {
  unsigned flags=(p->windowsel>>(4*layer))&15;bool a=false,b=false;
  if(flags&2) {
    a=p->window1left<=p->window1right &&
      (p->window1left==0 || x>=p->window1left) && (p->window1right==255 || x<=p->window1right);
    if(flags&1) a=!a;
  }
  if(flags&8) {
    b=p->window2left<=p->window2right &&
      (p->window2left==0 || x>=p->window2left) && (p->window2right==255 || x<=p->window2right);
    if(flags&4) b=!b;
  }
  if(!(flags&2)) return b;
  if(!(flags&8)) return a;
  switch((p->wbgobjlog>>(layer*2))&3) {
    case 1:return a&&b;case 2:return a!=b;case 3:return a==b;
    default:return a||b;
  }
}
static uint32_t compose_color(const Ppu *p,CameraPixel main,CameraPixel sub,int x) {
  unsigned color=p->cgram[main.palette];
  bool inside=camera_window(p,5,x);
  unsigned clip=PPU_clipMode(p),prevent=PPU_preventMathMode(p);
  bool visible=clip==0 || (clip==1 && inside) || (clip==2 && !inside);
  bool math=prevent==0 || (prevent==1 && inside) || (prevent==2 && !inside);
  if(!visible) color=0;
  int rgb[3]={(int)(color&31),(int)((color>>5)&31),(int)((color>>10)&31)};
  if(math && (PPU_mathEnabled(p)&(1u<<main.layer))) {
    unsigned c2=PPU_addSubscreen(p) && sub.palette ? p->cgram[sub.palette] : p->fixedColor;
    bool half=PPU_halfColor(p) && (!PPU_addSubscreen(p) || sub.palette);
    for(unsigned i=0;i<3;i++) {
      int value=(c2>>(i*5))&31;
      rgb[i]+=PPU_subtractColor(p) ? -value : value;
      if(half) rgb[i]/=2;
      if(rgb[i]<0) rgb[i]=0;
      if(rgb[i]>31) rgb[i]=31;
    }
  }
  uint32_t result=0;
  for(unsigned i=0;i<3;i++) {
    unsigned c=((rgb[i]<<3)|(rgb[i]>>2))*PPU_brightness(p)/15;
    result|=c<<((2-i)*8);
  }
  return p->inidisp&0x80 ? 0 : result;
}
static bool camera_render_layers(const Ppu *p,const uint8_t *ram,
    const IssdCameraView *view,uint32_t *pixels,size_t stride) {
  if(!p || !ram || !view || view->w<=0 || view->h<=0 ||
      view->w>2048 || view->h>1024 || stride<(size_t)view->w || PPU_mode(p)!=1) return false;
  IssdWorldLayer layers[2];
  if(!issd_widescreen_world_layer(ram,0,layers) ||
      !issd_widescreen_world_layer(ram,1,layers+1)) return false;
  memset(art_tiles,0,sizeof art_tiles);
  memset(world_tiles,0,sizeof world_tiles);
  for(int y=0;y<view->h;y++) for(int x=0;x<view->w;x++)
  {
    uint32_t art=0;
    CameraPixel main=world_pixel(p,ram,layers,x+view->x,y+view->y,0,&art);
    CameraPixel sub=world_pixel(p,ram,layers,x+view->x,y+view->y,1,NULL);
    if(pixels) pixels[(size_t)y*stride+x]=compose_color(p,main,sub,x+view->x);
  }
  return true;
}
bool issd_camera_render_world(const Ppu *p,const uint8_t *ram,
    const IssdCameraView *view,uint32_t *pixels,size_t stride) {
  return pixels && camera_render_layers(p,ram,view,pixels,stride);
}

bool issd_camera_prepare(Ppu *p,const uint8_t *ram,const uint8_t *rom,size_t size,
    IssdCameraMode mode,int width,int height) {
  camera_prepared=false;
  if(mode!=ISSD_CAMERA_TACTICAL && mode!=ISSD_CAMERA_TACTICAL_WIDE) return false;
  if(width<=0 || width>504 || height!=224 || !issd_widescreen_pitch_layout(p,ram)) return false;
  camera_view=issd_camera_view(width,height,mode);
  camera_output_width=width;camera_output_height=height;
  if(!issd_widescreen_camera_pieces(p,ram,rom,size,&camera_view,camera_pieces,
      CAMERA_MAX_PIECES,&camera_piece_count)) return false;
  camera_prepared=true;return true;
}

static void draw_camera_piece(const Ppu *p,const IssdCameraPiece *piece) {
  int left=issd_camera_project(piece->x,camera_view.x,camera_output_width,camera_view.w);
  int top=issd_camera_project(piece->y,camera_view.y,camera_output_height,camera_view.h);
  int right=issd_camera_project(piece->x+piece->size,camera_view.x,camera_output_width,camera_view.w)+1;
  int bottom=issd_camera_project(piece->y+piece->size,camera_view.y,camera_output_height,camera_view.h)+1;
  if(left<0) left=0;if(top<0) top=0;
  if(right>camera_output_width) right=camera_output_width;
  if(bottom>camera_output_height) bottom=camera_output_height;
  unsigned attr=piece->attributes;
  unsigned base=(attr&1) ? PPU_objTileAdr2(p) : PPU_objTileAdr1(p);
  unsigned priority=4*((attr>>4)&3)+2;
  for(int dy=top;dy<bottom;dy++) for(int dx=left;dx<right;dx++) {
    int x=dx*camera_view.w/camera_output_width+camera_view.x-piece->x;
    int y=dy*camera_view.h/camera_output_height+camera_view.y-piece->y;
    if(x<0 || y<0 || x>=piece->size || y>=piece->size) continue;
    int px=attr&0x40 ? piece->size-1-x : x;
    int py=attr&0x80 ? piece->size-1-y : y;
    unsigned tile=(piece->tile&0xf0)|((piece->tile+(px>>3))&15);
    tile=(tile+16*(py>>3))&255;
    unsigned color=tile_pixel(p,base,tile,px,py);
    if(!color) continue;
    CameraPixel pixel={128+((attr>>1)&7)*16+color,
        (attr&8) ? 4u : 6u,priority};
    size_t i=(size_t)dy*camera_output_width+dx;
    /* Resolve OBJ overlap first, independently of its background priority. */
    if(!camera_obj[i].palette) camera_obj[i]=pixel;
  }
}
static void native_extras(const Ppu *p) {
  static const uint8_t sizes[8][2]={{8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}};
  camera_hud_count=0;camera_extra_count=0;
  for(unsigned slot=0;slot<128;slot++) {
    unsigned hi=(p->highOam[slot/4]>>((slot%4)*2))&3;
    int x=(p->oam[slot*2]&255)|((hi&1)<<8);if(x>=256) x-=512;
    int y=p->oam[slot*2]>>8,size=sizes[p->obsel>>5][(hi>>1)&1];
    if(y>=224 || x+size<=0 || x>=256) continue;
    uint16_t attr=p->oam[slot*2+1];bool field=false;
    for(size_t i=0;i<camera_piece_count;i++) {
      const IssdCameraPiece *piece=camera_pieces+i;
      if(((piece->x-x)&511)==0 && ((piece->y-y)&255)==0 && piece->size==size &&
          (piece->tile|((uint16_t)piece->attributes<<8))==attr) {
        camera_native_slots[i]=(int16_t)slot;field=true;break;
      }
    }
    if(field) continue;
    IssdCameraPiece piece={x,y,size,attr&255,(uint8_t)(attr>>8)};
    /* The cartridge owns sprite HUD flags and radar dots. Remaining field
     * indicators retain their original geometry but follow the tactical transform. */
    if(y<32 || (x>=88 && x<168 && y>=160 && y<216)) camera_hud_pieces[camera_hud_count++]=piece;
    else {
      camera_extra_slots[camera_extra_count]=(uint8_t)slot;
      camera_extra_pieces[camera_extra_count++]=piece;
    }
  }
}
static void draw_hud_pieces(const Ppu *p,uint32_t *output,int width,int height) {
  if(!(p->screenEnabled[0]&16)) return;
  for(size_t i=camera_hud_count;i>0;i--) {
    const IssdCameraPiece *piece=camera_hud_pieces+i-1;unsigned attr=piece->attributes;
    unsigned base=(attr&1) ? PPU_objTileAdr2(p) : PPU_objTileAdr1(p);
    for(int y=0;y<piece->size;y++) for(int x=0;x<piece->size;x++) {
      int dx=piece->x+x+(width-256)/2,dy=piece->y+y;
      if(dx<0 || dy<0 || dx>=width || dy>=height) continue;
      int px=attr&0x40 ? piece->size-1-x : x,py=attr&0x80 ? piece->size-1-y : y;
      unsigned tile=((piece->tile&0xf0)|((piece->tile+(px>>3))&15))+16*(py>>3);
      unsigned color=tile_pixel(p,base,tile&255,px,py);if(!color) continue;
      CameraPixel pixel={128+((attr>>1)&7)*16+color,(attr&8) ? 4u : 6u,14};
      output[(size_t)dy*width+dx]=compose_color(p,pixel,backdrop(),piece->x+x);
    }
  }
}
bool issd_camera_compose(const Ppu *p,const uint8_t *ram,uint32_t *output,
    const uint32_t *hud,int width,int height) {
  if(!camera_prepared || !output || !p || !ram || width<=0 || width>504 || height!=224) return false;
  if(width!=camera_output_width || height!=camera_output_height) return false;
  IssdWorldLayer layers[2];
  if(!issd_widescreen_world_layer(ram,0,layers) || !issd_widescreen_world_layer(ram,1,layers+1)) {
    camera_prepared=false;return false;
  }
  memset(art_tiles,0,sizeof art_tiles);
  memset(world_tiles,0,sizeof world_tiles);
  /* Only destination samples contribute to the final image. This avoids
   * shading discarded expanded-world pixels and keeps Android memory bounded. */
  for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
    int wx=x*camera_view.w/width+camera_view.x,wy=y*camera_view.h/height+camera_view.y;
    size_t i=(size_t)y*width+x;
    camera_main[i]=world_pixel(p,ram,layers,wx,wy,0,camera_art+i);
    camera_sub[i]=world_pixel(p,ram,layers,wx,wy,1,NULL);
  }
  memset(camera_obj,0,(size_t)width*height*sizeof *camera_obj);
  memset(camera_native_slots,0xff,camera_piece_count*sizeof *camera_native_slots);
  native_extras(p);
  /* Keep all actor pieces in shared depth order, including clipped pieces.
   * Native unmatched indicators enter immediately before their next admitted
   * actor anchor, preserving their relative OAM ordering. */
  size_t extra=0;
  for(size_t i=0;i<camera_piece_count;i++) {
    if(camera_native_slots[i]>=0)
      while(extra<camera_extra_count && camera_extra_slots[extra]<camera_native_slots[i])
        draw_camera_piece(p,camera_extra_pieces+extra++);
    draw_camera_piece(p,camera_pieces+i);
  }
  while(extra<camera_extra_count) draw_camera_piece(p,camera_extra_pieces+extra++);
  uint32_t colors[256];bool visible_x[504],math_x[504],obj_mask_x[504];
  for(unsigned i=0;i<256;i++) {
    /* Layer six never participates in SNES color math. */
    unsigned native=p->cgram[i];colors[i]=0;
    for(unsigned channel=0;channel<3;channel++) {
      unsigned c=(native>>(channel*5))&31;
      c=((c<<3)|(c>>2))*PPU_brightness(p)/15;
      colors[i]|=c<<((2-channel)*8);
    }
  }
  unsigned clip=PPU_clipMode(p),prevent=PPU_preventMathMode(p);
  for(int x=0;x<width;x++) {
    int wx=x*camera_view.w/width+camera_view.x;bool inside=camera_window(p,5,wx);
    visible_x[x]=clip==0 || (clip==1 && inside) || (clip==2 && !inside);
    math_x[x]=prevent==0 || (prevent==1 && inside) || (prevent==2 && !inside);
    obj_mask_x[x]=camera_window(p,4,wx);
  }
  for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
    int wx=x*camera_view.w/width;
    size_t i=(size_t)y*width+x,dest=i;
    CameraPixel obj=camera_obj[i];
    bool masked=obj_mask_x[x];
    if(obj.palette && (p->screenEnabled[0]&16) &&
        (!(p->screenWindowed[0]&16) || !masked) && obj.priority>camera_main[i].priority) camera_main[i]=obj;
    if(obj.palette && (p->screenEnabled[1]&16) &&
        (!(p->screenWindowed[1]&16) || !masked) && obj.priority>camera_sub[i].priority) camera_sub[i]=obj;
    bool visible=visible_x[x],math=math_x[x];
    if(p->inidisp&0x80) output[dest]=0;
    else if(!(math && (PPU_mathEnabled(p)&(1u<<camera_main[i].layer))))
      output[dest]=visible ? colors[camera_main[i].palette] : 0;
    else output[dest]=compose_color(p,camera_main[i],camera_sub[i],wx+camera_view.x);
    /* Retain native colors wherever the cartridge applies color effects. */
    if(camera_main[i].layer<2 && visible &&
        !(math && (PPU_mathEnabled(p)&(1u<<camera_main[i].layer))) &&
        (camera_art[i]>>24) && !(p->inidisp&0x80)) {
      uint32_t art=camera_art[i],rgb=0;
      for(int shift=0;shift<24;shift+=8) rgb|=(((art>>shift)&255)*PPU_brightness(p)/15)<<shift;
      output[dest]=rgb;
    }
    if(hud && (hud[dest]>>24)) output[dest]=hud[dest]&0xffffff;
  }
  draw_hud_pieces(p,output,width,height);
  return true;
}
