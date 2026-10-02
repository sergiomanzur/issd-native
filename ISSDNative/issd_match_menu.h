#ifndef ISSD_MATCH_MENU_H
#define ISSD_MATCH_MENU_H
#include <stdint.h>
void issd_match_menu_open(void);
void issd_match_menu_step(int direction);
void issd_match_menu_adjust(int direction);
void issd_match_menu_confirm(void);
void issd_match_menu_cancel(void);
void issd_match_menu_click(int px, int py, int x, int y);
void issd_match_menu_render(uint32_t *fb, int width, int height, int x, int y,
    void (*text)(uint32_t *, int, int, int, int, const char *, uint32_t));
#endif
