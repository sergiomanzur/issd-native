#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Host-presentation choices for a 256x224 SNES frame. These do not alter the
 * emulated PPU; they only define the horizontal pixel aspect used at present. */
typedef enum SnesDisplayAspect {
  kSnesDisplayAspect_Crt4x3 = 0,
  kSnesDisplayAspect_SquarePixels8x7 = 1,
  kSnesDisplayAspect_SquareFrame1x1 = 2,
  kSnesDisplayAspect_Count,
} SnesDisplayAspect;

typedef struct SnesDisplayViewport {
  int x;
  int y;
  int width;
  int height;
} SnesDisplayViewport;

static inline SnesDisplayAspect SnesDisplayAspect_Clamp(int value) {
  return value >= 0 && value < kSnesDisplayAspect_Count
      ? (SnesDisplayAspect)value : kSnesDisplayAspect_Crt4x3;
}
/* Horizontal:vertical pixel aspect. A 256x224 frame therefore presents as
 * 4:3, 8:7, or 1:1 respectively. */
static inline void SnesDisplayAspect_GetPixelAspect(
    SnesDisplayAspect aspect, int *numerator, int *denominator) {
  static const uint8_t kNumerators[kSnesDisplayAspect_Count] = {7, 1, 7};
  static const uint8_t kDenominators[kSnesDisplayAspect_Count] = {6, 1, 8};
  aspect = SnesDisplayAspect_Clamp((int)aspect);
  if (numerator) *numerator = kNumerators[aspect];
  if (denominator) *denominator = kDenominators[aspect];
}

/* Widescreen is a horizontal 4/3 expansion of the authentic 256-pixel field.
 * Keeping the selected pixel aspect produces 16:9 from 4:3, 32:21 from 8:7,
 * and 4:3 from a 1:1 frame. The PPU requires a centered even width. */
static inline int SnesDisplayAspect_ComputeWideFrameWidth(int native_width) {
  if (native_width <= 0) native_width = 256;
  int64_t width = ((int64_t)native_width * 4 + 1) / 3;
  if (width & 1) width++;
  return width > INT32_MAX ? INT32_MAX - 1 : (int)width;
}

static inline void SnesDisplayAspect_ComputePresentationSize(
    int frame_width, int frame_height, SnesDisplayAspect aspect,
    int *width, int *height) {
  if (!width || !height) return;
  if (frame_width <= 0) frame_width = 256;
  if (frame_height <= 0) frame_height = 224;
  int par_num, par_den;
  SnesDisplayAspect_GetPixelAspect(aspect, &par_num, &par_den);
  *width = (int)(((int64_t)frame_width * par_num + par_den / 2) / par_den);
  *height = frame_height;
}

static inline int SnesDisplayAspect_ComputeWindowWidth(
    int frame_width, int frame_height, int window_height,
    SnesDisplayAspect aspect) {
  if (frame_width <= 0) frame_width = 256;
  if (frame_height <= 0) frame_height = 224;
  if (window_height <= 0) window_height = frame_height;
  int par_num, par_den;
  SnesDisplayAspect_GetPixelAspect(aspect, &par_num, &par_den);
  int64_t numerator = (int64_t)frame_width * par_num * window_height;
  int64_t denominator = (int64_t)par_den * frame_height;
  return (int)((numerator + denominator / 2) / denominator);
}

static inline void SnesDisplayAspect_ComputeViewport(
    int source_width, int source_height, int drawable_width,
    int drawable_height, SnesDisplayAspect aspect, bool ignore_aspect,
    bool integer_scale, SnesDisplayViewport *viewport) {
  if (!viewport) return;
  viewport->x = viewport->y = 0;
  viewport->width = drawable_width > 0 ? drawable_width : 1;
  viewport->height = drawable_height > 0 ? drawable_height : 1;
  if (ignore_aspect || source_width <= 0 || source_height <= 0 ||
      drawable_width <= 0 || drawable_height <= 0)
    return;

  int par_num, par_den;
  SnesDisplayAspect_GetPixelAspect(aspect, &par_num, &par_den);
  double source_display_width =
      (double)source_width * (double)par_num / (double)par_den;
  double scale_x = drawable_width / source_display_width;
  double scale_y = (double)drawable_height / source_height;
  double scale = scale_x < scale_y ? scale_x : scale_y;
  if (integer_scale && scale >= 1.0)
    scale = (double)(int)scale;
  if (scale <= 0.0) scale = scale_x < scale_y ? scale_x : scale_y;

  viewport->width = (int)(source_display_width * scale + 0.5);
  viewport->height = (int)(source_height * scale + 0.5);

  /* Even PPU widths cannot represent every target ratio exactly. Suppress a
   * sub-source-pixel seam while retaining real pillar/letterboxing. */
  if (!integer_scale) {
    int width_gap = drawable_width - viewport->width;
    int height_gap = drawable_height - viewport->height;
    int native_x = (drawable_width + source_width - 1) / source_width;
    int native_y = (drawable_height + source_height - 1) / source_height;
    if (width_gap > 0 && width_gap <= native_x)
      viewport->width = drawable_width;
    if (height_gap > 0 && height_gap <= native_y)
      viewport->height = drawable_height;
  }
  viewport->x = (drawable_width - viewport->width) / 2;
  viewport->y = (drawable_height - viewport->height) / 2;
}
