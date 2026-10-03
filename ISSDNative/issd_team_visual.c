#include "issd_team_visual.h"
#include <limits.h>
#include <string.h>
#include "snes/ppu.h"
#include "issd_mod.h"
extern bool issd_menu_team_flag_available(int team);
extern void issd_menu_draw_team_identity(uint32_t *,int,int,int,int,int,int,int,int,_Bool);
static uint16_t word(const uint8_t *p,size_t a) {return p[a]|((uint16_t)p[a+1]<<8);}
/* $82:F59A indexes the cartridge's 42 big-name descriptors. $80:9539
 * draws a RAM descriptor via $80:9896/$9924; its three parallel arrays
 * contain signed x/y and tile/properties at +$2000/$4000/$6000. */
static int decode(const uint8_t *ram,int actor,int team,IssdTeamVisual *v) {
    unsigned ptr=word(ram,actor), count=ram[ptr];
    if (!ptr || ptr>=0x8000 || !count || count>ISSD_TEAM_VISUAL_PARTS ||
        ptr+count*2>=0xA000) return 0;
    memset(v,0,sizeof *v); v->team=team;v->actor=actor;v->part_count=(int)count;
    int minx=INT_MAX,miny=INT_MAX,maxx=INT_MIN,maxy=INT_MIN;
    int ax=(int16_t)word(ram,actor+8), ay=(int16_t)word(ram,actor+12)+(int16_t)word(ram,actor+16);
    uint16_t base=word(ram,actor+2);
    uint8_t priority=ram[actor+5]&0x80 ? ram[actor+5] : ram[0x7C];
    uint8_t attrs=(uint8_t)((base>>8)|(((priority&0x30)|ram[actor+4])&0xF0));
    /* Flipped actors use separate guest paths; reject rather than guess. */
    if (attrs&0xC0) return 0;
    for (unsigned i=0;i<count;i++) {
        unsigned e=ptr+i*2;
        int large=ram[e+1]&0x80;
        int size=large?16:8;
        int x=ax-(large?8:4)+(int16_t)word(ram,e+0x2000);
        int y=ay-(large?8:4)+(int16_t)word(ram,e+0x4000);
        uint8_t prop=ram[e+0x6001];
        uint8_t attr=(prop&0xC1)^attrs;
        if(prop&0x20) attr=(attr&0x08)?(attr|2):(attr&~4)|8;
        v->parts[i]=(IssdTeamVisualPart){x,y,size,(uint16_t)((uint8_t)(ram[e+0x6000]+base)|((uint16_t)attr<<8))};
        if(x<minx)minx=x;if(y<miny)miny=y;
        if(x+size>maxx)maxx=x+size;if(y+size>maxy)maxy=y+size;
    }
    v->x=minx;v->y=miny;v->width=maxx-minx;v->height=maxy-miny;
    return 1;
}
size_t issd_team_visual_collect(const uint8_t *ram,const uint8_t *rom,
                               size_t rom_size,IssdTeamVisual *out,size_t capacity) {
    if(!ram||!rom||!out||rom_size<0x1759A+84)return 0;
    size_t n=0;
    /* $80:95E0 follows this zero-terminated list, rather than traversing
     * allocated/stale actors. Bound protects malformed private snapshots. */
    for(unsigned slot=0;slot<128 && n<capacity;slot++) {
        unsigned actor=word(ram,0x1D40+slot*2);
        if(!actor)break;
        if(actor>0x1D00 || actor<0x400)continue;
        unsigned ptr=word(ram,actor);
        for(int team=0;team<42;team++) {
            if(ptr && ptr==word(rom,0x1759A+team*2)) {
                if(decode(ram,(int)actor,team,out+n))n++;
                break;
            }
        }
    }
    return n;
}
size_t issd_team_visual_match_oam(const IssdTeamVisual *v,const uint16_t oam[256],
                                const uint8_t high[32],uint8_t indices[ISSD_TEAM_VISUAL_PARTS]) {
    if(!v||!oam||!high||!indices)return 0;
    size_t n=0;uint8_t used[128]={0};
    for(int p=0;p<v->part_count;p++) {
        const IssdTeamVisualPart *part=v->parts+p;
        for(unsigned i=0;i<128;i++) {
            unsigned hi=(high[i/4]>>((i%4)*2))&3;
            int x=(oam[i*2]&255)|((hi&1)<<8);if(x>=256)x-=512;
            int y=oam[i*2]>>8;
            if(!used[i] && x==part->x && y==(part->y&255) &&
               ((hi&2)?16:8)==part->size && oam[i*2+1]==part->tile_attributes) {
                used[i]=1;indices[n++]=(uint8_t)i;break;
            }
        }
    }
    return n;
}
static IssdTeamVisual presented[96];
static size_t presented_count;
static uint16_t saved_oam[256];
static uint8_t saved_high[32];
static Ppu *transaction;
static void present(Ppu *ppu,const IssdTeamVisual *v) {
    uint8_t indices[64];
    IssdTeamVisual visible=*v;
    visible.part_count=0;
    for(int i=0;i<v->part_count;i++) {
        const IssdTeamVisualPart *p=v->parts+i;
        if(p->x<256 && p->x+p->size>0 && p->y<224 && p->y+p->size>0)
            visible.parts[visible.part_count++]=*p;
    }
    size_t n=issd_team_visual_match_oam(&visible,ppu->oam,ppu->highOam,indices);
    /* A stale actor image must not erase half a name or double the rest.
     * Accept only complete visible identities for the displayed frame. */
    if(!n || n!=(size_t)visible.part_count || v->width<=0 || v->width>256 ||
       v->height<=0 || v->height>64 || presented_count>=96)return;
    presented[presented_count++]=*v;
    for(size_t j=0;j<n;j++)ppu->oam[indices[j]*2]=(ppu->oam[indices[j]*2]&255)|0xF000;
}
static void present_flag(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,size_t size,unsigned actor,int team) {
    unsigned ptr=word(ram,actor);
                /* ROM flag descriptor: count then signed y,x,tile,props. */
                size_t source=0x40000+(ptr&0x7FFF);
                if(source>=size)return;
                unsigned parts=rom[source];
                if(!parts||parts>64||source+1+parts*4>size)return;
                IssdTeamVisual flag={0};flag.team=team;flag.actor=(int)actor;flag.flag=1;
                flag.part_count=(int)parts;flag.x=INT_MAX;flag.y=INT_MAX;
                int right=INT_MIN,bottom=INT_MIN;
                uint16_t base=word(ram,actor+2);
                for(unsigned p=0;p<parts;p++) {
                    const uint8_t *d=rom+source+1+p*4;
                    int large=(d[3]&0x10)!=0,sz=large?16:8;
                    int x=(int16_t)word(ram,actor+8)-(large?8:4)+(int8_t)d[1];
                    int y=(int16_t)word(ram,actor+12)+(int16_t)word(ram,actor+16)-(large?8:4)+(int8_t)d[0];
                    uint8_t attrs=(uint8_t)((base>>8)|(((ram[0x7C]&0x30)|ram[actor+4])&0xF0));
                    uint8_t prop=(d[3]&0xC1)^attrs;
                    flag.parts[p]=(IssdTeamVisualPart){x,y,sz,(uint16_t)((uint8_t)(d[2]+base)|((uint16_t)prop<<8))};
                    if(x<flag.x)flag.x=x;if(y<flag.y)flag.y=y;
                    if(x+sz>right)right=x+sz;if(y+sz>bottom)bottom=y+sz;
                }
                flag.width=right-flag.x;flag.height=bottom-flag.y;present(ppu,&flag);

}
void issd_team_visual_begin(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,size_t size) {
    if(transaction)issd_team_visual_end(transaction);
    presented_count=0;
    if(!ppu||!ram||!rom||size<0x175EE || ram[0x32]!=6 ||
       (ram[0x70]!=12 && ram[0x70]!=18 && ram[0x70]!=15))return;
    memcpy(saved_oam,ppu->oam,sizeof saved_oam);memcpy(saved_high,ppu->highOam,sizeof saved_high);
    transaction=ppu;
    int flag_team[8]={-1,-1,-1,-1,-1,-1,-1,-1};
    IssdTeamVisual names[48];
    size_t count=issd_team_visual_collect(ram,rom,size,names,48);
    for(size_t i=0;i<count;i++) {
        IssdTeamVisual *name=names+i;
        const char *label=issd_mod_team_plate_name(name->team);
        if(label&&label[0])present(ppu,name);
        if(!issd_menu_team_flag_available(name->team))continue;
        /* Both Cup $85:C7E4 and World $8B:9DB7 place the name actor
         * 24 pixels to the right of its corresponding flag actor. */
        for(unsigned slot=0;slot<128;slot++) {
            unsigned actor=word(ram,0x1D40+slot*2);if(!actor)break;
            if(actor<0x400||actor>0x1D00)continue;
            if(ram[0x70]==15) {
                unsigned partner=name->actor==0xAA0?0xAD0:name->actor==0xBA0?0xBD0:0;
                if(!partner||actor!=partner)continue;
            } else {
                int paired=(int16_t)word(ram,actor+8)+24==(int16_t)word(ram,name->actor+8) &&
                    word(ram,actor+12)==word(ram,name->actor+12);
                /* World $8B:98BC constructs flag object k and name object
                 * k+6 from DATA_81EBC5, including its two-column matchup.
                 * $8B:9DB7 uses exactly the same slots for standings. */
                if((word(ram,0x1648)&0x24)==0x20) {
                    for(unsigned k=0;k<6;k++) {
                        if(name->actor==word(rom,0xEBC5+(k+6)*2) &&
                           actor==word(rom,0xEBC5+k*2))paired=1;
                    }
                }
                if(!paired)continue;
            }
            unsigned ptr=word(ram,actor);
            for(unsigned f=0;f<8;f++) {
                if(ram[0x70]==15) {
                    if(f || ptr!=(actor==0xAD0?0xE886:0xE88F))continue;
                } else if(ptr!=word(rom,0x17486+f*2))continue;
                if(ram[0x70]!=15) {
                    if(flag_team[f]==-1 || flag_team[f]==name->team)flag_team[f]=name->team;
                    else flag_team[f]=-2;
                }
                if(ram[0x70]==15)present_flag(ppu,ram,rom,size,actor,name->team);
                break;
            }
        }
    }
    /* Cup displays the same loaded flag slot in its list, group grid and
     * next-game preview. A verified name/slot association applies to every
     * active actor using that descriptor in this frame. Conflicts reject
     * the slot instead of guessing from descriptor number alone. */
    if(ram[0x70]!=15) {
        for(unsigned slot=0;slot<128;slot++) {
            unsigned actor=word(ram,0x1D40+slot*2);if(!actor)break;
            if(actor<0x400||actor>0x1D00)continue;
            for(unsigned f=0;f<8;f++) {
                if(flag_team[f]>=0 && word(ram,actor)==word(rom,0x17486+f*2))
                    present_flag(ppu,ram,rom,size,actor,flag_team[f]);
            }
        }
    }

}
void issd_team_visual_render(uint32_t *fb,int width,int height,int margin) {
    for(size_t i=0;i<presented_count;i++) {
        IssdTeamVisual *v=presented+i;
        issd_menu_draw_team_identity(fb,width,height,margin,v->team,v->x,v->y,v->width,v->height,v->flag!=0);
    }
}
void issd_team_visual_end(Ppu *ppu) {
    if(ppu&&ppu==transaction) {
        memcpy(ppu->oam,saved_oam,sizeof saved_oam);memcpy(ppu->highOam,saved_high,sizeof saved_high);
        transaction=0;
    }
}
