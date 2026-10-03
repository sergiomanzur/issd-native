#include "issd_readability.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define GLYPH {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
const uint8_t g_issd_font8x8[96][8] = {
    [1]=GLYPH, [17]=GLYPH, [18]=GLYPH, [19]=GLYPH, [20]=GLYPH,
    [33]=GLYPH, [34]=GLYPH, [35]=GLYPH, [48]=GLYPH
};
static uint8_t ram[0x20000], saved[0x20000];
static uint32_t fb[446 * 224];
static void put(unsigned a, unsigned v) { ram[a]=(uint8_t)v; ram[a+1]=(uint8_t)(v>>8); }
static void clear(void) { for (unsigned i=0;i<446*224;i++) fb[i]=0xff226622; }
static unsigned changed(void) { unsigned n=0; for (unsigned i=0;i<446*224;i++) n += fb[i]!=0xff226622; return n; }
int main(void) {
    IssdConfig cfg={0}; cfg.radar_scale=1;
    put(0x32,6); put(0x70,8); put(0x400,0x8000);
    put(0x408,128); put(0x40c,100); put(0x410,(unsigned)-16);
    clear(); issd_readability_render(fb,446,224,95,ram,&cfg); assert(!changed());
    cfg.ball_outline=true; clear(); memcpy(saved,ram,sizeof ram);
    issd_readability_render(fb,446,224,95,ram,&cfg);
    assert(!changed()); assert(fb[84*446+223]==0xff226622); assert(!memcmp(saved,ram,sizeof ram));
    put(0x70,0x12); clear(); issd_readability_render(fb,446,224,95,ram,&cfg); assert(!changed());
    put(0x70,8); cfg.ball_outline=false; cfg.ball_shadow=true;
    clear(); issd_readability_render(fb,446,224,95,ram,&cfg); assert(changed()>0);
    cfg.ball_shadow=false; cfg.player_markers=true;
    put(0x1acc,0x600); put(0x1ace,0x8000); put(0x600,0x8000); put(0x630,2);
    put(0x608,120); put(0x60c,110); clear();
    issd_readability_render(fb,446,224,95,ram,&cfg); assert(!changed());
    put(0x1ace,0); clear(); issd_readability_render(fb,446,224,95,ram,&cfg); assert(changed()>0);
    put(0x608,0x8000); clear(); issd_readability_render(fb,446,224,95,ram,&cfg); assert(!changed());
    cfg.player_markers=false; cfg.radar_scale=2; put(0x12a2,1536); put(0x12a4,512);
    put(0x62a,768); put(0x62c,256); clear();
    issd_readability_render(fb,446,224,95,ram,&cfg); assert(changed()>0);
    assert(fb[(224-112-8)*446+(446-160)/2]==0xffa0c8b0);
    /* Radar retains a real player whose native screen pose was culled. */
    put(0x600,0); put(0x69a,0xd00); put(0x668,1); clear();
    issd_readability_render(fb,446,224,95,ram,&cfg);
    assert(fb[((224-112-8)+2+256*(112-5)/512)*446+
              (446-160)/2+2+768*(160-5)/1536]==0xff60c8ff);
    put(0x600,0x8000);
    cfg.radar_scale=1; cfg.player_names=true; put(0x608,120); put(0x69a,0xd00); put(0x668,1);
    ram[0x3f91]=3; memcpy(ram+0xd478+3*8,"\x68\x69\x6a\0\0\0\0\0",8);
    char name[9]; assert(issd_readability_player_name(ram,0x600,name)); assert(!strcmp(name,"ABC"));
    ram[0x3f91]=0x20; assert(!issd_readability_player_name(ram,0x600,name));
    /* A large requested map must fit a small surface without corrupting guards
     * or the original scoreboard. */
    cfg.player_names=false; cfg.radar_scale=3;
    uint32_t small[120*100+2];
    for (unsigned i=0;i<120*100+2;i++) small[i]=0xff226622;
    memcpy(saved,ram,sizeof ram);
    issd_readability_render(small+1,120,100,0,ram,&cfg);
    unsigned small_changed=0;
    for (unsigned i=1;i<=120*100;i++) small_changed+=small[i]!=0xff226622;
    assert(small_changed>0);
    assert(small[0]==0xff226622 && small[120*100+1]==0xff226622);
    for (unsigned i=1;i<=120*24;i++) assert(small[i]==0xff226622);
    assert(!memcmp(saved,ram,sizeof ram));
    /* 1x preserves the native map even with non-default placement/opacity. */
    cfg.radar_scale=1; cfg.radar_position=3; cfg.radar_opacity=25;
    clear(); issd_readability_render(fb,446,224,95,ram,&cfg); assert(!changed());
    cfg.radar_scale=2;
    clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    assert(fb[32*446+8]==0xffa0c8b0);
    assert(fb[33*446+9]==0xff1f5c26);
    cfg.radar_opacity=75; clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    assert(fb[33*446+9]==0xff19482d);
    cfg.radar_position=4; clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    assert(fb[32*446+(446-160-8)]==0xffa0c8b0);
    assert(fb[32*446+8]==0xff226622);
    cfg.radar_scale=1; cfg.player_markers=true; cfg.player_names=true;
    ram[0x3f91]=3;
    cfg.hud_scale=1; clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    unsigned normal=changed();
    cfg.hud_scale=3; clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    assert(changed()>normal*5);
    /* Co-located controllers get distinct complete name blocks. Opaque glyphs
     * let this count catch a second label overwriting the first. */
    put(0x1afc,0x700); put(0x700,0x8000); put(0x730,2);
    put(0x708,120); put(0x70c,110); put(0x79a,0xd00); put(0x768,1);
    cfg.player_markers=false; cfg.hud_scale=2;
    clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    unsigned first=0,second=0;
    for (unsigned i=0;i<446*224;i++) {
        first+=fb[i]==0xff60c8ff; second+=fb[i]==0xffffa050;
    }
    assert(first==3*16*16 && second==first);
    put(0x60c,24); put(0x70c,24); cfg.hud_scale=3; cfg.player_markers=true;
    clear(); memcpy(saved,ram,sizeof ram);
    issd_readability_render(fb,446,224,95,ram,&cfg);
    for (unsigned i=0;i<446*24;i++) assert(fb[i]==0xff226622);
    assert(changed()>0 && !memcmp(saved,ram,sizeof ram));
    /* Labels must also avoid the cartridge's unchanged bottom-center radar. */
    cfg.hud_scale=1; cfg.player_markers=false;
    put(0x60c,190); put(0x70c,190);
    clear(); issd_readability_render(fb,446,224,95,ram,&cfg);
    for (int y=160;y<216;y++) for (int x=183;x<263;x++)
        assert(fb[y*446+x]==0xff226622);
    /* Every position/scale respects guard pixels even on tiny surfaces. */
    put(0x608,20); put(0x60c,30); put(0x708,20); put(0x70c,30);
    memcpy(saved,ram,sizeof ram);
    for (int w=1;w<=120;w+=17) for (int h=1;h<=100;h+=13)
        for (int position=0;position<5;position++) for (int scale=1;scale<=3;scale++) {
            uint32_t *guarded=malloc(((size_t)w*h+2)*sizeof *guarded);
            assert(guarded);
            for (int i=0;i<w*h+2;i++) guarded[i]=0xff226622;
            cfg.radar_scale=scale; cfg.radar_position=position;
            cfg.player_markers=true; cfg.player_names=true; cfg.hud_scale=scale;
            issd_readability_render(guarded+1,w,h,0,ram,&cfg);
            assert(guarded[0]==0xff226622 && guarded[w*h+1]==0xff226622);
            for (int i=0;i<w*(h<24 ? h : 24);i++) assert(guarded[i+1]==0xff226622);
            free(guarded);
        }
    assert(!memcmp(saved,ram,sizeof ram));
    issd_readability_render(NULL,0,0,0,NULL,NULL);
    return 0;
}
