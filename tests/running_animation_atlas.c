/* Inspect every real run keyframe and its new midpoint using local cartridge
 * graphics and decompressed geometry from an actual native match. No assets are
 * embedded in the repository; the output belongs in ignored build artifacts. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../ISSDNative/issd_running.c"
static uint8_t memory[0x20000], cartridge[0x400000], atlas[SIDE*8][SIDE*16][3];
static Ppu machine;
static void store(unsigned a,unsigned v) { memory[a]=v; memory[a+1]=v>>8; }
static size_t read_file(const char *path,void *out,size_t capacity) {
    FILE *f=fopen(path,"rb"); assert(f);
    size_t n=fread(out,1,capacity,f); fclose(f); return n;
}
int main(int argc,char **argv) {
    assert(argc==5);
    size_t rom_size=read_file(argv[1],cartridge,sizeof(cartridge));
    assert(read_file(argv[2],memory,sizeof(memory))==sizeof(memory));
    assert(read_file(argv[3],&machine,sizeof(machine))==sizeof(machine));
    assert(machine.obsel==3);
    for (unsigned o=0x500;o<0x1b00;o+=0x100) store(o,0);
    store(0x802,0x60); store(0x81c,0xcf9f); store(0x818,0xcfaf);
    store(0x832,0x38); store(0x892,0xbecf); store(0x830,2);
    store(0x81e,0); store(0x832,0x38);
    memset(atlas,32,sizeof(atlas));
    unsigned changed=0;
    for (unsigned d=0;d<8;d++) for (unsigned step=0;step<8;step++) {
        const uint8_t *table=span(cartridge,rom_size,0x82cf9f+d*2,2); assert(table);
        const uint8_t *strip=span(cartridge,rom_size,0x820000|word(table,0),16); assert(strip);
        uint16_t desc=(uint16_t)(word(strip,step*2)|0x8000);
        const uint8_t *header=span(cartridge,rom_size,0x820000|desc,9); assert(header);
        store(0x800,word(header,0)); store(0x814,desc);
        store(0x816,cartridge[0x10000+0x4faf+step]/2); /* CFAF in bank82 */
        store(0x81a,((step+1)%8)*2); store(0x82e,d);
        uint16_t tiles[WORDS]; Geometry g; uint8_t image[SIDE][SIDE];
        assert(next_tiles(cartridge,rom_size,desc,tiles));
        memcpy(machine.vram+0x6600,tiles,sizeof(tiles));
        assert(geometry(memory,cartridge,rom_size,desc,&g));
        for (unsigned phase=0;phase<2;phase++) {
            if (phase) changed+=issd_running_begin(&machine,memory,cartridge,rom_size);
            canvas(&g,machine.vram+0x6600,image);
            const uint8_t *flip=span(cartridge,rom_size,0x81967a+d*2,2); assert(flip);
            for (unsigned y=0;y<SIDE;y++) for (unsigned x=0;x<SIDE;x++) {
                unsigned color=image[y][x];
                if (!color) continue;
                unsigned palette=(memory[0x803]>>1)&7;
                unsigned c=machine.cgram[128+palette*16+color];
                unsigned ax=step*SIDE*2+phase*SIDE+(word(flip,0)&0x40 ? SIDE-1-x : x),ay=d*SIDE+y;
                atlas[ay][ax][0]=(uint8_t)((c&31)*255/31);
                atlas[ay][ax][1]=(uint8_t)(((c>>5)&31)*255/31);
                atlas[ay][ax][2]=(uint8_t)(((c>>10)&31)*255/31);
            }
            if (phase) issd_running_end(&machine);
        }
    }
    FILE *out=fopen(argv[4],"wb"); assert(out);
    fprintf(out,"P6\n%d %d\n255\n",SIDE*16,SIDE*8); assert(fwrite(atlas,1,sizeof(atlas),out)==sizeof(atlas)); fclose(out);
    printf("%u of 64 real running midpoints change native pixels\n",changed);
    assert(changed==64);
    return 0;
}
