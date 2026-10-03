#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_running.h"
#include "issd_animation.h"
#include "snes/ppu.h"

static uint8_t ram[0x20000], rom[0x100000], saved_ram[0x20000];
static Ppu ppu;
static uint16_t saved_vram[0x8000];
static uint16_t saved_oam[0x100], saved_cgram[0x100];
static uint8_t saved_high[0x20];
static void put(uint8_t *p, unsigned a, unsigned v) { p[a]=v; p[a+1]=v>>8; }
static unsigned offset(unsigned b, unsigned a) { return (b&127)*0x8000+(a&0x7fff); }
static void pixel(uint16_t *tiles, unsigned x, unsigned y, unsigned color) {
    unsigned tile=(y/8)*16+x/8, line=y&7, mask=1u<<(7-(x&7));
    uint16_t *p=tiles+tile*16+line;
    for (unsigned plane=0;plane<4;plane++) {
        unsigned shift=(plane&1)*8, index=(plane/2)*8;
        p[index]=(p[index]&~(mask<<shift))|((color>>plane&1)*mask<<shift);
    }
}
static unsigned read_pixel(const uint16_t *tiles,unsigned x,unsigned y) {
    const uint16_t *p=tiles+((y/8)*16+x/8)*16+(y&7);
    unsigned b=7-(x&7);
    return (p[0]>>b&1)|((p[0]>>(b+8)&1)<<1)|((p[8]>>b&1)<<2)|((p[8]>>(b+8)&1)<<3);
}
static void fixture(unsigned direction) {
    memset(ram,0,sizeof(ram)); memset(rom,0,sizeof(rom)); memset(&ppu,0,sizeof(ppu));
    issd_animation_reset();
    put(ram,0x32,6); put(ram,0x70,8); put(ram,0x50,1);
    ppu.obsel=3; ppu.bgmode=1; ppu.bgXsc[0]=3; ppu.bgXsc[1]=0x13;
    put(ram,0x800,0x4500); put(ram,0x802,0x60);
    put(ram,0x808,120); put(ram,0x80c,100);
    put(ram,0x814,0xa749); put(ram,0x816,2); put(ram,0x818,0xcfaf);
    put(ram,0x81a,2); put(ram,0x81c,0xcf9f); put(ram,0x82e,direction);
    put(ram,0x830,2); put(ram,0x834,1); put(ram,0x832,0x38);
    put(ram,0x892,0xbecf);
    /* A 16x16 body centered eight pixels above the player's foot anchor. */
    ram[0x4500]=1; ram[0x4501]=0x80;
    put(ram,0x6500,0); put(ram,0x8500,-8); ram[0xa500]=0;
    ram[0xa501]=(direction&1) ? 0x40 : 0;
    memset(rom+offset(0x82,0xcfaf),4,8);
    for (unsigned d=0;d<8;d++) put(rom,offset(0x82,0xcf9f+d*2),0xd000+d*16);
    for (unsigned i=0;i<8;i++) {
        unsigned desc=0xa749+i*9;
        put(rom,offset(0x82,0xd000+direction*16+i*2),i==7 ? desc&0x7fff : desc);
        unsigned o=offset(0x82,desc);
        put(rom,o,0x4500); put(rom,o+2,0x8000+i*0x204); rom[o+4]=0x90;
        unsigned src=offset(0x90,0x8000+i*0x204);
        put(rom,src,0x100); put(rom,src+0x102,0x100);
    }
    uint16_t current[0x180]={0}, next[0x180]={0};
    /* Head/torso stay identical. A calf moves four pixels between keys. */
    for (unsigned y=0;y<5;y++) for (unsigned x=5;x<11;x++) {
        pixel(current,x,y,3); pixel(next,x,y,3);
    }
    for (unsigned y=10;y<16;y++) for (unsigned x=3;x<6;x++) {
        pixel(current,x,y,6+direction); pixel(next,x+4,y,6+direction);
    }
    memcpy(ppu.vram+0x6600,current,sizeof(current));
    for (unsigned i=0x170;i<0x180;i++) ppu.vram[0x6600+i]=0xbeef;
    unsigned src=offset(0x90,0x8204);
    for (unsigned row=0;row<2;row++) for (unsigned w=0;w<128;w++)
        put(rom,src+row*0x102+2+w*2,next[row*0x100+w]);
    memcpy(saved_ram,ram,sizeof(ram)); memcpy(saved_vram,ppu.vram,sizeof(saved_vram));
    memcpy(saved_oam,ppu.oam,sizeof(saved_oam)); memcpy(saved_high,ppu.highOam,sizeof(saved_high));
    memcpy(saved_cgram,ppu.cgram,sizeof(saved_cgram));
}
int main(void) {
    for (unsigned d=0;d<8;d++) {
        fixture(d);
        assert(issd_running_begin(&ppu,ram,rom,sizeof(rom))==1);
        assert(memcmp(ppu.vram,saved_vram,sizeof(saved_vram))!=0);
        /* The new calf is halfway between the authored keys, not a repeated
         * key or a colour blend. Check both ink and transparency. */
        assert(read_pixel(ppu.vram+0x6600,5,13)==6+d);
        assert(read_pixel(ppu.vram+0x6600,3,13)==0);
        assert(read_pixel(ppu.vram+0x6600,9,13)==0);
        for (unsigned y=0;y<6;y++) for (unsigned x=0;x<16;x++)
            assert(read_pixel(ppu.vram+0x6600,x,y)==read_pixel(saved_vram+0x6600,x,y));
        assert(!memcmp(ppu.vram+0x6770,saved_vram+0x6770,16*sizeof(uint16_t)));
        assert(!memcmp(ram,saved_ram,sizeof(ram)));
        assert(!memcmp(ppu.oam,saved_oam,sizeof(saved_oam)));
        assert(!memcmp(ppu.highOam,saved_high,sizeof(saved_high)));
        assert(!memcmp(ppu.cgram,saved_cgram,sizeof(saved_cgram)));
        assert(!memcmp(ppu.vram,saved_vram,0x6600*sizeof(uint16_t)));
        assert(!memcmp(ppu.vram+0x6780,saved_vram+0x6780,(0x8000-0x6780)*sizeof(uint16_t)));
        issd_running_end(&ppu);
        assert(!memcmp(ppu.vram,saved_vram,sizeof(saved_vram)));
        issd_running_end(&ppu);
        /* Frozen widened actors use the continuation's phase rather than the
         * unchanged guest timer. Getter calls do not advance it twice. */
        put(ram,0x81e,1); put(ram,0x816,4);
        uint16_t descriptor;
        assert(issd_animation_pose(0x800,ram,rom,sizeof(rom),false,&descriptor));
        assert(!issd_running_begin(&ppu,ram,rom,sizeof(rom)));
        assert(issd_animation_pose(0x800,ram,rom,sizeof(rom),false,&descriptor));
        assert(issd_animation_pose(0x800,ram,rom,sizeof(rom),false,&descriptor));
        assert(issd_running_begin(&ppu,ram,rom,sizeof(rom))==1);
        issd_running_end(&ppu); put(ram,0x81e,0);
        assert(!memcmp(ppu.vram,saved_vram,sizeof(saved_vram)));
        /* First half of a keyframe, stopped/idle, keeper and other screens stay original. */
        put(ram,0x816,4); assert(!issd_running_begin(&ppu,ram,rom,sizeof(rom)));
        put(ram,0x816,2); put(ram,0x81c,0xcef8); assert(!issd_running_begin(&ppu,ram,rom,sizeof(rom)));
        put(ram,0x81c,0xcf9f); put(ram,0x832,0x58); assert(!issd_running_begin(&ppu,ram,rom,sizeof(rom)));
        put(ram,0x832,0x38);
        memcpy(ram+0x1000,ram+0x800,0x100); put(ram,0x800,0);
        assert(!issd_running_begin(&ppu,ram,rom,sizeof(rom)));
        put(ram,0x800,0x4500); put(ram,0x70,0x12); assert(!issd_running_begin(&ppu,ram,rom,sizeof(rom)));
        put(ram,0x70,8); assert(!issd_running_begin(&ppu,ram,rom,100));
        /* Packed geometry also retains whole-sprite flips. */
        fixture(d);
        for (unsigned i=0;i<8;i++) put(rom,offset(0x82,0xa749+i*9),0x8000);
        unsigned geo=offset(0x88,0x8000);
        rom[geo]=1; rom[geo+1]=(uint8_t)-8; rom[geo+2]=0;
        rom[geo+3]=0; rom[geo+4]=0x10|((d&1)?0x40:0);
        assert(issd_running_begin(&ppu,ram,rom,sizeof(rom))==1);
        issd_running_end(&ppu);
        assert(!memcmp(ppu.vram,saved_vram,sizeof(saved_vram)));
    }
    puts("eight-direction running animation tests passed");
    return 0;
}
