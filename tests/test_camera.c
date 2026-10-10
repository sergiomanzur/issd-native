#include <assert.h>
#include <limits.h>
#include "issd_camera.h"

int main(void) {
  IssdCameraView v=issd_camera_view(398,224,ISSD_CAMERA_TACTICAL);
  assert(v.w==498 && v.h==280 && v.x==-121 && v.y==-28);
  v=issd_camera_view(398,224,ISSD_CAMERA_TACTICAL_WIDE);
  assert(v.w==597 && v.h==336 && v.x==-171 && v.y==-56);
  v=issd_camera_view(504,224,ISSD_CAMERA_TACTICAL_WIDE);
  assert(v.w==756 && v.h==336 && v.x==-250 && v.y==-56);
  v=issd_camera_view(256,224,ISSD_CAMERA_CLASSIC);
  assert(v.w==256 && v.h==224 && v.x==0 && v.y==0);
  v=issd_camera_view(358,224,ISSD_CAMERA_TACTICAL);
  assert(v.w==448 && v.h==280 && v.x==-96);
  v=issd_camera_view(320,224,ISSD_CAMERA_TACTICAL_WIDE);
  assert(v.w==480 && v.x==-112);
  v=issd_camera_view(399,225,ISSD_CAMERA_TACTICAL_WIDE);
  assert(v.w==599 && v.h==338 && v.x==-172 && v.y==-57);
  v=issd_camera_view(398,224,(IssdCameraMode)99);
  assert(v.w==398 && v.h==224 && v.x==-71);
  assert(issd_camera_view(0,224,ISSD_CAMERA_TACTICAL).w==0);
  assert(issd_camera_view(INT_MAX,224,ISSD_CAMERA_TACTICAL).w==0);
  assert(issd_camera_project(-1,0,4,5)==-1);
  assert(issd_camera_project(128,-121,4,5)==199);
  assert(issd_camera_project(-122,-121,4,5)==-1);
  assert(issd_camera_project(280,-56,2,3)==224);
  return 0;
}
