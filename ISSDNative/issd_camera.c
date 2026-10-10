#include "issd_camera.h"
#include <stdint.h>

static int floor_div(int64_t n,int d) {
  return (int)(n>=0 ? n/d : -((-n+d-1)/d));
}
IssdCameraView issd_camera_view(int width,int height,IssdCameraMode mode) {
  IssdCameraView v={0,0,0,0};
  if(width<=0 || height<=0 || width>4096 || height>4096) return v;
  int num=1,den=1;
  if(mode==ISSD_CAMERA_TACTICAL) {num=4;den=5;}
  if(mode==ISSD_CAMERA_TACTICAL_WIDE) {num=2;den=3;}
  v.w=(width*den+num-1)/num;v.h=(height*den+num-1)/num;
  v.x=floor_div(256-v.w,2);v.y=floor_div(height-v.h,2);
  return v;
}
int issd_camera_project(int coordinate,int origin,int numerator,int denominator) {
  if(numerator<=0 || denominator<=0) return 0;
  return floor_div(((int64_t)coordinate-origin)*numerator,denominator);
}
const char *issd_camera_label(IssdCameraMode mode) {
  return mode==ISSD_CAMERA_TACTICAL ? "TACTICAL (80%)" :
    mode==ISSD_CAMERA_TACTICAL_WIDE ? "TACTICAL WIDE (67%)" : "CLASSIC";
}
