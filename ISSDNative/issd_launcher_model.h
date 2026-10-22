#ifndef ISSD_LAUNCHER_MODEL_H
#define ISSD_LAUNCHER_MODEL_H
#include "issd_config.h"
#include <stddef.h>
enum { ISSD_LAUNCHER_HOME,ISSD_LAUNCHER_GRAPHICS,ISSD_LAUNCHER_CONTROLS,
       ISSD_LAUNCHER_GAMEPLAY,ISSD_LAUNCHER_MODS,ISSD_LAUNCHER_BROWSER };
bool issd_launcher_rom_ok(const char *path,char *why,size_t size);
int issd_launcher_move(int focus,int count,int direction);
void issd_launcher_adjust(IssdConfig *cfg,int page,int row,int direction);
#endif
