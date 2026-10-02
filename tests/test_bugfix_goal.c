#include "issd_bugfix_goal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[0x20000], before[0x20000];
static unsigned word(unsigned a) { return ram[a] | (unsigned)ram[a+1] << 8; }
static void put(unsigned a, unsigned v) { ram[a]=(uint8_t)v; ram[a+1]=(uint8_t)(v>>8); }
static void witness(int previous, int current) {
    memset(ram, 0, sizeof ram);
    put(0x32,6); put(0x70,8); put(0x12a2,0x700); put(0x12a4,0x1c0);
    put(0x98,0x180); put(0x9a,0x80);
    put(0x19e4,0x60); put(0x19e0,(unsigned)(previous+0x60+0x80-0x180));
    put(0x42a,(unsigned)current); put(0x42c,0xe0);
}
int main(void) {
    /* Original 838D61 has C clear for x=-5, even when x=-8 last frame.
     * Leaving the goal latch reset lets it award a goal on every update. */
    witness(-8,-5);
    assert(issd_bugfix_goal_reject(ram,true));
    witness(0x706,0x704);
    assert(issd_bugfix_goal_reject(ram,true));
    witness(-4,-5); assert(!issd_bugfix_goal_reject(ram,true));
    witness(0x703,0x704); assert(!issd_bugfix_goal_reject(ram,true));
    witness(20,-5); assert(!issd_bugfix_goal_reject(ram,true));
    witness(-8,0x704); assert(issd_bugfix_goal_reject(ram,true));
    witness(-8,-5); memcpy(before,ram,sizeof ram);
    assert(!issd_bugfix_goal_reject(ram,false));
    assert(memcmp(ram,before,sizeof ram)==0);
    put(0xbc,1); assert(!issd_bugfix_goal_reject(ram,true));
    put(0xbc,0); put(0xba,1); assert(!issd_bugfix_goal_reject(ram,true));
    witness(-8,-5); put(0x70,7); assert(!issd_bugfix_goal_reject(ram,true));
    assert(!issd_bugfix_goal_reject(NULL,true));
    for (unsigned team=0xd00;team<=0xe00;team+=0x100) {
        witness(0,-5); put(team+0xa2,99);
        put(team+0xa2,word(team+0xa2)+1);
        assert(issd_bugfix_goal_score(ram,(uint16_t)team,true));
        assert(word(team+0xa2)==99);
        put(team+0xa2,98); assert(!issd_bugfix_goal_score(ram,(uint16_t)team,true));
        put(team+0xa2,word(team+0xa2)+1); assert(word(team+0xa2)==99);
        put(team+0xa2,99); memcpy(before,ram,sizeof ram);
        assert(!issd_bugfix_goal_score(ram,(uint16_t)team,false));
        assert(memcmp(before,ram,sizeof ram)==0);
        assert(!issd_bugfix_goal_score(ram,0x400,true));
    }
    assert(!issd_bugfix_goal_score(NULL,0xd00,true));
    witness(0,0); put(0xbc,3); put(0x12f2,0x380);
    put(0xca,0xfff8); put(0xce,0xe0); put(0x11f0,0xfff8); put(0x11f2,0xe0);
    assert(issd_bugfix_goal_restart(ram,true));
    assert(word(0xca)==4 && word(0x11f0)==4 && word(0xce)==0xe0);
    assert(word(0xbe)==0);
    put(0xca,0x708); put(0x11f0,0x708);
    assert(issd_bugfix_goal_restart(ram,true));
    assert(word(0xca)==0x6fc && word(0x11f0)==0x6fc && word(0xbe)==2);
    put(0xca,600); put(0xce,200); memcpy(before,ram,sizeof ram);
    assert(!issd_bugfix_goal_restart(ram,true)); assert(!memcmp(before,ram,sizeof ram));
    put(0xca,0xfff8); memcpy(before,ram,sizeof ram);
    assert(!issd_bugfix_goal_restart(ram,false)); assert(!memcmp(before,ram,sizeof ram));
    put(0xbc,1); assert(!issd_bugfix_goal_restart(ram,true));
    witness(0,0); put(0xbc,3); put(0x12f2,0x380);
    put(0xca,600); put(0xce,0xffff);
    assert(issd_bugfix_goal_restart(ram,true));
    assert(word(0xca)==600 && word(0xce)==4 && word(0x11f2)==4);
    put(0xce,0x1c1);
    assert(issd_bugfix_goal_restart(ram,true)); assert(word(0xce)==0x1bc);
    static const unsigned lengths[]={1792,1856,1984,2048,1920,1920,1792,2176};
    static const unsigned widths[]={576,640,704,640,640,576,704,704};
    for(unsigned i=0;i<8;i++) {
        witness((int)lengths[i]+3,(int)lengths[i]+4);
        put(0x12a2,lengths[i]); put(0x12a4,widths[i]);
        assert(!issd_bugfix_goal_reject(ram,true));
        witness((int)lengths[i]+4,(int)lengths[i]+5);
        put(0x12a2,lengths[i]); put(0x12a4,widths[i]);
        assert(issd_bugfix_goal_reject(ram,true));
    }
    puts("goal witness and score cap passed"); return 0;
}
