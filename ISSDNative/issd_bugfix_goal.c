#include "issd_bugfix_goal.h"
static unsigned word(const uint8_t *r, unsigned a) {
    return r[a] | (unsigned)r[a+1] << 8;
}
bool issd_bugfix_goal_reject(const uint8_t *ram, bool enabled) {
    if (!enabled || !ram || word(ram,0x32)!=6 || word(ram,0x70)!=8 ||
        word(ram,0xba) || word(ram,0xbc)) return false;
    unsigned length=word(ram,0x12a2), width=word(ram,0x12a4);
    if (length<1024 || length>4096 || width<256 || width>1024) return false;
    /* 838747 snapshots screen coordinates before movement; 80DCA9 converts
     * them using the unchanged camera origin in this same ball update. */
    int previous=(int16_t)(uint16_t)(word(ram,0x19e0)+word(ram,0x98)-
                                    word(ram,0x19e4)-word(ram,0x9a));
    int current=(int16_t)word(ram,0x42a);
    /* Match 838D61's four-unit radius exactly. Reject an award only when
     * there was no crossing from the playable side of either goal line. */
    return (current < -4 || current >= (int)length+4) &&
           (previous < -4 || previous >= (int)length+4);
}
bool issd_bugfix_goal_score(uint8_t *ram, uint16_t team, bool enabled) {
    if (!enabled || !ram || (team!=0xd00 && team!=0xe00)) return false;
    if (word(ram,team+0xa2)<100) return false;
    ram[team+0xa2]=99; ram[team+0xa3]=0; return true;
}
bool issd_bugfix_goal_restart(uint8_t *ram, bool enabled) {
    if (!enabled || !ram || word(ram,0x32)!=6 || word(ram,0x70)!=8 ||
        word(ram,0xbc)!=3) return false;
    unsigned length=word(ram,0x12a2), width=word(ram,0x12a4);
    if (length<1024 || length>4096 || width<256 || width>1024) return false;
    int x=(int16_t)word(ram,0xca), y=(int16_t)word(ram,0xce);
    if (x>=0 && x<=(int)length && y>=0 && y<=(int)width) return false;
    /* A4DBDB copies the fouled player's world position, including positions
     * outside the pitch, into both restart records. Bring only invalid axes
     * inside by the same four-unit ball radius used in 838D61. */
    if (x<0) x=4; else if (x>(int)length) x=(int)length-4;
    if (y<0) y=4; else if (y>(int)width) y=(int)width-4;
    ram[0xca]=ram[0x11f0]=(uint8_t)x;
    ram[0xcb]=ram[0x11f1]=(uint8_t)((unsigned)x>>8);
    ram[0xce]=ram[0x11f2]=(uint8_t)y;
    ram[0xcf]=ram[0x11f3]=(uint8_t)((unsigned)y>>8);
    /* DBF6 still computes the Y quadrant; repair the already-computed X half. */
    ram[0xbe]=(uint8_t)(x>=(int)word(ram,0x12f2)?2:0); ram[0xbf]=0;
    return true;
}
