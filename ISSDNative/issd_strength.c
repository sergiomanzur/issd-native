#include "issd_strength.h"
#include <stdio.h>
#include <string.h>
static int rating(unsigned n){if(n<2)n=2;if(n>9)n=9;return (int)((n-2)*99+3)/7;}
bool issd_strength_calculate(const uint8_t*r,size_t size,int team,IssdTeamStrength*out){
    if(!r||!out||team<0||team>=42||size<0x60000)return false;
    size_t ptr=0x5ef48+(unsigned)team*2;
    unsigned address=r[ptr]|(unsigned)r[ptr+1]<<8;
    if(address<0x8000)return false;
    size_t shape=0x58000+address-0x8000;
    if(shape+31>size)return false;
    int totals[3]={0},counts[3]={0};
    for(unsigned p=0;p<11;p++){
        const uint8_t*a=r+0x50000+((unsigned)team*20+p)*7;
        int speed=rating(a[0]&15),shoot=rating(a[1]>>4),tech=rating(a[1]&15);
        int balance=rating(a[2]>>4),intel=rating(a[2]&15),dribble=rating(a[3]>>4);
        int jump=rating(a[3]&15),stamina=rating(a[4]&15);
        if(!p){totals[0]+=(speed+tech+intel+jump+2)/4;counts[0]++;continue;}
        unsigned role=r[shape+20+p];int group;
        if(role==1||role==5)group=0;else if(role==2||role==6)group=1;else if(role==3)group=2;else return false;
        int value=group==0?(speed+balance+intel+jump+2)/4:
                  group==1?(tech+intel+dribble+stamina+2)/4:(shoot+speed+tech+dribble+2)/4;
        totals[group]+=value;counts[group]++;
    }
    if(!counts[0]||!counts[1]||!counts[2])return false;
    out->defence=(totals[0]+counts[0]/2)/counts[0];
    out->midfield=(totals[1]+counts[1]/2)/counts[1];
    out->attack=(totals[2]+counts[2]/2)/counts[2];
    out->overall=(out->defence+out->midfield+out->attack+1)/3;
    out->stars=1+(out->overall>=40)+(out->overall>=65)+(out->overall>=85);
    return true;
}
static void text(uint32_t*f,int w,int h,int x,int y,const char*s,uint32_t color,const uint8_t font[96][8]){
    for(;*s;s++,x+=8){unsigned ch=(unsigned char)*s;if(ch<32||ch>127)continue;
        for(int row=0;row<8;row++)for(int col=0;col<8;col++)
            if(x+col>=0&&x+col<w&&y+row>=0&&y+row<h&&(font[ch-32][row]&(0x80>>col)))f[(y+row)*w+x+col]=color;}
}
void issd_strength_draw(uint32_t*f,int w,int h,int margin,const uint8_t*ram,const uint8_t*r,size_t size,const uint8_t font[96][8]){
    static const uint8_t scroll[]={20,36,16,32};
    if(!f||!ram||!font||ram[0x32]!=6||ram[0x70]!=12)return;
    for(int i=0;i<4;i++)if(ram[0x18+i*2]!=scroll[i])return;
    IssdTeamStrength s;if(!issd_strength_calculate(r,size,ram[0x1526]/2,&s))return;
    for(int y=198;y<220&&y<h;y++)for(int x=8;x<248;x++)if(x+margin>=0&&x+margin<w)f[y*w+x+margin]=0xff101e30;
    char line[32];snprintf(line,sizeof line,"ATT %02d  MID %02d  DEF %02d",s.attack,s.midfield,s.defence);
    text(f,w,h,margin+20,200,line,0xfff3e8cc,font);
    snprintf(line,sizeof line,"OVR %02d     %.*s",s.overall,s.stars,"****");
    text(f,w,h,margin+20,211,line,0xffffbc54,font);
}
