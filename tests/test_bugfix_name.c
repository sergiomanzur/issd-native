#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifndef ISSD_NAME_RED
#include "issd_bugfix_name.h"
#else
static bool issd_bugfix_name_skip_caret(const uint8_t *r, uint16_t d, bool on) {
    (void)r; (void)d; (void)on; return false;
}
#endif
#ifdef ISSD_NAME_ROM_TEST
#define main password_fixture_main
#include "test_password_codec.c"
#undef main
static void run_name(Bus *bus, Interp816 *cpu, bool enabled) {
    cpu->dp=0x1500; cpu->db=0x81; cpu->k=0x8a; cpu->pc=0xb3fe;
    cpu->e=cpu->mf=cpu->xf=false; cpu->sp=0x1ffc;
    bus->ram[0x1ffd]=0xfe; bus->ram[0x1ffe]=0xff;
    for(unsigned step=0;step<10000;step++) {
        if(cpu->k==0x8a && cpu->pc==0xffff) return;
        if(cpu->k==0x8a && cpu->pc==0xb3fe &&
           issd_bugfix_name_skip_caret(bus->ram,cpu->dp,enabled)) cpu->pc=0xb3fd;
        interp816_runOpcode(cpu);
    }
    assert(!"name caret routine exceeded opcode budget");
}
int main(int argc, char **argv) {
    assert(argc==2);
    Bus *bus=calloc(1,sizeof *bus); assert(bus);
    bus->size=0x200000; bus->rom=malloc(bus->size); assert(bus->rom);
    load(argv[1],bus->rom,bus->size);
    Interp816 *cpu=interp816_init(bus,read_bus,write_bus); assert(cpu);
    uint8_t original[0x20000];
    for(unsigned letters=0;letters<=3;letters++) {
        memset(bus->ram,0,sizeof bus->ram); bus->ram[0x156c]=(uint8_t)letters;
        bus->ram[0x49]=0x32; /* Real DMA queue cursor points at $7E3200. */
        run_name(bus,cpu,false); memcpy(original,bus->ram,sizeof original);
        assert(bus->ram[0x48]==10); /* Real code queues one DMA VRAM upload. */
        unsigned source=bus->ram[0x3206]|(unsigned)bus->ram[0x3207]<<8;
        unsigned destination=bus->ram[0x3204]|(unsigned)bus->ram[0x3205]<<8;
        assert(source==(letters<3 ? 0xd41a+letters*2 : 0x8ce0));
        assert(destination==(letters<3 ? 0x20d+letters : 0x5e70));
        if(letters==3) {
            /* The original indexed load fetches adjacent 81E047 (script pointer)
             * instead of a caret location. Its low word enters SourceLo. */
            assert((read_bus(bus,0x81e047)|read_bus(bus,0x81e048)<<8)==0xbce0);
        }
        memset(bus->ram,0,sizeof bus->ram); bus->ram[0x156c]=(uint8_t)letters;
        bus->ram[0x49]=0x32;
        run_name(bus,cpu,true);
        if(letters<3) assert(!memcmp(original,bus->ram,sizeof original));
        else {
            assert(bus->ram[0x48]==0 && bus->ram[0x49]==0x32);
            for(unsigned a=0x3200;a<0x320a;a++) assert(bus->ram[a]==0);
            assert(bus->ram[0x156c]==3);
        }
    }
    interp816_free(cpu); free(bus->rom); free(bus); return 0;
}
#else
int main(void) {
    uint8_t ram[0x20000]={0}, saved[0x20000];
    for(unsigned letters=0;letters<5;letters++) {
        ram[0x156c]=(uint8_t)letters; memcpy(saved,ram,sizeof ram);
        assert(!issd_bugfix_name_skip_caret(ram,0x1500,false));
        assert(issd_bugfix_name_skip_caret(ram,0x1500,true)==(letters>=3));
        assert(!memcmp(saved,ram,sizeof ram));
    }
    assert(!issd_bugfix_name_skip_caret(ram,0x1400,true));
    assert(!issd_bugfix_name_skip_caret(NULL,0x1500,true));
    return 0;
}
#endif
