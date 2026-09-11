#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Deterministic scripted input, for driving the game to a specific place.
 *
 * --auto-start mashes A and Start on a timer. That cannot choose a menu item,
 * so it lands wherever the timing happens to put it, and it skips straight
 * past pre-match presentation before any of it can be looked at. A script
 * plays the same match the same way every run, which is what makes a visual
 * bug reproducible.
 *
 * File format, one entry per line, blank lines and # comments ignored:
 *
 *     <frame> <BUTTON>[,<BUTTON>...]
 *     <frame> NONE
 *
 * Buttons are held from that frame until the next entry, so a tap is a line
 * that presses and another a few frames later that releases:
 *
 *     240 START
 *     246 NONE
 */

#ifdef __cplusplus
extern "C" {
#endif

/* SNES pad bits, matching the mask main.c builds for RtlRunFrame. */
#define ISSD_BTN_B      (1u << 0)
#define ISSD_BTN_Y      (1u << 1)
#define ISSD_BTN_SELECT (1u << 2)
#define ISSD_BTN_START  (1u << 3)
#define ISSD_BTN_UP     (1u << 4)
#define ISSD_BTN_DOWN   (1u << 5)
#define ISSD_BTN_LEFT   (1u << 6)
#define ISSD_BTN_RIGHT  (1u << 7)
#define ISSD_BTN_A      (1u << 8)
#define ISSD_BTN_X      (1u << 9)
#define ISSD_BTN_L      (1u << 10)
#define ISSD_BTN_R      (1u << 11)

/* Load a script. Returns the number of entries, 0 if the file is unreadable
 * or empty (in which case the caller should fall back to its own input). */
int issd_script_load(const char *path);

/* True once a script is loaded and should drive player 1. */
bool issd_script_active(void);

/* Button mask to apply on this frame. */
uint32_t issd_script_mask(uint32_t frame);

#ifdef __cplusplus
}
#endif
