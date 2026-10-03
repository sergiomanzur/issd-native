#include "issd_visual.h"
#include <assert.h>
int main(void) {
    for(unsigned c=0;c<256;c++) {
        uint32_t gray=0x7f000000u|(c<<16)|(c<<8)|c;
        assert(issd_visual_color_boost(gray)==gray);
        assert(issd_visual_crt_pixel(gray,0)==gray);
        assert(issd_visual_crt_pixel(gray,100)==(0x7f000000u|((c*3/4)<<16)|((c*3/4)<<8)|(c*3/4)));
        unsigned previous=c;
        for(int strength=0;strength<=100;strength+=25) {
            unsigned channel=issd_visual_crt_pixel(gray,strength)&255;
            assert(channel<=previous); previous=channel;
        }
    }
    assert(issd_visual_color_boost(0xff207438u)==0xff1a7835u);
    assert(issd_visual_color_boost(0x12345678u)>>24==0x12);
    assert(issd_visual_color_boost(0xff00ff00u)==0xff00ff00u);
    uint32_t guard[]={0x12345678u,0xff207438u,0xff808080u,0x87654321u};
    issd_visual_boost_frame(guard+1,2);
    assert(guard[0]==0x12345678u && guard[3]==0x87654321u);
    assert(guard[1]==0xff1a7835u && guard[2]==0xff808080u);
    issd_visual_boost_frame(NULL,0);
    return 0;
}
