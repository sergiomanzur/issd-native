#include "retcomm_rbengine/input_hist.h"

#include <stdio.h>
#include <string.h>

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

static uint8_t hc_yes(void *ctx)
{
    (void)ctx;
    return 1;
}
static uint8_t hc_no(void *ctx)
{
    (void)ctx;
    return 0;
}

int main(void)
{
    RbeInputHist h;
    RNetRbFrame f, got;
    RNetInputContractFrame pub, wire;
    RNetInputContractParams params;
    RNetInputContractHostGates gates;
    RNetInputContractDecision d;

    rbe_ih_reset(&h, 2);
    CHECK(h.slot_count == 2, "slot_count");

    memset(&f, 0, sizeof(f));
    f.tick = 10;
    f.buttons = 0xFDFFu;
    f.stick_x = 40;
    f.analog = 1u;
    f.is_valid = 1u;
    CHECK(rbe_ih_put(&h, 0, &f), "put local");
    CHECK(rbe_ih_get(&h, 0, 10, &got), "get local");
    CHECK(got.stick_x == 40 && got.analog == 1u, "get fields");

    CHECK(rbe_ih_invent_hold_last(&h, 1, 11, &got), "invent neutral");
    CHECK(got.is_predicted && got.buttons == 0xFFFFu, "invent predicted neutral");
    CHECK(got.stick_x == 0 && got.analog == 0u, "invent sticks");

    f = got;
    f.tick = 11;
    f.buttons = 0xFBFFu;
    f.stick_x = -20;
    f.analog = 1u;
    f.is_predicted = 0;
    CHECK(rbe_ih_put(&h, 1, &f), "put auth remote");
    CHECK(rbe_ih_invent_hold_last(&h, 1, 12, &got), "invent hold-last");
    CHECK(got.buttons == 0xFBFFu && got.stick_x == -20 && got.analog == 1u,
          "held");
    CHECK(got.is_predicted, "hold-last predicted");

    CHECK(rbe_ih_invent_idle(&h, 1, 13, &got), "invent idle");
    CHECK(got.buttons == 0xFFFFu && got.stick_x == 0, "idle");

    f = got;
    f.stick_x = -18;
    f.is_predicted = 0;
    CHECK(rbe_ih_promote(&h, 1, &f), "promote");
    CHECK(rbe_ih_get(&h, 1, 13, &got) && !got.is_predicted && got.stick_x == -18,
          "promoted");

    rnet_input_contract_params_init_defaults(&params);
    rbe_ih_frame_to_contract(&got, &pub);
    pub.is_predicted = 1;
    pub.stick_x = -20;
    wire = pub;
    wire.is_predicted = 0;
    wire.stick_x = -18;
    memset(&gates, 0, sizeof(gates));
    gates.hash_confirm_promote = hc_no;
    d = rnet_input_contract_stick_replace_decide(&pub, &wire, 1, &params, &gates);
    CHECK(rnet_input_contract_decision_is_rewind(d), "no hash_confirm → rewind");
    gates.hash_confirm_promote = hc_yes;
    d = rnet_input_contract_stick_replace_decide(&pub, &wire, 1, &params, &gates);
    CHECK(d == nRNetInputContractPromoteHashConfirm, "hash_confirm → promote");

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
