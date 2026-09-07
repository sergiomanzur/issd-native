#include "retcomm_rbengine/hash_confirm.h"

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
    RbeHashConfirm hc;
    rbe_hc_reset(&hc);

    CHECK(!rbe_hc_confirm_through(&hc, 0), "empty not confirmed");
    CHECK(rbe_hc_resolved_through(&hc) == 0u, "resolved starts 0");

    rbe_hc_note_local(&hc, 0, 0x1111u);
    rbe_hc_note_local(&hc, 1, 0x2222u);
    rbe_hc_note_local(&hc, 2, 0x3333u);
    CHECK(!rbe_hc_confirm_through(&hc, 0), "local-only not enough");

    rbe_hc_note_peer(&hc, 0, 0x1111u);
    CHECK(rbe_hc_confirm_through(&hc, 0), "tick 0 matched");
    CHECK(rbe_hc_resolved_through(&hc) == 0u, "resolved at 0");
    CHECK(!rbe_hc_confirm_through(&hc, 1), "tick 1 not yet");

    rbe_hc_note_peer(&hc, 1, 0x2222u);
    CHECK(rbe_hc_confirm_through(&hc, 1), "tick 1 matched");
    CHECK(rbe_hc_resolved_through(&hc) == 1u, "resolved at 1");

    rbe_hc_note_peer(&hc, 2, 0xDEADu);
    CHECK(rbe_hc_resolved_through(&hc) == 1u, "mismatch holds watermark");
    {
        uint32_t mt = 0, mld = 0, mpd = 0;
        CHECK(rbe_hc_peek_mismatch(&hc, &mt, &mld, &mpd), "peek mismatch");
        CHECK(mt == 2u && mld == 0x3333u && mpd == 0xDEADu, "peek mismatch vals");
    }

    rbe_hc_prime_after(&hc, 819u);
    rbe_hc_note_local(&hc, 820, 0xAAAAu);
    rbe_hc_note_peer(&hc, 820, 0xAAAAu);
    CHECK(rbe_hc_resolved_through(&hc) == 820u, "prime then match");

    rbe_hc_reset(&hc);
    rbe_hc_note_local(&hc, 0, 0x1u);
    rbe_hc_note_peer(&hc, 0, 0x1u);
    rbe_hc_note_local(&hc, 1, 0xAAAAu);
    rbe_hc_note_peer(&hc, 1, 0xBBBBu);
    rbe_hc_note_local(&hc, 1u + RBE_HC_RING, 0xCCCCu);
    rbe_hc_note_peer(&hc, 1u + RBE_HC_RING, 0xCCCCu);
    rbe_hc_note_local(&hc, 50u + RBE_HC_RING, 0xDDDDu);
    rbe_hc_note_peer(&hc, 50u + RBE_HC_RING, 0xDDDDu);
    CHECK(rbe_hc_heal_stale_gap(&hc), "heal advances over stale gap");
    CHECK(rbe_hc_resolved_through(&hc) == 50u + RBE_HC_RING, "heal tip");

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
