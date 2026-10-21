#include <assert.h>
#include <stdio.h>
#include "issd_video.h"

static void expect_rect(IssdVideoRect r, int x, int y, int w, int h) {
    if (r.x != x || r.y != y || r.w != w || r.h != h)
        fprintf(stderr, "got %d,%d %dx%d; expected %d,%d %dx%d\n", r.x,r.y,r.w,r.h,x,y,w,h);
    assert(r.x == x && r.y == y && r.w == w && r.h == h);
}

int main(void) {
    expect_rect(issd_video_viewport(1280,720,256,224,ISSD_ASPECT_4_3,false,false),160,0,960,720);
    expect_rect(issd_video_viewport(1280,720,256,224,ISSD_ASPECT_4_3,false,true),192,24,896,672);
    expect_rect(issd_video_viewport(1280,720,256,224,ISSD_ASPECT_INTEGER,false,false),256,24,768,672);
    expect_rect(issd_video_viewport(1920,1080,320,224,ISSD_ASPECT_AUTHENTIC,true,false),189,0,1542,1080);
    expect_rect(issd_video_viewport(1280,720,398,224,ISSD_ASPECT_16_9,true,false),0,0,1279,720);
    expect_rect(issd_video_viewport(320,180,256,224,ISSD_ASPECT_INTEGER,false,true),57,0,205,180);
    expect_rect(issd_video_viewport(0,720,256,224,ISSD_ASPECT_4_3,false,true),0,0,0,0);
    for (int w=1; w<1600; w+=37) for (int h=1; h<1000; h+=53) {
        IssdVideoRect r=issd_video_viewport(w,h,320,224,ISSD_ASPECT_AUTHENTIC,true,true);
        assert(r.x>=0 && r.y>=0 && r.w>0 && r.h>0 && r.x+r.w<=w && r.y+r.h<=h);
    }
    /* Fit: the widest tested view at the largest whole-number scale. */
    assert(issd_video_fit_extra(1920,1080)==112);  /* 480 x4 */
    assert(issd_video_fit_extra(2560,1440)==85);   /* 426 x6 */
    assert(issd_video_fit_extra(3840,2160)==85);   /* 426 x9 */
    assert(issd_video_fit_extra(1280,720)==85);    /* 426 x3 */
    assert(issd_video_fit_extra(1280,800)==85);    /* 426 x3, Steam Deck */
    assert(issd_video_fit_extra(3440,1440)==112);  /* 480 x6 */
    assert(issd_video_fit_extra(800,600)==71);     /* 398 x2 */
    assert(issd_video_fit_extra(0,0)==71);
    expect_rect(issd_video_viewport(1920,1080,480,224,ISSD_ASPECT_FIT,true,false),0,92,1920,896);
    expect_rect(issd_video_viewport(2560,1440,426,224,ISSD_ASPECT_FIT,true,false),2,48,2556,1344);
    expect_rect(issd_video_viewport(1280,800,426,224,ISSD_ASPECT_FIT,true,false),1,64,1278,672);
    for (int w=1; w<4000; w+=41) for (int h=1; h<2200; h+=59) {
        int render_w=256+2*issd_video_fit_extra(w,h);
        IssdVideoRect r=issd_video_viewport(w,h,render_w,224,ISSD_ASPECT_FIT,true,false);
        assert(r.x>=0 && r.y>=0 && r.w>0 && r.h>0 && r.x+r.w<=w && r.y+r.h<=h);
        if (h>=224 && w>=render_w) assert(r.h%224==0 && r.w==render_w*(r.h/224));
    }
    int w,h;
    issd_video_output_dimensions(0,1280,800,&w,&h); assert(w==1280 && h==800);
    issd_video_output_dimensions(2,1280,800,&w,&h); assert(w==1920 && h==1080);
    issd_video_output_dimensions(4,1280,800,&w,&h); assert(w==3840 && h==2160);
    assert(issd_video_sharp_prescale(256,224,960,720)==4);
    assert(issd_video_sharp_prescale(256,224,160,140)==1);
    assert(issd_video_sharp_prescale(320,224,3840,2160)==8);
    assert(issd_video_sharp_prescale(2048,1792,3840,2160)==2);
    assert(issd_video_sharp_prescale(3000,2000,16000,9000)==1);
    assert(issd_video_internal_scale(ISSD_RES_8X_4K)==8);
    assert(issd_video_internal_scale(ISSD_RES_3X)==3);
    assert(issd_video_overlay_scale(0,1280,720)==2);
    assert(issd_video_overlay_scale(0,3840,2160)==4);
    assert(issd_video_overlay_scale(3,1280,720)==3);
    expect_rect(issd_video_overlay_safe_rect(1280,720,2),16,16,1248,688);
    expect_rect(issd_video_overlay_safe_rect(4,3,4),1,1,2,1);
    puts("video geometry tests passed");
    return 0;
}
