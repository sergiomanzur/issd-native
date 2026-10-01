#ifndef ISSD_CONTROLS_H
#define ISSD_CONTROLS_H
#include <stdbool.h>
#include <stdint.h>
bool issd_controls_active(void);
void issd_controls_open(void);
void issd_controls_step(int direction);
void issd_controls_adjust(int direction);
void issd_controls_confirm(void);
void issd_controls_cancel(void);
void issd_controls_end_capture(void);
void issd_controls_set_key_label(const char *(*label)(int));
void issd_controls_render(uint32_t *fb, int w, int h, int x, int y,
    void (*text)(uint32_t *, int, int, int, int, const char *, uint32_t));
#endif
