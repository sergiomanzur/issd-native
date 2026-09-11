/* Touch gamepad: hit testing, hide/unhide, and the menu button that must
 * survive hiding. Runs on the desktop - no device, no emulator. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_touch.h"

/* Retroid Pocket 6 class handheld: 1080p landscape. */
#define W 1920
#define H 1080

static const IssdTouchRect *rect(IssdTouchControl c) {
    const IssdTouchRect *r = NULL;
    int n = issd_touch_rects(&r);
    assert(n == ISSD_TOUCH_COUNT);
    return &r[c];
}

static void tap_at(int x, int y) {          /* press then release */
    int xs[1] = { x }, ys[1] = { y };
    issd_touch_set_points(xs, ys, 1);
    issd_touch_set_points(NULL, NULL, 0);
}

static uint16_t hold_at(int x, int y) {
    int xs[1] = { x }, ys[1] = { y };
    issd_touch_set_points(xs, ys, 1);
    return issd_touch_pad_mask();
}

static void centre_of(IssdTouchControl c, int *x, int *y) {
    const IssdTouchRect *r = rect(c);
    *x = r->x + r->w / 2;
    *y = r->y + r->h / 2;
}

static uint16_t tap_button(IssdTouchControl c) {
    int x, y; centre_of(c, &x, &y);
    return hold_at(x, y);
}

static void reset(void) {
    issd_touch_set_enabled(true);
    issd_touch_set_pad_visible(true);
    issd_touch_set_points(NULL, NULL, 0);
    (void)issd_touch_take_menu_press();
}

int main(void) {
    issd_touch_set_viewport(W, H);
    reset();

    /* --- every face button reports its own bit, and only its own --- */
    struct { IssdTouchControl c; uint16_t bit; const char *name; } b[] = {
        { ISSD_TOUCH_A, ISSD_PAD_A, "A" }, { ISSD_TOUCH_B, ISSD_PAD_B, "B" },
        { ISSD_TOUCH_X, ISSD_PAD_X, "X" }, { ISSD_TOUCH_Y, ISSD_PAD_Y, "Y" },
        { ISSD_TOUCH_L, ISSD_PAD_L, "L" }, { ISSD_TOUCH_R, ISSD_PAD_R, "R" },
        { ISSD_TOUCH_START,  ISSD_PAD_START,  "START"  },
        { ISSD_TOUCH_SELECT, ISSD_PAD_SELECT, "SELECT" },
    };
    for (unsigned i = 0; i < sizeof(b) / sizeof(b[0]); i++) {
        uint16_t m = tap_button(b[i].c);
        if (m != b[i].bit) {
            printf("button %s produced mask %04X, expected %04X\n",
                   b[i].name, m, b[i].bit);
            assert(0);
        }
    }
    issd_touch_set_points(NULL, NULL, 0);

    /* --- no two controls overlap: every centre hits exactly one thing --- */
    for (int i = 0; i < ISSD_TOUCH_COUNT; i++) {
        if (i == ISSD_TOUCH_DPAD) continue;          /* 9-way, tested below */
        const IssdTouchRect *ri = rect((IssdTouchControl)i);
        for (int j = i + 1; j < ISSD_TOUCH_COUNT; j++) {
            const IssdTouchRect *rj = rect((IssdTouchControl)j);
            bool overlap = ri->x < rj->x + rj->w && rj->x < ri->x + ri->w &&
                           ri->y < rj->y + rj->h && rj->y < ri->y + ri->h;
            if (overlap) { printf("controls %d and %d overlap\n", i, j); assert(0); }
        }
    }

    /* --- everything stays inside the viewport --- */
    for (int i = 0; i < ISSD_TOUCH_COUNT; i++) {
        const IssdTouchRect *r = rect((IssdTouchControl)i);
        assert(r->w > 0 && r->h > 0);
        assert(r->x >= 0 && r->y >= 0);
        assert(r->x + r->w <= W && r->y + r->h <= H);
    }

    /* --- d-pad: edges give one direction, corners give clean diagonals --- */
    const IssdTouchRect *d = rect(ISSD_TOUCH_DPAD);
    const int l = d->x + d->w / 6, cx = d->x + d->w / 2, r_ = d->x + d->w * 5 / 6;
    const int t = d->y + d->h / 6, cy = d->y + d->h / 2, bo = d->y + d->h * 5 / 6;
    assert(hold_at(cx, t)  == ISSD_PAD_UP);
    assert(hold_at(cx, bo) == ISSD_PAD_DOWN);
    assert(hold_at(l,  cy) == ISSD_PAD_LEFT);
    assert(hold_at(r_, cy) == ISSD_PAD_RIGHT);
    assert(hold_at(l,  t)  == (ISSD_PAD_LEFT  | ISSD_PAD_UP));
    assert(hold_at(r_, bo) == (ISSD_PAD_RIGHT | ISSD_PAD_DOWN));
    assert(hold_at(cx, cy) == 0);                 /* dead centre presses nothing */
    issd_touch_set_points(NULL, NULL, 0);

    /* --- multi-touch: a direction and a button at once, as in a real match --- */
    {
        int ax, ay; centre_of(ISSD_TOUCH_A, &ax, &ay);
        int xs[2] = { r_, ax }, ys[2] = { cy, ay };
        issd_touch_set_points(xs, ys, 2);
        assert(issd_touch_pad_mask() == (ISSD_PAD_RIGHT | ISSD_PAD_A));
        issd_touch_set_points(NULL, NULL, 0);
    }

    /* --- hide toggles the pad, and fires once per tap, not per frame --- */
    reset();
    assert(issd_touch_pad_visible());
    int hx, hy; centre_of(ISSD_TOUCH_HIDE, &hx, &hy);
    {   /* hold for several frames: must toggle exactly once, on release */
        int xs[1] = { hx }, ys[1] = { hy };
        for (int f = 0; f < 5; f++) issd_touch_set_points(xs, ys, 1);
        assert(issd_touch_pad_visible());          /* not yet - still held */
        issd_touch_set_points(NULL, NULL, 0);
        assert(!issd_touch_pad_visible());         /* released: hidden */
    }

    /* --- while hidden, the pad is inert --- */
    assert(tap_button(ISSD_TOUCH_A) == 0);
    assert(hold_at(cx, t) == 0);
    issd_touch_set_points(NULL, NULL, 0);

    /* --- while hidden, only the menu button is drawn --- */
    {
        const IssdTouchRect *all = NULL;
        int n = issd_touch_rects(&all);
        for (int i = 0; i < n; i++) {
            if (i == ISSD_TOUCH_MENU) assert(all[i].visible);
            else                      assert(!all[i].visible);
        }
    }

    /* --- THE requirement: the menu button still works while hidden --- */
    {
        int mx, my; centre_of(ISSD_TOUCH_MENU, &mx, &my);
        assert(!issd_touch_pad_visible());
        assert(!issd_touch_take_menu_press());
        tap_at(mx, my);
        assert(issd_touch_take_menu_press());
        assert(!issd_touch_take_menu_press());     /* edge triggered, consumed */
        assert(!issd_touch_pad_visible());         /* menu does not unhide */
    }

    /* --- unhide works from hidden: the hide button is reachable again --- */
    issd_touch_set_pad_visible(true);
    assert(issd_touch_pad_visible());
    tap_at(hx, hy);
    assert(!issd_touch_pad_visible());
    issd_touch_set_pad_visible(true);

    /* --- menu press is edge triggered even when held for many frames --- */
    reset();
    {
        int mx, my; centre_of(ISSD_TOUCH_MENU, &mx, &my);
        int xs[1] = { mx }, ys[1] = { my };
        for (int f = 0; f < 10; f++) {
            issd_touch_set_points(xs, ys, 1);
            assert(!issd_touch_take_menu_press());  /* never while held */
        }
        issd_touch_set_points(NULL, NULL, 0);
        assert(issd_touch_take_menu_press());       /* exactly once, on release */
        assert(!issd_touch_take_menu_press());
    }

    /* --- disabled: nothing is drawn and nothing is pressed --- */
    reset();
    issd_touch_set_enabled(false);
    assert(tap_button(ISSD_TOUCH_A) == 0);
    {
        const IssdTouchRect *all = NULL;
        int n = issd_touch_rects(&all);
        for (int i = 0; i < n; i++) assert(!all[i].visible);
    }
    issd_touch_set_enabled(true);

    /* --- layout survives rotation and small screens --- */
    const int sizes[][2] = { {1920,1080}, {1080,1920}, {1280,720}, {960,544}, {2400,1080} };
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        const int vw = sizes[i][0], vh = sizes[i][1];
        issd_touch_set_viewport(vw, vh);
        reset();
        for (int c = 0; c < ISSD_TOUCH_COUNT; c++) {
            const IssdTouchRect *r = rect((IssdTouchControl)c);
            if (r->x < 0 || r->y < 0 || r->x + r->w > vw || r->y + r->h > vh) {
                printf("control %d escapes %dx%d viewport: %d,%d %dx%d\n",
                       c, vw, vh, r->x, r->y, r->w, r->h);
                assert(0);
            }
        }
        /* the menu button must remain tappable at every size */
        int mx, my; centre_of(ISSD_TOUCH_MENU, &mx, &my);
        tap_at(mx, my);
        assert(issd_touch_take_menu_press());
    }

    puts("touch control tests passed");
    return 0;
}
