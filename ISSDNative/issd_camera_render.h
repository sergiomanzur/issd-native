#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "issd_camera.h"
#include "issd_widescreen.h"
typedef struct Ppu Ppu;
/* Direct sampling has no SNES tilemap-ring or OAM coordinate limits. */
bool issd_camera_render_world(const Ppu *ppu,const uint8_t *ram,
    const IssdCameraView *view,uint32_t *pixels,size_t stride);
bool issd_camera_prepare(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,size_t rom_size,
    IssdCameraMode mode,int width,int height);
bool issd_camera_compose(const Ppu *ppu,const uint8_t *ram,uint32_t *output,
    const uint32_t *hud,int width,int height);
bool issd_camera_active(void);
typedef const uint32_t *(*IssdCameraArtLookup)(const Ppu *,unsigned,unsigned,unsigned,int *);
void issd_camera_set_art_lookup(IssdCameraArtLookup lookup);
