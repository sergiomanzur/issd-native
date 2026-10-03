#include "issd_team_visual.h"
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "snes/ppu.h"
const char *issd_mod_team_plate_name(int team) {(void)team;return "CUSTOM";}
static bool have_flag;
static int draw_count;
bool issd_menu_team_flag_available(int team) {(void)team;return have_flag;}
void issd_menu_draw_team_identity(uint32_t *fb,int w,int h,int m,int t,int x,int y,int bw,int bh,bool f) {
    draw_count++;
    (void)fb;(void)w;(void)h;(void)m;(void)t;(void)x;(void)y;(void)bw;(void)bh;(void)f;
}
static unsigned char ram[0x20000], rom[0x50000];
static Ppu ppu;
static void word(unsigned char *p, unsigned a, unsigned v) {p[a]=v;p[a+1]=v>>8;}
int main(void) {
    IssdTeamVisual v[4];
    word(rom,0x1759A+30*2,0x5020);
    word(ram,0x1D40,0x0400);
    word(ram,0x0400,0x5020); word(ram,0x0408,70); word(ram,0x040C,90);
    ram[0x5020]=2;
    word(ram,0x7020,0); word(ram,0x9020,0); word(ram,0xB020,0x0020);
    word(ram,0x7022,8); word(ram,0x9022,0); word(ram,0xB022,0x0021);
    assert(issd_team_visual_collect(ram,rom,sizeof rom,v,4)==1);
    assert(v[0].team==30 && v[0].x==66 && v[0].y==86 && v[0].width==16);
    uint16_t oam[256]={0};uint8_t high[32]={0},indices[64];
    oam[0]=66|(86<<8);oam[1]=0x20;oam[2]=74|(86<<8);oam[3]=0x21;
    assert(issd_team_visual_match_oam(v,oam,high,indices)==2);
    oam[3]=0x22; assert(issd_team_visual_match_oam(v,oam,high,indices)==1);
    memcpy(ppu.oam,oam,sizeof oam);ram[0x32]=6;ram[0x70]=18;
    issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert(memcmp(ppu.oam,oam,sizeof oam)==0);
    /* An offscreen exact match cannot compensate for a visible wrong tile. */
    issd_team_visual_end(&ppu);
    word(ram,0x0408,0xFFFC);memset(&ppu,0,sizeof ppu);
    ppu.oam[0]=248|(86<<8);ppu.oam[1]=0x20;ppu.highOam[0]=1;
    ppu.oam[2]=0|(86<<8);ppu.oam[3]=0x22;
    draw_count=0;issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert((ppu.oam[0]>>8)==86 && (ppu.oam[2]>>8)==86);
    issd_team_visual_render(0,256,224,0);assert(draw_count==0);
    issd_team_visual_end(&ppu);word(ram,0x0408,70);
    issd_team_visual_end(&ppu);
    /* World/Cup paired flag uses the original actor and ROM parts, not
     * the name's text bounds or an assumed standings row position. */
    word(ram,0x1D42,0x0500);word(ram,0x0500,0xE898);
    word(ram,0x0508,46);word(ram,0x050C,90);word(ram,0x0502,0x0A00);
    word(rom,0x17486,0xE898);
    unsigned f=0x46898;rom[f]=2;
    rom[f+1]=7;rom[f+2]=8;rom[f+3]=0;rom[f+4]=0x19;
    rom[f+5]=7;rom[f+6]=16;rom[f+7]=1;rom[f+8]=0x19;
    ppu.oam[4]=46|(89<<8);ppu.oam[5]=0x0B00;
    ppu.oam[6]=54|(89<<8);ppu.oam[7]=0x0B01;ppu.highOam[0]=0xA0;
    have_flag=true;ram[0x32]=6;
    issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert((ppu.oam[4]>>8)==240 && (ppu.oam[6]>>8)==240);
    issd_team_visual_end(&ppu);have_flag=false;
    /* One verified Cup row association must also update duplicate flags
     * using the same loaded slot in the group grid/next-game preview. */
    word(ram,0x1D44,0x0600);word(ram,0x0600,0xE898);
    word(ram,0x0608,120);word(ram,0x060C,150);word(ram,0x0602,0x0A00);
    ppu.oam[8]=120|(149<<8);ppu.oam[9]=0x0B00;
    ppu.oam[10]=128|(149<<8);ppu.oam[11]=0x0B01;ppu.highOam[1]=0x0A;
    have_flag=true;issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert((ppu.oam[8]>>8)==240 && (ppu.oam[10]>>8)==240);
    issd_team_visual_end(&ppu);have_flag=false;
    issd_team_visual_end(&ppu);
    /* Two different teams claiming one loaded slot invalidate every copy.
     * Association collection must finish before any flags are hidden. */
    word(ram,0x1D46,0x0700);word(rom,0x1759A+31*2,0x5040);
    memcpy(ram+0x0700,ram+0x0400,0x40);word(ram,0x0700,0x5040);
    word(ram,0x0708,144);word(ram,0x070C,150);ram[0x5040]=2;
    word(ram,0x7040,0);word(ram,0x9040,0);word(ram,0xB040,0x20);
    word(ram,0x7042,8);word(ram,0x9042,0);word(ram,0xB042,0x21);
    have_flag=true;issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert((ppu.oam[4]>>8)==89 && (ppu.oam[8]>>8)==149);
    issd_team_visual_end(&ppu);have_flag=false;
    word(ram,0x1D46,0);word(rom,0x1759A+31*2,0);
    /* World matchup source pairs object slots k/k+6, with asymmetric
     * coordinates. The sorted name descriptor still defines the team. */
    memcpy(ram+0x0700,ram+0x0400,0x40);memcpy(ram+0x0F00,ram+0x0500,0x40);
    word(ram,0x1D40,0x0700);word(ram,0x1D42,0x0F00);word(ram,0x1D44,0);
    word(rom,0xEBC5,0x0F00);word(rom,0xEBC5+12,0x0700);word(ram,0x1648,0x20);
    word(ram,0x0708,48);word(ram,0x070C,64);word(ram,0x0F08,23);word(ram,0x0F0C,65);
    ppu.oam[0]=44|(60<<8);ppu.oam[1]=0x20;ppu.oam[2]=52|(60<<8);ppu.oam[3]=0x21;
    ppu.oam[4]=23|(64<<8);ppu.oam[5]=0x0B00;ppu.oam[6]=31|(64<<8);ppu.oam[7]=0x0B01;
    have_flag=true;issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert((ppu.oam[4]>>8)==240 && (ppu.oam[6]>>8)==240);
    issd_team_visual_end(&ppu);have_flag=false;
    word(ram,0x1D40,0x0400);word(ram,0x1D42,0);word(ram,0x1648,0);
    oam[3]=0x21;memcpy(ppu.oam,oam,sizeof oam);
    issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert((ppu.oam[0]>>8)==240 && (ppu.oam[2]>>8)==240);
    issd_team_visual_end(&ppu);
    assert(memcmp(ppu.oam,oam,sizeof oam)==0);
    ram[0x32]=0;issd_team_visual_begin(&ppu,ram,rom,sizeof rom);
    assert(memcmp(ppu.oam,oam,sizeof oam)==0);
    /* Identity follows the real descriptor, independent of sort/page/index. */
    word(rom,0x1759A+30*2,0); word(rom,0x1759A+2*2,0x5020);
    assert(issd_team_visual_collect(ram,rom,sizeof rom,v,4)==1 && v[0].team==2);
    word(ram,0x0408,0xFFF0);
    assert(issd_team_visual_collect(ram,rom,sizeof rom,v,4)==1 && v[0].x==-20);
    ram[0x5020]=65; assert(issd_team_visual_collect(ram,rom,sizeof rom,v,4)==0);
    assert(issd_team_visual_collect(ram,rom,20,v,4)==0);
    return 0;
}
