#include "issd_running.h"
#include "issd_animation.h"
#include "snes/ppu.h"
#include <stdbool.h>
#include <string.h>

enum { SIDE=80, CENTER=40, LEGS=30, WORDS=0x180, MAX_PARTS=16 };
typedef struct { int x,y; unsigned tile,size,flags; } Part;
typedef struct { Part parts[MAX_PARTS]; unsigned count; } Geometry;
static Ppu *owner;
static uint16_t original_vram[0x8000];
static unsigned word(const uint8_t *p, unsigned a) { return p[a]|(unsigned)p[a+1]<<8; }
static const uint8_t *span(const uint8_t *rom,size_t size,unsigned address,unsigned n) {
    unsigned a=address&65535;
    if (!rom || a<0x8000 || n>0x10000u-a) return NULL;
    size_t offset=((address>>16)&127)*0x8000u+(a&0x7fff);
    return offset<=size && n<=size-offset ? rom+offset : NULL;
}
static bool owned_tile(unsigned t) { return t<8 || (t>=16 && t<23); }
static unsigned pixel(const uint16_t *tiles,unsigned tile,unsigned x,unsigned y) {
    unsigned t=tile+(x>>3)+(y>>3)*16;
    if (!owned_tile(t)) return 0;
    const uint16_t *p=tiles+t*16+(y&7);
    unsigned bit=7-(x&7);
    return (p[0]>>bit&1)|((p[0]>>(bit+8)&1)<<1)|
           ((p[8]>>bit&1)<<2)|((p[8]>>(bit+8)&1)<<3);
}
static void put_pixel(uint16_t *tiles,unsigned tile,unsigned x,unsigned y,unsigned c) {
    unsigned t=tile+(x>>3)+(y>>3)*16;
    if (!owned_tile(t)) return;
    uint16_t *p=tiles+t*16+(y&7);
    unsigned mask=1u<<(7-(x&7));
    for (unsigned plane=0;plane<4;plane++) {
        unsigned shift=(plane&1)*8, index=(plane/2)*8;
        p[index]=(uint16_t)((p[index]&~(mask<<shift))|((c>>plane&1)*mask<<shift));
    }
}
static bool geometry(const uint8_t *ram,const uint8_t *rom,size_t size,
                     uint16_t descriptor,Geometry *g) {
    const uint8_t *desc=span(rom,size,0x820000u|descriptor,9);
    if (!desc) return false;
    unsigned pose=word(desc,0), count;
    const uint8_t *packed=NULL;
    if (pose&0x8000) {
        packed=span(rom,size,0x880000u|pose,1);
        if (!packed) return false;
        count=packed[0]; packed=span(rom,size,0x880000u|pose,1+count*4);
        if (!packed) return false;
    } else {
        if (!pose || pose>=0xa000) return false;
        count=ram[pose];
        if (pose+count*2>0xa000) return false;
    }
    if (!count || count>MAX_PARTS) return false;
    g->count=0;
    for (unsigned i=0;i<count;i++) {
        int dx,dy; unsigned tile,flags; bool large;
        if (packed) {
            const uint8_t *p=packed+1+i*4;
            dy=(int8_t)p[0]; dx=(int8_t)p[1]; tile=p[2]; flags=p[3];
            large=(flags&0x10)!=0;
        } else {
            unsigned a=pose+i*2;
            dx=(int16_t)word(ram,0x2000+a); dy=(int16_t)word(ram,0x4000+a);
            tile=ram[0x6000+a]; flags=ram[0x6001+a]; large=(ram[a+1]&0x80)!=0;
        }
        /* Number/jersey overlays and shared hair/shadow tiles stay authored.
         * The hardware owns flipping, palette, depth and all OAM positions. */
        if (!owned_tile(tile)) continue;
        if (flags&1) return false; /* This part would select the other OBJ bank. */
        unsigned pixels=large ? 16 : 8;
        Part p={CENTER+dx-(int)pixels/2,CENTER+dy-(int)pixels/2,tile,pixels,flags};
        if (p.x<0 || p.y<0 || p.x+(int)pixels>SIDE || p.y+(int)pixels>SIDE) return false;
        g->parts[g->count++]=p;
    }
    return g->count!=0;
}
static bool next_tiles(const uint8_t *rom,size_t size,uint16_t descriptor,uint16_t *tiles) {
    const uint8_t *d=span(rom,size,0x820000u|descriptor,9);
    if (!d) return false;
    unsigned address=d[2]|(unsigned)d[3]<<8|(unsigned)d[4]<<16;
    memset(tiles,0,WORDS*sizeof(*tiles));
    for (unsigned row=0;row<2;row++) {
        const uint8_t *header=span(rom,size,address,2);
        if (!header) return false;
        unsigned len=word(header,0);
        if (!len || len>0x100 || (len&31)) return false;
        const uint8_t *data=span(rom,size,address+2,len);
        if (!data) return false;
        for (unsigned j=0;j<len;j+=2) tiles[row*0x100+j/2]=(uint16_t)word(data,j);
        address+=2+len;
    }
    return true;
}
static void canvas(const Geometry *g,const uint16_t *tiles,uint8_t image[SIDE][SIDE]) {
    memset(image,0,SIDE*SIDE);
    /* Lower numbered pieces have OAM priority, exactly as the native actor. */
    for (unsigned i=g->count;i>0;i--) {
        const Part *p=&g->parts[i-1];
        for (unsigned y=0;y<p->size;y++) for (unsigned x=0;x<p->size;x++) {
            unsigned tx=p->flags&0x40 ? p->size-1-x : x;
            unsigned ty=p->flags&0x80 ? p->size-1-y : y;
            unsigned c=pixel(tiles,p->tile,tx,ty);
            if (c) image[p->y+y][p->x+x]=(uint8_t)c;
        }
    }
}
static void project(const uint8_t a[SIDE][SIDE],const uint8_t b[SIDE][SIDE],
                    const uint8_t coverage[SIDE][SIDE],uint8_t out[SIDE][SIDE],
                    bool reverse) {
    for (int y=LEGS;y<SIDE;y++) for (int x=0;x<SIDE;x++) {
        unsigned c=a[y][x];
        if (!c) continue;
        int bx=x,by=y,best=33;
        /* Match the same palette ink locally, then place it at the midpoint.
         * Integer, palette-indexed motion creates crisp new leg poses rather
         * than blending colours or changing the original action clock. */
        for (int dy=-4;dy<=4;dy++) for (int dx=-4;dx<=4;dx++) {
            int nx=x+dx,ny=y+dy,distance=dx*dx+dy*dy;
            if (nx>=0 && nx<SIDE && ny>=LEGS && ny<SIDE &&
                b[ny][nx]==c && distance<best) { bx=nx;by=ny;best=distance; }
        }
        if (reverse && best==33) continue;
        unsigned mx=(unsigned)(x+bx)/2,my=(unsigned)(y+by)/2;
        /* New pixels cannot escape the native sprite rectangles. Keep a
         * matched limb inside its authored footprint instead of cutting it. */
        if (!coverage[my][mx]) {
            mx=(unsigned)(reverse ? bx : x);
            my=(unsigned)(reverse ? by : y);
        }
        if (coverage[my][mx]) out[my][mx]=(uint8_t)c;
    }
}
static bool enhance(Ppu *ppu,const uint8_t *ram,unsigned object,
                     const uint8_t *rom,size_t size) {
    uint16_t from,to;
    if (!issd_animation_running_pair(object,ram,rom,size,&from,&to)) return false;
    unsigned base=0x6000+(word(ram,object+2)&0x1ff)*16;
    if (base+WORDS>0x8000) return false;
    Geometry current,next;
    uint16_t source[WORDS],target[WORDS];
    uint8_t a[SIDE][SIDE],b[SIDE][SIDE],mid[SIDE][SIDE],coverage[SIDE][SIDE]={0};
    if (!geometry(ram,rom,size,from,&current) || !geometry(ram,rom,size,to,&next) ||
        !next_tiles(rom,size,to,target)) return false;
    memcpy(source,ppu->vram+base,sizeof(source));
    canvas(&current,source,a); canvas(&next,target,b);
    memcpy(mid,a,sizeof(mid)); memset(mid+LEGS,0,(SIDE-LEGS)*SIDE);
    for (unsigned i=0;i<current.count;i++) {
        const Part *p=&current.parts[i];
        for (unsigned y=0;y<p->size;y++) for (unsigned x=0;x<p->size;x++) {
            unsigned tx=p->flags&0x40 ? p->size-1-x : x;
            unsigned ty=p->flags&0x80 ? p->size-1-y : y;
            if (owned_tile(p->tile+(tx>>3)+(ty>>3)*16)) coverage[p->y+y][p->x+x]=1;
        }
    }
    project(a,b,coverage,mid,false); project(b,a,coverage,mid,true);
    for (unsigned i=0;i<current.count;i++) {
        const Part *p=&current.parts[i];
        for (unsigned y=0;y<p->size;y++) for (unsigned x=0;x<p->size;x++) {
            if (p->y+(int)y<LEGS) continue;
            unsigned tx=p->flags&0x40 ? p->size-1-x : x;
            unsigned ty=p->flags&0x80 ? p->size-1-y : y;
            put_pixel(source,p->tile,tx,ty,mid[p->y+y][p->x+x]);
        }
    }
    if (!memcmp(source,ppu->vram+base,sizeof(source))) return false;
    if (!owner) { owner=ppu; memcpy(original_vram,ppu->vram,sizeof(original_vram)); }
    memcpy(ppu->vram+base,source,sizeof(source));
    return true;
}
unsigned issd_running_begin(Ppu *ppu, const uint8_t *ram,
                            const uint8_t *rom, size_t rom_size) {
    if (owner) issd_running_end(owner);
    if (!ppu || !ram || !rom || (ppu->obsel>>5)!=0 || (ppu->obsel&7)!=3 ||
        (word(ram,0x32)!=3 && word(ram,0x32)!=6) || word(ram,0x70)!=8 ||
        !word(ram,0x50) || (ppu->bgmode&0xf7)!=1 ||
        ppu->bgXsc[0]!=3 || ppu->bgXsc[1]!=0x13) return 0;
    unsigned count=0;
    for (unsigned object=0x600;object<0x1b00;object+=0x100)
        if (object!=0x1000 && word(ram,object) &&
            enhance(ppu,ram,object,rom,rom_size)) count++;
    return count;
}
void issd_running_end(Ppu *ppu) {
    if (!ppu || owner!=ppu) return;
    memcpy(ppu->vram,original_vram,sizeof(original_vram));
    owner=NULL;
}
