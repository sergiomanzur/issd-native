#include "issd_touch.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int xs[ISSD_TOUCH_COUNT], ys[ISSD_TOUCH_COUNT], sizes[ISSD_TOUCH_COUNT];
static const IssdTouchRect *rects(void) {
    const IssdTouchRect *r = NULL;
    assert(issd_touch_rects(&r) == ISSD_TOUCH_COUNT);
    return r;
}
static void defaults(void) {
    for (int i = 0; i < ISSD_TOUCH_COUNT; ++i) { xs[i] = ys[i] = -1; sizes[i] = 100; }
    issd_touch_set_layout(xs,ys,sizes);
    issd_touch_set_viewport(1920,1080);
    issd_touch_set_enabled(true); issd_touch_set_pad_visible(true);
    issd_touch_reset_points();
}
static uint16_t hold(int x,int y) {
    issd_touch_set_points(&x,&y,1);
    return issd_touch_pad_mask();
}
static void tap(int x,int y) { hold(x,y); issd_touch_set_points(NULL,NULL,0); }
static void rectangle(unsigned id,int x,int y,int w,int h) {
    const IssdTouchRect *r = &rects()[id];
    assert(r->x == x && r->y == y && r->w == w && r->h == h);
}
static void default_rectangles(void) {
    /* Hand-checked original 1920x1080 rectangles, in enum order. */
    static const int expected[ISSD_TOUCH_COUNT][4] = {
        {67,689,324,324},{1718,716,135,135},{1556,878,135,135},
        {1556,554,135,135},{1394,716,135,135},{67,67,270,83},
        {1583,67,270,83},{973,930,202,83},{745,930,202,83},
        {767,67,166,83},{987,67,166,83}
    };
    for (unsigned i = 0; i < ISSD_TOUCH_COUNT; ++i)
        rectangle(i,expected[i][0],expected[i][1],expected[i][2],expected[i][3]);
}
static void moved(void) {
    xs[ISSD_TOUCH_A] = ys[ISSD_TOUCH_A] = 500;
    issd_touch_set_layout(xs,ys,sizes);
    rectangle(ISSD_TOUCH_A,893,473,135,135);
    assert(hold(960,540) == ISSD_PAD_A);
    assert(hold(1785,783) == 0); /* Its old centre must stop pressing A. */
    xs[ISSD_TOUCH_DPAD] = 300; ys[ISSD_TOUCH_DPAD] = 500;
    issd_touch_set_layout(xs,ys,sizes);
    assert(hold(468,432) == (ISSD_PAD_LEFT | ISSD_PAD_UP));
    assert(hold(576,540) == 0);
}
static void scaled(void) {
    sizes[ISSD_TOUCH_A] = 200;
    issd_touch_set_layout(xs,ys,sizes);
    rectangle(ISSD_TOUCH_A,1650,648,270,270);
    assert(hold(1900,700) == ISSD_PAD_A); /* Outside its old padded rectangle. */
    sizes[ISSD_TOUCH_A] = 50;
    issd_touch_set_layout(xs,ys,sizes);
    rectangle(ISSD_TOUCH_A,1752,750,67,67);
    assert(hold(1785,783) == ISSD_PAD_A);
    assert(hold(1720,780) == 0);
}
static void rotation(void) {
    xs[ISSD_TOUCH_A] = 750; ys[ISSD_TOUCH_A] = 500;
    issd_touch_set_layout(xs,ys,sizes);
    rectangle(ISSD_TOUCH_A,1373,473,135,135);
    issd_touch_set_viewport(1080,1920);
    rectangle(ISSD_TOUCH_A,743,893,135,135);
    assert(hold(810,960) == ISSD_PAD_A);
    issd_touch_set_viewport(1920,1080);
    rectangle(ISSD_TOUCH_A,1373,473,135,135);
}
static void bounds(void) {
    static const int views[][2] = {{1920,1080},{1080,1920},{1,1},{1,10000},
                                  {10000,1},{7,11},{19,17},{INT_MAX,INT_MAX}};
    for (int i = 0; i < ISSD_TOUCH_COUNT; ++i) {
        xs[i] = i % 2 ? 1000 : 0; ys[i] = i % 2 ? 0 : 1000; sizes[i] = 200;
    }
    issd_touch_set_layout(xs,ys,sizes);
    for (unsigned v = 0; v < sizeof(views)/sizeof(views[0]); ++v) {
        issd_touch_set_viewport(views[v][0],views[v][1]);
        const IssdTouchRect *r = rects();
        for (int i = 0; i < ISSD_TOUCH_COUNT; ++i) {
            assert(r[i].x >= 0 && r[i].y >= 0 && r[i].w >= 1 && r[i].h >= 1);
            assert((int64_t)r[i].x+r[i].w <= views[v][0]);
            assert((int64_t)r[i].y+r[i].h <= views[v][1]);
        }
        issd_touch_set_pad_visible(false);
        assert(r[ISSD_TOUCH_MENU].visible);
        int x = r[ISSD_TOUCH_MENU].x+r[ISSD_TOUCH_MENU].w/2;
        int y = r[ISSD_TOUCH_MENU].y+r[ISSD_TOUCH_MENU].h/2;
        tap(x,y); assert(issd_touch_take_menu_press());
        assert(!issd_touch_pad_visible());
        issd_touch_set_pad_visible(true);
    }
}
static void invalid(void) {
    for (int i = 0; i < ISSD_TOUCH_COUNT; ++i) {
        xs[i] = i % 2 ? 1001 : INT_MIN;
        ys[i] = i % 2 ? INT_MAX : -2;
        sizes[i] = i % 2 ? 201 : 49;
    }
    issd_touch_set_layout(xs,ys,sizes); default_rectangles();
    xs[ISSD_TOUCH_A] = 500; ys[ISSD_TOUCH_A] = 500; sizes[ISSD_TOUCH_A] = 200;
    issd_touch_set_layout(xs,ys,sizes);
    issd_touch_set_layout(NULL,NULL,NULL); default_rectangles();
}
static void extreme_dpad(void) {
    xs[ISSD_TOUCH_DPAD] = ys[ISSD_TOUCH_DPAD] = 500;
    sizes[ISSD_TOUCH_DPAD] = 200;
    issd_touch_set_layout(xs,ys,sizes); issd_touch_set_viewport(INT_MAX,INT_MAX);
    const IssdTouchRect *d = &rects()[ISSD_TOUCH_DPAD];
    int x = (int)(d->x + (int64_t)d->w*5/6);
    int y = (int)(d->y + (int64_t)d->h*5/6);
    assert(hold(x,y) == (ISSD_PAD_RIGHT | ISSD_PAD_DOWN));
}
static void clear_held(bool change_layout,bool change_viewport) {
    tap(1070,108); /* Pending menu release edge. */
    int px[] = {1785,850,1250}, py[] = {783,108,400};
    issd_touch_set_points(px,py,3); /* A, held Hide, pending outside-screen tap. */
    assert(issd_touch_pad_mask() == ISSD_PAD_A);
    assert(rects()[ISSD_TOUCH_HIDE].pressed);
    if (change_layout) {
        xs[ISSD_TOUCH_A] = 500; issd_touch_set_layout(xs,ys,sizes);
    } else if (change_viewport) issd_touch_set_viewport(1080,1920);
    else issd_touch_reset_points();
    assert(issd_touch_pad_mask() == 0);
    assert(!issd_touch_take_menu_press());
    assert(!issd_touch_take_screen_tap(NULL,NULL));
    const IssdTouchRect *r = rects();
    for (int i = 0; i < ISSD_TOUCH_COUNT; ++i) assert(!r[i].pressed);
    issd_touch_set_points(NULL,NULL,0);
    assert(!issd_touch_take_menu_press());
    assert(issd_touch_pad_visible()); /* Dropping held Hide is not a release tap. */
    hold(1250,400); assert(issd_touch_take_screen_tap(NULL,NULL));
}
static void same_config(void) {
    assert(hold(1785,783) == ISSD_PAD_A);
    issd_touch_set_layout(xs,ys,sizes); issd_touch_set_viewport(1920,1080);
    assert(issd_touch_pad_mask() == ISSD_PAD_A);
    assert(rects()[ISSD_TOUCH_A].pressed);
}
static void center(void) {
    int x = -2, y = -2;
    assert(issd_touch_center(ISSD_TOUCH_A,&x,&y));
    assert(x == 930 && y == 725); /* Original pixel centre1785,783. */
    xs[ISSD_TOUCH_A] = x; ys[ISSD_TOUCH_A] = y;
    issd_touch_set_layout(xs,ys,sizes);
    assert(issd_touch_center(ISSD_TOUCH_A,&x,&y));
    assert(x == 930 && y == 725);
    assert(hold(1785,783) == ISSD_PAD_A);
    assert(issd_touch_center(ISSD_TOUCH_MENU,&x,&y));
    assert(x == 557 && y == 100);
    assert(issd_touch_center(ISSD_TOUCH_A,NULL,NULL));
    x = 123; y = 456;
    assert(!issd_touch_center(-1,&x,&y));
    assert(!issd_touch_center(ISSD_TOUCH_COUNT,&x,&y));
    assert(x == 123 && y == 456);
    issd_touch_set_viewport(1,1);
    assert(issd_touch_center(ISSD_TOUCH_A,&x,&y));
    assert(x == 500 && y == 500);
    issd_touch_set_viewport(INT_MAX,INT_MAX);
    for (int i = 0; i < ISSD_TOUCH_COUNT; ++i) {
        assert(issd_touch_center(i,&x,&y));
        assert(x >= 0 && x <= 1000 && y >= 0 && y <= 1000);
    }
}
int main(int argc,char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1],"unknown-center")) {
        int x = 123, y = 456;
        assert(!issd_touch_center(ISSD_TOUCH_A,&x,&y));
        assert(x == 123 && y == 456);
        puts("unknown viewport centre rejected"); return 0;
    }
    defaults();
    if (!strcmp(argv[1],"default")) default_rectangles();
    else if (!strcmp(argv[1],"moved")) moved();
    else if (!strcmp(argv[1],"scaled")) scaled();
    else if (!strcmp(argv[1],"rotation")) rotation();
    else if (!strcmp(argv[1],"bounds")) bounds();
    else if (!strcmp(argv[1],"invalid")) invalid();
    else if (!strcmp(argv[1],"extreme-dpad")) extreme_dpad();
    else if (!strcmp(argv[1],"reset")) clear_held(false,false);
    else if (!strcmp(argv[1],"layout-reset")) clear_held(true,false);
    else if (!strcmp(argv[1],"viewport-reset")) clear_held(false,true);
    else if (!strcmp(argv[1],"same-config")) same_config();
    else if (!strcmp(argv[1],"center")) center();
    else assert(0);
    puts("touch layout case passed"); return 0;
}
