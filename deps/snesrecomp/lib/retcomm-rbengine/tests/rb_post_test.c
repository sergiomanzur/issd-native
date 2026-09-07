#include "retcomm_rbengine/rb_post.h"

#include <stdio.h>

static int failures;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL: %s\n", msg);                                         \
            failures++;                                                        \
        } else {                                                               \
            printf("ok:   %s\n", msg);                                         \
        }                                                                      \
    } while (0)

int main(void)
{
    CHECK(rbe_rb_peer_post_tip_ok(100, 100), "matching tip");
    CHECK(!rbe_rb_peer_post_tip_ok(100, 101), "stale tip rejected");
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
