#include "issd_match_options.h"
static unsigned word(const uint8_t*r,unsigned a){return r[a]|(unsigned)r[a+1]<<8;}
bool issd_duration_valid(int m){return m==0||m==1||m==2||m==3||m==5||m==7||m==10||m==15||m==20;}
uint16_t issd_duration_bcd(int m,unsigned period,uint16_t original){
    if(!m||!issd_duration_valid(m)||period>3)return original;
    /* The original extra-time table uses roughly a third of regulation. */
    if(period>=2)m=(m+2)/3;
    return (uint16_t)(((m/10)<<12)|((m%10)<<8));
}
uint16_t issd_penalty_timing_input(const uint8_t*r,uint16_t original,bool enabled){
    if(!enabled||!r||word(r,0x32)!=6||
       (word(r,0x70)!=12&&word(r,0x70)!=17)||(word(r,0x1440)!=0&&word(r,0x1440)!=2))return original;
    /* $8C970B reads menu input $140E, bypassing the shooter routing at
     * $8CA2CF. $140C is already routed for the current taker; $141A is the
     * opposing keeper and is deliberately never changed. */
    return (uint16_t)word(r,0x140c);
}
bool issd_final_attack(const uint8_t*r,bool enabled){
    if(!enabled||!r||word(r,0x32)!=6||word(r,0x70)!=8||word(r,0xbc)||
       word(r,0xba)||word(r,0xc0)||word(r,0x16d0)||word(r,0x16d6)>=600)return false;
    unsigned actor=word(r,0x45c),length=word(r,0x12a2),width=word(r,0x12a4);
    if(actor<0x500||actor>0x1a00||(actor&255)||!word(r,actor+0x30)||
       length<1024||length>4096||width<256||width>1024)return false;
    unsigned team=word(r,actor+0x9a),x=word(r,0x42a),y=word(r,0x42c);
    if((team!=0xd00&&team!=0xe00)||x>=length||y>=width)return false;
    /* Original penalty areas: 384 longitudinal units, 192 each side of
     * midfield. The active team's $A4 side bit survives halftime swaps. */
    return y>=width/2-192&&y<width/2+192&&
        ((word(r,team+0xa4)&0x8000)?x<384:x>=length-384);
}
bool issd_short_goal_kick(const uint8_t*r,uint16_t actor,bool enabled){
    if(!enabled||!r||(actor!=0x500&&actor!=0x1000)||word(r,0x32)!=6||
       word(r,0x70)!=8||word(r,0xbc))return false;
    unsigned controller=word(r,0x88);
    if(controller<0x1aa0||controller>0x1b60||(controller-0x1aa0)%0x30)return false;
    return !(word(r,controller+0x2e)&0x8000)&&word(r,controller+0x2c)==actor&&
        (word(r,controller+0x14)&0x8000)!=0;
}
