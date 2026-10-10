#include "issd_readability.h"
#include "issd_camera.h"
#include <string.h>

extern const uint8_t g_issd_font8x8[96][8];
static unsigned word(const uint8_t *r, unsigned a) { return r[a] | (unsigned)r[a+1] << 8; }
static const uint32_t colors[4]={0xff60c8ff,0xffffa050,0xffdc88ff,0xff60eea0};
static void pixel(uint32_t *fb,int w,int h,int x,int y,uint32_t c) {
    if (x>=0 && x<w && y>=24 && y<h) fb[(size_t)y*w+x]=c;
}
static void text(uint32_t *fb,int w,int h,int x,int y,const char *s,uint32_t c,int scale) {
    for (; *s; ++s,x+=8*scale) {
        unsigned ch=(unsigned char)*s;
        if (ch<32 || ch>126) continue;
        for (int py=0;py<8;py++) for (int px=0;px<8;px++)
            if (g_issd_font8x8[ch-32][py] & (0x80>>px)) {
                for (int sy=0;sy<scale;sy++) for (int sx=0;sx<scale;sx++) {
                    pixel(fb,w,h,x+px*scale+sx+scale,y+py*scale+sy+scale,0xff101820);
                    pixel(fb,w,h,x+px*scale+sx,y+py*scale+sy,c);
                }
            }
    }
}
static bool actor_valid(const uint8_t *r,unsigned a) {
    return a>=0x500 && a<=0x1a00 && !(a&255) && word(r,a) && word(r,a+0x30);
}
bool issd_readability_player_name(const uint8_t *r,unsigned a,char name[9]) {
    if (!name) return false;
    name[0]=0;
    if (!r || !actor_valid(r,a)) return false;
    unsigned team=word(r,a+0x9a), slot=word(r,a+0x68);
    if ((team!=0xd00 && team!=0xe00) || slot>10) return false;
    unsigned entry=r[(team==0xd00 ? 0x3f90 : 0x3fa4)+slot];
    unsigned roster=entry&31;
    if ((entry&0x20) || roster>=20) return false;
    unsigned base=(team==0xd00 ? 0xd478 : 0xd518)+roster*8;
    unsigned len=0;
    for (unsigned i=0;i<8;i++) {
        unsigned code=r[base+i]; char ch=' ';
        if (code>=0x68 && code<=0x81) ch=(char)('A'+code-0x68);
        else if (code>=0x82 && code<=0x9b) ch=(char)('a'+code-0x82);
        else if (code==0x54) ch='.';
        name[i]=ch;
        if (ch!=' ') len=i+1;
    }
    name[len]=0;
    return len!=0;
}
static bool on_pitch(int x,int y,int w,int h) {
    /* Avoid drawing labels for objects entirely outside the visible field. */
    return x>=0 && x<w && y>=24 && y<h-8;
}
typedef struct { int x,y,w,h; } Rect;
static void actor_screen(const uint8_t *r,unsigned actor,int w,int h,int margin,
                         const IssdConfig *cfg,int *x,int *y,int *height) {
    *x=(int16_t)word(r,actor+8);*y=(int16_t)word(r,actor+12);
    *height=(int16_t)word(r,actor+16);
    if(h==224 && (cfg->camera_mode==ISSD_CAMERA_TACTICAL || cfg->camera_mode==ISSD_CAMERA_TACTICAL_WIDE)) {
        IssdCameraView view=issd_camera_view(w,h,cfg->camera_mode);
        *x=issd_camera_project(*x,view.x,w,view.w);
        *y=issd_camera_project(*y,view.y,h,view.h);
        *height=issd_camera_project(*height,0,h,view.h);
    } else *x+=margin;
}
static bool overlaps(Rect a,Rect b) {
    return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
static Rect radar_rect(int w,int h,int scale,int position) {
    Rect b={0,0,0,0};
    if (scale<2 || w<12 || h<40) return b;
    if (scale>3) scale=3;
    b.w=80*scale; b.h=56*scale;
    /* Fit proportionally; a 3x preference must remain usable on native and
     * smaller surfaces. Keep eight pixels clear around the map. */
    if (b.w>w-16) { b.h=b.h*(w-16)/b.w; b.w=w-16; }
    if (b.h>h-40) { b.w=b.w*(h-40)/b.h; b.h=h-40; }
    if (b.w<6 || b.h<6) { b.w=b.h=0; return b; }
    b.x=(w-b.w)/2; b.y=h-b.h-8;
    if (position==1 || position==3) b.x=8;
    if (position==2 || position==4) b.x=w-b.w-8;
    if (position==3 || position==4) b.y=32;
    return b;
}
static uint32_t blend(uint32_t old,uint32_t color,int opacity) {
    uint32_t result=old&0xff000000;
    for (int shift=0;shift<24;shift+=8)
        result|=((((old>>shift)&255)*(100-opacity)+((color>>shift)&255)*opacity+50)/100)<<shift;
    return result;
}
static void radar(uint32_t *fb,int w,int h,const uint8_t *r,const IssdConfig *cfg) {
    int length=(int)word(r,0x12a2), width=(int)word(r,0x12a4);
    if (length<1024 || length>4096 || width<256 || width>1024) return;
    Rect b=radar_rect(w,h,cfg->radar_scale,cfg->radar_position);
    if (!b.w) return;
    int rw=b.w,rh=b.h,x0=b.x,y0=b.y;
    int scale=cfg->radar_scale>3 ? 3 : cfg->radar_scale;
    int opacity=cfg->radar_opacity;
    if (opacity<25 || opacity>100) opacity=75;
    for (int y=0;y<rh;y++) for (int x=0;x<rw;x++)
        pixel(fb,w,h,x0+x,y0+y,(x==0 || y==0 || x==rw-1 || y==rh-1 || x==rw/2) ? 0xffa0c8b0 :
              blend(fb[(size_t)(y0+y)*w+x0+x],0xff163e30,opacity));
    for (unsigned a=0x400;a<=0x1a00;a+=0x100) {
        if (a==0x400) {
            if (!word(r,a)) continue;
        } else {
            /* Radar is world-space: a culled screen pose must not remove a
             * real roster player from the map. Qualify the active field record. */
            unsigned team=word(r,a+0x9a);
            if (!word(r,a+0x30) || (team!=0xd00 && team!=0xe00) || word(r,a+0x68)>10) continue;
        }
        unsigned wx=word(r,a+0x2a),wy=word(r,a+0x2c);
        if (wx>(unsigned)length || wy>(unsigned)width) continue;
        int x=x0+2+(int)wx*(rw-5)/length,y=y0+2+(int)wy*(rh-5)/width;
        uint32_t c=a==0x400 ? 0xffffffff : (word(r,a+0x9a)==0xd00 ? colors[0] : colors[1]);
        for (int yy=0;yy<scale;yy++) for (int xx=0;xx<scale;xx++) pixel(fb,w,h,x+xx,y+yy,c);
    }
}
void issd_readability_render(uint32_t *fb,int w,int h,int margin,const uint8_t *r,const IssdConfig *cfg) {
    if (!fb || !r || !cfg || w<=0 || h<=0 || word(r,0x32)!=6 || word(r,0x70)!=8) return;
    if (word(r,0x400)) {
        int x,y,height;actor_screen(r,0x400,w,h,margin,cfg,&x,&y,&height);
        int elevated=y+height;
        if (cfg->ball_shadow && (elevated < y-4 || elevated > y+4) && on_pitch(x,y,w,h))
            for (int dy=-1;dy<=1;dy++) for (int dx=-4;dx<=4;dx++)
                if (dx*dx+dy*dy*8<=16) pixel(fb,w,h,x+dx,y+dy,0xff182a20);

    }
    struct Selected { unsigned actor,player; int x,y; Rect marker; } selected[4];
    unsigned human=0,count=0;
    int scale=cfg->hud_scale;
    if (scale<1) scale=1;
    if (scale>3) scale=3;
    Rect occupied[10]; unsigned used=0;
    Rect map=radar_rect(w,h,cfg->radar_scale,cfg->radar_position);
    if (map.w && word(r,0x12a2)>=1024 && word(r,0x12a2)<=4096 &&
        word(r,0x12a4)>=256 && word(r,0x12a4)<=1024) occupied[used++]=map;
    if (word(r,0x12a2)>=1024 && word(r,0x12a2)<=4096 &&
        word(r,0x12a4)>=256 && word(r,0x12a4)<=1024)
        occupied[used++]=(Rect){(w-80)/2,h-64,80,56};
    for (unsigned control=0x1aa0;control<=0x1b60 && human<4;control+=0x30) {
        unsigned a=word(r,control+0x2c);
        if (!a || (word(r,control+0x2e)&0x8000)) continue;
        unsigned player=human++;
        if (!actor_valid(r,a)) continue;
        int x,y,height;actor_screen(r,a,w,h,margin,cfg,&x,&y,&height);
        if (!on_pitch(x,y,w,h)) continue;
        selected[count].actor=a; selected[count].player=player;
        selected[count].x=x; selected[count].y=y;
        selected[count].marker=(Rect){x-3*scale,y+4*scale,7*scale,4*scale};
        count++;
    }
    radar(fb,w,h,r,cfg);
    /* Reserve all markers before placing text, including later controllers.
     * Crowded selections move a marker a few pixels or omit it if no room. */
    if (cfg->player_markers) for (unsigned i=0;i<count;i++) {
        Rect marker=selected[i].marker; bool placed=false;
        for (int attempt=0;attempt<5 && !placed;attempt++) {
            Rect candidate=marker;
            candidate.x+=(attempt%2 ? 1 : -1)*((attempt+1)/2)*8*scale;
            if (candidate.x<0 || candidate.x+candidate.w>w || candidate.y<24 || candidate.y+candidate.h>h) continue;
            bool blocked=false;
            for (unsigned j=0;j<used;j++) if (overlaps(candidate,occupied[j])) blocked=true;
            if (blocked) continue;
            occupied[used++]=candidate; placed=true;
            for (int dy=0;dy<4*scale;dy++) for (int dx=-(dy/scale)*scale;dx<=(dy/scale)*scale;dx++)
                pixel(fb,w,h,candidate.x+3*scale+dx,candidate.y+dy,colors[selected[i].player]);
        }
    }
    for (unsigned i=0;i<count;i++) {
        char name[9]={0},label[3]={'P',(char)('1'+selected[i].player),0};
        if (cfg->player_names) issd_readability_player_name(r,selected[i].actor,name);
        int lines=(cfg->player_markers ? 1 : 0)+(name[0] ? 1 : 0);
        if (!lines) continue;
        int chars=(int)strlen(name);
        if (cfg->player_markers && chars<2) chars=2;
        Rect box={selected[i].x-chars*4*scale,selected[i].y-(cfg->player_markers ? 28 : 18)*scale,
                  chars*8*scale+scale,lines*10*scale-scale};
        if (box.w>w || box.h>h-24) continue;
        if (box.x<0) box.x=0;
        if (box.x+box.w>w) box.x=w-box.w;
        bool placed=false;
        /* First try the player's nearby positions, then find a free row.
         * Suppress text if the frame cannot fit it without an overlap. */
        for (int attempt=0;attempt<h+2 && !placed;attempt++) {
            Rect candidate=box;
            if (attempt==1) candidate.y=selected[i].y+10*scale;
            else if (attempt>1) candidate.y=24+attempt-2;
            if (candidate.y<24 || candidate.y+candidate.h>h) continue;
            bool blocked=false;
            for (unsigned j=0;j<used;j++) if (overlaps(candidate,occupied[j])) blocked=true;
            if (blocked) continue;
            occupied[used++]=candidate; placed=true;
            int ty=candidate.y;
            if (cfg->player_markers) {
                text(fb,w,h,candidate.x+(chars-2)*4*scale,ty,label,colors[selected[i].player],scale);
                ty+=10*scale;
            }
            if (name[0]) text(fb,w,h,candidate.x+(chars-(int)strlen(name))*4*scale,ty,name,colors[selected[i].player],scale);
        }
    }
}
