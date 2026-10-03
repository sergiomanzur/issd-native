#ifndef ISSD_VISUAL_H
#define ISSD_VISUAL_H
#include <stdint.h>
#include <stddef.h>

/* Pure framebuffer cosmetics shared by desktop, Android and preview. */
static inline uint32_t issd_visual_color_boost(uint32_t color) {
    int r=(color>>16)&255, g=(color>>8)&255, b=color&255;
    int gray=(77*r+150*g+29*b+128)>>8;
    int channels[3]={r+(r-gray)/8,g+(g-gray)/8,b+(b-gray)/8};
    uint32_t out=color&0xff000000u;
    for(int i=0;i<3;i++) {
        int v=channels[i];
        if(v<0) v=0;
        if(v>255) v=255;
        out|=(uint32_t)v<<(16-i*8);
    }
    return out;
}
static inline void issd_visual_boost_frame(uint32_t *pixels, size_t count) {
    if(!pixels) return;
    for(size_t i=0;i<count;i++) pixels[i]=issd_visual_color_boost(pixels[i]);
}
static inline uint32_t issd_visual_crt_pixel(uint32_t color,int strength) {
    if(strength<0) strength=0;
    if(strength>100) strength=100;
    uint32_t out=color&0xff000000u;
    for(int shift=0;shift<24;shift+=8)
        out|=((((color>>shift)&255)*(400-strength))/400)<<shift;
    return out;
}
#endif
