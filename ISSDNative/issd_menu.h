#ifndef ISSD_MENU_H
#define ISSD_MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "issd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ISSD_SCHEMA_CLASSIC = 0,   /* SNES ISSD: B=Pass, A=Shoot, Y=Dash, X=Through */
    ISSD_SCHEMA_FIFA = 1,      /* Modern FIFA: A=Pass, B=Shoot, X=Cross, Y=Through, RT/RB=Sprint */
    ISSD_SCHEMA_PES = 2        /* Modern PES: A=Pass, X=Shoot, B=Cross, Y=Through, RB=Sprint */
} IssdControlSchema;

typedef struct {
    bool is_open;
    int current_item;
    int current_slot;
    IssdControlSchema control_schema;
    char status_message[64];
    int status_timer;
} IssdOverlayMenu;

extern IssdOverlayMenu g_overlay_menu;

void issd_menu_init(void);
void issd_menu_toggle(void);
void issd_menu_open(void);
void issd_menu_close(void);
bool issd_menu_is_open(void);

/* Navigation: returns true if handled */
bool issd_menu_navigate_up(void);
bool issd_menu_navigate_down(void);
bool issd_menu_navigate_left(void);
bool issd_menu_navigate_right(void);
bool issd_menu_confirm(void);
bool issd_menu_cancel(void);

/* Render overlay on top of 256x224 32-bit ARGB framebuffer */
void issd_menu_render(uint32_t *framebuffer, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_MENU_H */
