#include "issd_video.h"
#include <stdint.h>

static int min_int(int a, int b) { return a < b ? a : b; }
static int clamp_int(int n, int low, int high) { return n < low ? low : n > high ? high : n; }

IssdVideoRect issd_video_viewport(int output_w, int output_h, int render_w, int render_h,
                                IssdAspectRatio aspect, bool true_widescreen, bool integer_scaling) {
    IssdVideoRect r = {0,0,0,0};
    if (output_w <= 0 || output_h <= 0 || render_w <= 0 || render_h <= 0) return r;
    int num = 4, den = 3;
    switch (aspect) {
        case ISSD_ASPECT_8_7: num = 8; den = 7; break;
        case ISSD_ASPECT_16_9: num = 16; den = 9; break;
        case ISSD_ASPECT_16_10: num = 16; den = 10; break;
        case ISSD_ASPECT_21_9: num = 21; den = 9; break;
        case ISSD_ASPECT_AUTHENTIC: num = 10; den = 7; break;
        case ISSD_ASPECT_INTEGER:
            num = render_w; den = render_h; integer_scaling = true; break;
        default: break;
    }
    if (true_widescreen && render_w > 256) { num = render_w; den = render_h; }
    if ((int64_t)output_w * den > (int64_t)output_h * num) {
        r.h = output_h;
        r.w = (int)((int64_t)r.h * num / den);
    } else {
        r.w = output_w;
        r.h = (int)((int64_t)r.w * den / num);
    }
    if (integer_scaling) {
        int factor = r.h / render_h;
        if (factor > 0) {
            r.h = render_h * factor;
            r.w = (int)((int64_t)r.h * num / den);
        }
        /* Smaller outputs fall back to fractional fitting instead of cropping. */
    }
    r.w = clamp_int(r.w, 1, output_w);
    r.h = clamp_int(r.h, 1, output_h);
    r.x = (output_w - r.w) / 2;
    r.y = (output_h - r.h) / 2;
    return r;
}

void issd_video_output_dimensions(int preset, int current_w, int current_h, int *w, int *h) {
    static const int sizes[][2] = {{0,0},{1280,720},{1920,1080},{2560,1440},{3840,2160}};
    if (preset < 1 || preset > 4) {
        if (w) *w = current_w;
        if (h) *h = current_h;
    } else {
        if (w) *w = sizes[preset][0];
        if (h) *h = sizes[preset][1];
    }
}

int issd_video_sharp_prescale(int render_w, int render_h, int viewport_w, int viewport_h) {
    if (render_w <= 0 || render_h <= 0 || viewport_w <= 0 || viewport_h <= 0) return 1;
    int64_t sx = ((int64_t)viewport_w + render_w - 1) / render_w;
    int64_t sy = ((int64_t)viewport_h + render_h - 1) / render_h;
    int wanted = (int)(sx > sy ? sx : sy);
    int limit = min_int(8, min_int(4096 / render_w, 4096 / render_h));
    if (limit < 1) limit = 1;
    return clamp_int(wanted, 1, limit);
}

int issd_video_internal_scale(IssdInternalResolution resolution) {
    static const int scales[] = {1,2,3,4,6,8};
    return resolution >= ISSD_RES_1X && resolution <= ISSD_RES_8X_4K ? scales[resolution] : 1;
}

const char *issd_video_internal_label(IssdInternalResolution resolution) {
    static const char *labels[] = {"1x (256 x 224)","2x (512 x 448)","3x (768 x 672)",
                                   "4x (1024 x 896)","6x (1536 x 1344)","8x (2048 x 1792)"};
    return resolution >= ISSD_RES_1X && resolution <= ISSD_RES_8X_4K ? labels[resolution] : labels[0];
}

int issd_video_overlay_scale(int setting, int output_w, int output_h) {
    if (setting > 0) return clamp_int(setting, 1, 4);
    return clamp_int(min_int(output_w / 640, output_h / 360), 1, 4);
}

IssdVideoRect issd_video_overlay_safe_rect(int output_w, int output_h, int scale) {
    IssdVideoRect r = {0,0,0,0};
    if (output_w <= 0 || output_h <= 0) return r;
    int margin = 8 * clamp_int(scale, 1, 4);
    r.x = min_int(margin, (output_w - 1) / 2);
    r.y = min_int(margin, (output_h - 1) / 2);
    r.w = output_w - 2 * r.x;
    r.h = output_h - 2 * r.y;
    return r;
}
