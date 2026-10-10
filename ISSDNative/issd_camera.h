#pragma once
#include "issd_config.h"

typedef struct { int x,y,w,h; } IssdCameraView;
IssdCameraView issd_camera_view(int width,int height,IssdCameraMode mode);
int issd_camera_project(int coordinate,int origin,int numerator,int denominator);
const char *issd_camera_label(IssdCameraMode mode);
