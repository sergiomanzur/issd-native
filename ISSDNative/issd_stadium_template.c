#include "issd_stadium_geometry.h"
#include "issd_decompress.h"
#include <stdlib.h>
#include <string.h>

static size_t offset(unsigned address) {
    return ((address & 0x7f0000) >> 1) | (address & 0x7fff);
}
static unsigned word(const uint8_t *source) {
    return source[0] | (unsigned)source[1] << 8;
}
static unsigned address24(const uint8_t *source) {
    return word(source) | (unsigned)source[2] << 16;
}
bool issd_stadium_read_native_resources(const uint8_t *rom,size_t size,unsigned base,
                                        uint8_t vram[65536],uint8_t palette[512]) {
    if (!rom || size<0x200000 || base>7 || !vram || !palette) return false;
    uint8_t *native=calloc(1,65536),*scratch=malloc(65536),colors[512]={0};
    if (!native || !scratch) {free(native);free(scratch);return false;}
    unsigned descriptors[4]={0x828578,
        0x820000|word(rom+offset(0x81ef41)+base*2),
        0x820000|word(rom+offset(0x81ef51)+base*2),
        0x820000|word(rom+offset(0x81ac2f)+base*2)};
    bool valid=true;
    for (unsigned d=0;d<4 && valid;++d) {
        size_t cursor=offset(descriptors[d]);
        if (cursor+2>size) {valid=false;break;}
        unsigned type=word(rom+cursor);cursor+=2;
        if (type==1) continue; /* original animated WRAM caches */
        if (type!=0) {valid=false;break;}
        bool complete=false;
        for (unsigned record=0;record<64;++record) {
            if (cursor>=size) break;
            if (rom[cursor]==255) {complete=true;break;}
            if (cursor+5>size) break;
            size_t destination=word(rom+cursor)*2;
            size_t source=offset(address24(rom+cursor+2));cursor+=5;
            size_t count=0;bool interleaved=false;
            if (source+2>size || !ISSD_Decompress(rom+source,size-source,scratch,
                                                65536,&count,&interleaved) ||
                destination+count>65536) break;
            if (interleaved) {
                if (count%16) break;
                for (size_t i=0;i<count;i+=16)
                    for (unsigned j=0;j<8;++j) {
                        native[destination+i+j*2]=scratch[i+j];
                        native[destination+i+j*2+1]=scratch[i+j+8];
                    }
            } else memcpy(native+destination,scratch,count);
        }
        valid=complete;
    }
    unsigned table=word(rom+offset(0x81ef91)+base*2);
    unsigned selectors[4]={0,0x1e,0x0a,4};
    unsigned sources[4]={0x899bba,0x899596,
        0x890000|word(rom+offset(0x81f021)+base*2),
        0x890000|word(rom+offset(0x810000|table))};
    for (unsigned i=0;i<4 && valid;++i) {
        unsigned destination=word(rom+offset(0x819176+selectors[i]));
        size_t source=offset(sources[i]);
        if (destination<0x2c00 || source+2>size) {valid=false;break;}
        size_t count=word(rom+source)+1;destination-=0x2c00;
        if (destination+count>512 || source+2+count>size) {valid=false;break;}
        memcpy(colors+destination,rom+source+2,count);
    }
    if (valid) {memcpy(vram,native,65536);memcpy(palette,colors,512);}
    free(native);free(scratch);return valid;
}
bool issd_stadium_read_template(const uint8_t *canonical, size_t size,
                                unsigned base_layout, IssdStadiumGeometry *output) {
    if (!canonical || !output || base_layout > 7 || size < 0x200000) return false;
    size_t position = offset(0x820000 | word(canonical+offset(0x81ef61)+base_layout*2));
    if (position+2 > size || word(canonical+position) != 1) return false;
    position += 2;
    uint8_t *ram = calloc(1, 0x20000), *scratch = malloc(0x10000);
    if (!ram || !scratch) { free(ram); free(scratch); return false; }
    bool complete = false;
    for (unsigned record = 0; record < 64; ++record) {
        if (position+2 > size) break;
        if (word(canonical+position) == 0xffff) { complete = true; break; }
        if (position+6 > size) break;
        unsigned destination = address24(canonical+position);
        unsigned source = address24(canonical+position+3);
        position += 6;
        unsigned bank = destination >> 16;
        if (bank != 0x7e && bank != 0x7f) break;
        size_t target = (destination & 0xffff) + (bank == 0x7f ? 0x10000 : 0);
        size_t start = offset(source & 0xbfffff), count = 0;
        bool interleaved = false;
        if (start+2 > size || !ISSD_Decompress(canonical+start, size-start,
                scratch, 0x10000, &count, &interleaved)) break;
        if (source & 0x400000) {
            if (count > (0x20000-target)/2) break;
            for (size_t i = 0; i < count; ++i) ram[target+i*2] = scratch[i];
        } else {
            if (count > 0x20000-target || (interleaved && count%16)) break;
            if (interleaved) {
                for (size_t i = 0; i < count; i += 16) {
                    for (unsigned j = 0; j < 8; ++j) {
                        ram[target+i+j*2] = scratch[i+j];
                        ram[target+i+j*2+1] = scratch[i+j+8];
                    }
                }
            } else memcpy(ram+target, scratch, count);
        }
    }
    if (complete) {
        IssdStadiumGeometry result = {0};
        result.base_layout = (uint16_t)base_layout;
        result.length = (uint16_t)word(canonical+offset(0x81ec47)+base_layout*4);
        result.width = (uint16_t)word(canonical+offset(0x81ec49)+base_layout*4);
        result.stride = (uint16_t)word(canonical+offset(0x81ee71)+base_layout*2);
        for (unsigned layer = 0; layer < 2; ++layer) {
            memcpy(result.metatiles[layer], ram+0x18000+layer*0x2000, 8192);
            memcpy(result.world_maps[layer], ram+0x1d000+layer*0x1000, 4096);
        }
        *output = result;
    }
    free(ram); free(scratch);
    return complete;
}
