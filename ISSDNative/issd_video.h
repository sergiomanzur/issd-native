#ifndef ISSD_VIDEO_H
#define ISSD_VIDEO_H

#include "issd_config.h"

/* Geometry is expressed in physical output pixels; no SDL dependency. */
typedef struct { int x, y, w, h; } IssdVideoRect;

IssdVideoRect issd_video_viewport(int output_w, int output_h, int render_w, int render_h,
                                IssdAspectRatio aspect, bool true_widescreen, bool integer_scaling);
void issd_video_output_dimensions(int preset, int current_w, int current_h, int *w, int *h);
/* Nearest-neighbor prescale before final linear presentation. Bounded to 8x and
 * 4096 per texture axis, avoiding an arbitrary full-output CPU scaling pass. */
int issd_video_sharp_prescale(int render_w, int render_h, int viewport_w, int viewport_h);
int issd_video_internal_scale(IssdInternalResolution resolution);
const char *issd_video_internal_label(IssdInternalResolution resolution);
int issd_video_overlay_scale(int setting, int output_w, int output_h);
IssdVideoRect issd_video_overlay_safe_rect(int output_w, int output_h, int scale);

#endif
