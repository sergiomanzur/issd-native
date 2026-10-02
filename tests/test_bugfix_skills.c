#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifdef ISSD_SKILLS_ROM_TEST
#define main password_fixture_main
#include "test_password_codec.c"
#undef main
#endif
#ifndef ISSD_SKILLS_RED
#include "issd_bugfix_skills.h"
#else
static bool issd_bugfix_skills(uint8_t *r, uint16_t d, bool on) {
    (void)r; (void)d; (void)on; return false;
}
#endif
static uint8_t ram[0x20000], saved[0x20000];
#ifndef ISSD_SKILLS_ROM_TEST
static unsigned word(unsigned a) { return ram[a] | ram[a+1] << 8; }
#endif
static void put(unsigned a, unsigned v) { ram[a]=(uint8_t)v; ram[a+1]=(uint8_t)(v>>8); }
static void fixture(unsigned side, unsigned group) {
    memset(ram, 0, sizeof ram);
    put(0x15e4,100);
    put(0x15ec,1); put(0x151e,1); put(0x1522,side); put(0x1546,group);
    put(0x1542+group*2,0); put(0x159e,0xffff);
    /* Last row's skill cache remains visible after the first Down enters
     * the footer. Goalkeeper uses a nonidentity roster mapping (record 3). */
    memset(ram+0x15aa,8,9);
    ram[0x3f90+side*20+group*11]=0x43;
    unsigned player=0x3e00+side*200+30;
    ram[player]=0x10; ram[player+1]=0x32; ram[player+2]=0x54;
    ram[player+3]=0x76; ram[player+4]=0xa8;
    memcpy(ram+0xc100+group*99,"\0\1\2\3\4\5\6\7\10",9);
}
/* Original 86B12C footer branch: no 869F8F/86A6F5 refresh. */
#ifndef ISSD_SKILLS_ROM_TEST
static void original_second_down(unsigned group) {
    put(0x151e,0); put(0x1542+group*2,0);
}
/* 86B1C6 permits refund only while the selected skill exceeds its original
 * C100 baseline. Skill zero costs three points (8A97DA/8A97EC). */
static void original_remove_excess(unsigned group) {
    unsigned baseline=ram[0xc100+group*99];
    while ((ram[0x15aa]&15)>baseline) {
        ram[0x15aa]--;
        put(0x15e4,word(0x15e4)-3);
    }
}
#endif
#ifdef ISSD_SKILLS_ROM_TEST
static void skills_write_bus(void *opaque, uint32_t address, uint8_t value) {
    write_bus(opaque,address,value);
    if ((address & 0xffff)==0x4203) {
        Bus *bus=opaque;
        unsigned product=bus->io[2]*value;
        bus->io[0x16]=(uint8_t)product; bus->io[0x17]=(uint8_t)(product>>8);
    }
}
static void execute_until(Bus *bus, Interp816 *cpu, unsigned pc, unsigned stop) {
    cpu->k=pc>>16; cpu->pc=(uint16_t)pc;
    cpu->dp=0x1500; cpu->db=0x81; cpu->sp=0x1ffb;
    cpu->e=cpu->mf=cpu->xf=false;
    bus->ram[0x1ffc]=0xfe; bus->ram[0x1ffd]=0xff; bus->ram[0x1ffe]=0x7f;
    for(unsigned i=0;i<10000;i++) {
        if (((unsigned)cpu->k<<16|cpu->pc)==stop) return;
        interp816_runOpcode(cpu);
    }
    assert(!"original skills routine exceeded opcode budget");
}
int main(int argc,char **argv) {
    assert(argc==2);
    Bus *bus=calloc(1,sizeof *bus); assert(bus);
    bus->size=0x200000; bus->rom=malloc(bus->size); assert(bus->rom);
    load(argv[1],bus->rom,bus->size);
    Interp816 *cpu=interp816_init(bus,read_bus,skills_write_bus); assert(cpu);
    const unsigned group=0;
    for(unsigned side=0;side<2;side++)
    for(unsigned enabled=0;enabled<2;enabled++) {
        fixture(side,group); memcpy(bus->ram,ram,sizeof ram);
        bus->ram[0x151e]=0;
        bus->ram[0x1542+group*2]=(uint8_t)(group?8:10);
        execute_until(bus,cpu,0x86b12c,0x7fffff);
        assert(bus->ram[0x151e]==1 && bus->ram[0x1542+group*2]==0);
        assert(bus->ram[0x159e]==0xff && bus->ram[0x159f]==0xff);
        assert(bus->ram[0x15aa]==8); /* First Down enters footer, keeps cache. */
        assert(issd_bugfix_skills(bus->ram,0x1500,enabled)==(bool)enabled);
        execute_until(bus,cpu,0x86b12c,0x7fffff);
        assert(bus->ram[0x151e]==0 && bus->ram[0x1542+group*2]==0);
        assert(bus->ram[0x15aa]==(enabled?0:8));
        /* Editing starts with that still-stale cache. Run the actual decrement
         * and cost checks, stopping before audio/display publication. */
        bus->ram[0x154a]=(uint8_t)(group*11); bus->ram[0x1542]=0;
        if (!enabled) {
            execute_until(bus,cpu,0x86b1c6,0x86b1fb);
            assert(bus->ram[0x15aa]==7 && bus->ram[0x15e4]==97);
        } else {
            execute_until(bus,cpu,0x86b1c6,0x86b1ff);
            assert(bus->ram[0x15aa]==0 && bus->ram[0x15e4]==100);
        }
        /* Publish with the cartridge's actual packed skill writer, proving
         * the stale displayed cache really corrupts the selected player. */
        cpu->y=(uint16_t)(group*11);
        execute_until(bus,cpu,0x86b357,0x86b3ac);
        assert((bus->ram[0x3e00+side*200+30]&15)==(enabled?0:7));
    }
    interp816_free(cpu); free(bus->rom); free(bus);
    return 0;
}
#else
int main(void) {
    const unsigned group=0;
    for (unsigned side=0;side<2;side++) {
        fixture(side,group); memcpy(saved,ram,sizeof ram);
        assert(!issd_bugfix_skills(ram,0x1500,false));
        assert(!memcmp(saved,ram,sizeof ram));
        original_second_down(group); original_remove_excess(group);
        assert(word(0x15e4)==76); /* Original grants 24 phantom points. */
        fixture(side,group);
        memcpy(saved,ram,sizeof ram);
        assert(issd_bugfix_skills(ram,0x1500,true));
        for(unsigned a=0;a<sizeof ram;a++)
            if (!(a>=0x15aa && a<=0x15b2) && !(a>=0xcd00 && a<=0xcd08) &&
                a!=0x159e && a!=0x159f) assert(ram[a]==saved[a]);
        assert(word(0x15e4)==100 && word(0x151e)==1);
        original_second_down(group); original_remove_excess(group);
        assert(word(0x15e4)==100);
        assert(!memcmp(ram+0x15aa,"\0\1\2\3\4\5\6\7\10",9));
        assert(word(0x159e)==group*11);
        assert(!memcmp(ram+0xcd00,ram+0xc100+group*99,9));
        /* Repeat the exact footer transition: no cumulative refund. */
        for(unsigned repeat=0;repeat<8;repeat++) {
            put(0x151e,1); memset(ram+0x15aa,8,9);
            assert(issd_bugfix_skills(ram,0x1500,true));
            original_second_down(group); original_remove_excess(group);
            assert(word(0x15e4)==100);
        }
        /* A legitimately purchased goalkeeper skill remains refundable. */
        fixture(side,group);
        ram[0x3e00+side*200+30]=0x14;
        put(0x15e4,12);
        assert(issd_bugfix_skills(ram,0x1500,true));
        assert(word(0x15e4)==12 && ram[0x15aa]==4);
        original_second_down(group); original_remove_excess(group);
        assert(word(0x15e4)==0);
    }
    const unsigned guards[]={0x15ec,0x151c,0x151e,0x1522,0x1546};
    const unsigned bad[]={0,1,0,2,1};
    for(unsigned i=0;i<5;i++) {
        fixture(0,0); put(guards[i],bad[i]); memcpy(saved,ram,sizeof ram);
        assert(!issd_bugfix_skills(ram,0x1500,true));
        assert(!memcmp(saved,ram,sizeof ram));
    }
    fixture(0,0); memcpy(saved,ram,sizeof ram);
    assert(!issd_bugfix_skills(ram,0x1400,true));
    assert(!memcmp(saved,ram,sizeof ram));
    assert(!issd_bugfix_skills(NULL,0x1500,true));
    fixture(0,0); ram[0x3f90]=20; memcpy(saved,ram,sizeof ram);
    assert(!issd_bugfix_skills(ram,0x1500,true));
    assert(!memcmp(saved,ram,sizeof ram));
    fixture(0,0); ram[0x3e00+30]=0x1a; memcpy(saved,ram,sizeof ram);
    assert(!issd_bugfix_skills(ram,0x1500,true));
    assert(!memcmp(saved,ram,sizeof ram));
    return 0;
}
#endif
