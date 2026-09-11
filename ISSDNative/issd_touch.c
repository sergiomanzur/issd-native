#include "issd_touch.h"
#include <string.h>

#define MAX_POINTS 10

static struct {
    bool          enabled;
    bool          pad_visible;
    int           vw, vh;
    IssdTouchRect r[ISSD_TOUCH_COUNT];
    int           px[MAX_POINTS], py[MAX_POINTS], pn;
    uint16_t      mask;
    bool          menu_down, menu_fired, hide_down;
} T = { .enabled = true, .pad_visible = true };

static bool hit(const IssdTouchRect *r, int x, int y) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

/* Fingers slide off small targets constantly, so face buttons take a generous
 * margin. The d-pad does not: it is already large and a margin there would
 * overlap its neighbours. */
static bool hit_padded(const IssdTouchRect *r, int x, int y, int pad) {
    return x >= r->x - pad && x < r->x + r->w + pad &&
           y >= r->y - pad && y < r->y + r->h + pad;
}

static void place(IssdTouchControl c, int x, int y, int w, int h,
                  bool round, const char *label) {
    T.r[c].x = x; T.r[c].y = y; T.r[c].w = w; T.r[c].h = h;
    T.r[c].round = round; T.r[c].label = label; T.r[c].pressed = false;
}

void issd_touch_set_viewport(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == T.vw && height == T.vh) return;
    T.vw = width; T.vh = height;

    /* Everything scales off the short edge so the layout holds at any aspect
     * and any density, from a phone to a 1080p handheld. */
    const int s     = height < width ? height : width;
    const int unit  = s / 8;              /* face button diameter */
    const int gap   = s / 40;
    const int edge  = s / 16;
    const int dpad  = (int)(unit * 2.4f); /* whole d-pad, not one direction */
    const int small = (int)(unit * 0.62f);

    /* D-pad: bottom left. Face cluster: bottom right, diamond. */
    place(ISSD_TOUCH_DPAD, edge, height - edge - dpad, dpad, dpad, false, NULL);

    const int fx = width - edge - unit * 3 - gap * 2;
    const int fy = height - edge - unit * 3 - gap * 2;
    const int cx = fx + unit + gap;
    const int cy = fy + unit + gap;
    place(ISSD_TOUCH_Y, fx,                     cy,                     unit, unit, true, "Y");
    place(ISSD_TOUCH_X, cx,                     fy,                     unit, unit, true, "X");
    place(ISSD_TOUCH_A, cx + unit + gap,        cy,                     unit, unit, true, "A");
    place(ISSD_TOUCH_B, cx,                     cy + unit + gap,        unit, unit, true, "B");

    /* Shoulders along the top, clear of the face cluster. */
    place(ISSD_TOUCH_L, edge,                      edge, unit * 2, small, false, "L");
    place(ISSD_TOUCH_R, width - edge - unit * 2,   edge, unit * 2, small, false, "R");

    /* Start/Select centred low, where a thumb will not rest by accident. */
    const int sw = (int)(unit * 1.5f);
    place(ISSD_TOUCH_SELECT, width / 2 - sw - gap / 2, height - edge - small, sw, small, false, "SELECT");
    place(ISSD_TOUCH_START,  width / 2 + gap / 2,      height - edge - small, sw, small, false, "START");

    /* Hide toggle sits top-centre-left; the menu button top-centre-right.
     * The menu button is deliberately separated from the hide toggle so that
     * hiding the pad cannot be confused with losing access to the menu. */
    place(ISSD_TOUCH_HIDE, width / 2 - small * 2 - gap, edge, small * 2, small, false, "HIDE");
    place(ISSD_TOUCH_MENU, width / 2 + gap,             edge, small * 2, small, false, "MENU");
}

void issd_touch_set_enabled(bool enabled) { T.enabled = enabled; }
bool issd_touch_enabled(void) { return T.enabled; }
bool issd_touch_pad_visible(void) { return T.pad_visible; }
void issd_touch_set_pad_visible(bool visible) { T.pad_visible = visible; }

void issd_touch_set_points(const int *xs, const int *ys, int count) {
    if (count < 0) count = 0;
    if (count > MAX_POINTS) count = MAX_POINTS;
    T.pn = count;
    for (int i = 0; i < count; i++) { T.px[i] = xs[i]; T.py[i] = ys[i]; }

    for (int i = 0; i < ISSD_TOUCH_COUNT; i++) T.r[i].pressed = false;
    T.mask = 0;

    if (!T.enabled || T.vw == 0) { T.menu_down = false; return; }

    const int pad_margin = T.vh / 60;
    bool menu_now = false, hide_now = false;

    for (int i = 0; i < count; i++) {
        const int x = T.px[i], y = T.py[i];

        /* The menu button is live whether or not the pad is hidden - that is
         * the point of it. Checked first so it wins any overlap. */
        if (hit_padded(&T.r[ISSD_TOUCH_MENU], x, y, pad_margin)) {
            menu_now = true;
            T.r[ISSD_TOUCH_MENU].pressed = true;
            continue;
        }
        if (!T.pad_visible) continue;

        if (hit_padded(&T.r[ISSD_TOUCH_HIDE], x, y, pad_margin)) {
            hide_now = true;
            T.r[ISSD_TOUCH_HIDE].pressed = true;
            continue;
        }

        /* D-pad as a 3x3: the centre cell presses nothing, edges press one
         * direction, corners press two so diagonals work. */
        const IssdTouchRect *d = &T.r[ISSD_TOUCH_DPAD];
        if (hit(d, x, y)) {
            const int col = (x - d->x) * 3 / (d->w ? d->w : 1);
            const int row = (y - d->y) * 3 / (d->h ? d->h : 1);
            if (col == 0) T.mask |= ISSD_PAD_LEFT;
            else if (col == 2) T.mask |= ISSD_PAD_RIGHT;
            if (row == 0) T.mask |= ISSD_PAD_UP;
            else if (row == 2) T.mask |= ISSD_PAD_DOWN;
            if (T.mask) T.r[ISSD_TOUCH_DPAD].pressed = true;
            continue;
        }

        static const struct { IssdTouchControl c; uint16_t bit; } buttons[] = {
            { ISSD_TOUCH_A, ISSD_PAD_A }, { ISSD_TOUCH_B, ISSD_PAD_B },
            { ISSD_TOUCH_X, ISSD_PAD_X }, { ISSD_TOUCH_Y, ISSD_PAD_Y },
            { ISSD_TOUCH_L, ISSD_PAD_L }, { ISSD_TOUCH_R, ISSD_PAD_R },
            { ISSD_TOUCH_START, ISSD_PAD_START },
            { ISSD_TOUCH_SELECT, ISSD_PAD_SELECT },
        };
        for (unsigned b = 0; b < sizeof(buttons) / sizeof(buttons[0]); b++) {
            if (hit_padded(&T.r[buttons[b].c], x, y, pad_margin)) {
                T.mask |= buttons[b].bit;
                T.r[buttons[b].c].pressed = true;
                break;
            }
        }
    }

    /* Both toggles fire on release, so a finger held down does not repeat. */
    if (!menu_now && T.menu_down) T.menu_fired = true;
    if (!hide_now && T.hide_down) T.pad_visible = !T.pad_visible;
    T.menu_down = menu_now;
    T.hide_down = hide_now;
}

uint16_t issd_touch_pad_mask(void) { return T.enabled ? T.mask : 0; }

bool issd_touch_take_menu_press(void) {
    bool fired = T.menu_fired;
    T.menu_fired = false;
    return fired;
}

int issd_touch_rects(const IssdTouchRect **out) {
    for (int i = 0; i < ISSD_TOUCH_COUNT; i++) {
        /* The menu button is never hidden. Everything else follows the pad. */
        T.r[i].visible = T.enabled &&
                         (i == ISSD_TOUCH_MENU || T.pad_visible);
    }
    if (out) *out = T.r;
    return ISSD_TOUCH_COUNT;
}
