#pragma once
#include <stdbool.h>
#include <stdint.h>

/* On-screen gamepad for touch devices.
 *
 * Deliberately free of SDL and of Android: it takes a viewport size and
 * normalised touch points, and returns a SNES joypad mask plus a list of
 * rectangles to draw. That keeps the hit-testing, the auto-hide rules and the
 * layout arithmetic testable on the desktop, where there is no touchscreen,
 * rather than only on a device.
 *
 * Layout is computed from the viewport every time it changes, so rotation and
 * differing panel sizes need no per-device tuning.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* SNES joypad bit positions, matching the mask main.c already builds. */
enum {
    ISSD_PAD_B      = 1u << 0,
    ISSD_PAD_Y      = 1u << 1,
    ISSD_PAD_SELECT = 1u << 2,
    ISSD_PAD_START  = 1u << 3,
    ISSD_PAD_UP     = 1u << 4,
    ISSD_PAD_DOWN   = 1u << 5,
    ISSD_PAD_LEFT   = 1u << 6,
    ISSD_PAD_RIGHT  = 1u << 7,
    ISSD_PAD_A      = 1u << 8,
    ISSD_PAD_X      = 1u << 9,
    ISSD_PAD_L      = 1u << 10,
    ISSD_PAD_R      = 1u << 11
};

typedef enum {
    ISSD_TOUCH_DPAD = 0,   /* one control, four directions, 9-way hit test */
    ISSD_TOUCH_A, ISSD_TOUCH_B, ISSD_TOUCH_X, ISSD_TOUCH_Y,
    ISSD_TOUCH_L, ISSD_TOUCH_R,
    ISSD_TOUCH_START, ISSD_TOUCH_SELECT,
    ISSD_TOUCH_HIDE,       /* toggles the pad; hides with it */
    ISSD_TOUCH_MENU,       /* opens the overlay menu; NEVER hidden */
    ISSD_TOUCH_COUNT
} IssdTouchControl;

typedef struct {
    int  x, y, w, h;       /* viewport pixels */
    bool round;            /* face buttons draw as circles */
    bool visible;          /* false while the pad is hidden */
    bool pressed;
    const char *label;
} IssdTouchRect;

/* Recompute the layout. Safe to call every frame; only does work when the
 * viewport actually changed. */
void issd_touch_set_viewport(int width, int height);

/* Replace the current set of active touch points. Coordinates are viewport
 * pixels. Call once per frame with every finger currently down, then read
 * issd_touch_pad_mask(). */
void issd_touch_set_points(const int *xs, const int *ys, int count);

/* SNES joypad mask produced by the current touch points. */
uint16_t issd_touch_pad_mask(void);

/* True on the frame the menu button was released - edge triggered, so holding
 * it does not reopen the menu every frame. */
bool issd_touch_take_menu_press(void);

/* Whether the pad (everything except the menu button) is currently shown. */
bool issd_touch_pad_visible(void);
void issd_touch_set_pad_visible(bool visible);

/* Rectangles to draw, in draw order. Returns the count; entries whose
 * `visible` is false should be skipped. */
int issd_touch_rects(const IssdTouchRect **out);

/* Whether the whole overlay should be drawn at all: false on a device with no
 * touchscreen, or when the user has turned touch controls off. */
void issd_touch_set_enabled(bool enabled);
bool issd_touch_enabled(void);

#ifdef __cplusplus
}
#endif
