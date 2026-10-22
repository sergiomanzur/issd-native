#ifndef ISSD_LAUNCHER_H
#define ISSD_LAUNCHER_H
#include "issd_launcher_model.h"
#include <SDL.h>
/* Reuses the same SDL window/renderer on Android and desktop. The caller owns
 * these resources after return. capture writes one frame for visual QA. */
int issd_launcher_run(IssdConfig *cfg,char *rom,size_t capacity,
                      SDL_Window **window,SDL_Renderer **renderer,
                      const char *capture,const char *actions);
#endif
