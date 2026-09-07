#include "retcomm_rbengine/mono_ms.h"
#include "retcomm_rbengine/sched.h"

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

static uint32_t now_ms(void *ctx)
{
    (void)ctx;
    return rbe_mono_ms();
}

int main(void)
{
    RbeSchedBridge br;
    int delay = 4;
    int pred = 8;
    int slot = 0;
    RNetSessionStats st;
    const char *reason = NULL;
    int stall;

    memset(&br, 0, sizeof(br));
    br.session = NULL;
    br.input_delay = &delay;
    br.input_prediction = &pred;
    br.local_slot = &slot;
    br.gates.now_ms = now_ms;

    rbe_sched_bind(&br);
    CHECK(rbe_sched_real_delay_enabled(), "real-delay default");
    CHECK(rbe_sched_wire_for_sim(42) == 42u, "real-delay wire==sim");

    memset(&st, 0, sizeof(st));
    st.sim_tick = 10;
    st.highest_remote_wire = 10;
    st.remote_lead = 4;
    stall = rbe_sched_pre_admit(10, 10, &st);
    CHECK(!stall, "pre_admit proceed with healthy lead");

    /* Past P → pcap freeze stall. */
    st.highest_remote_wire = 10;
    stall = rbe_sched_on_remote_miss(1, 20, 30, &st, pred, &reason);
    CHECK(stall == 1, "pcap freeze stalls");
    CHECK(reason && strcmp(reason, "pcap_freeze") == 0, "pcap reason");

    rbe_sched_set_admit_stall("test");
    CHECK(strcmp(rbe_sched_admit_stall_tag(), "test") == 0, "stall tag");
    rbe_sched_clear_admit_stall();
    CHECK(rbe_sched_admit_stall_tag()[0] == '\0', "stall cleared");

    rbe_sched_note_episode_boundary();
    rbe_sched_arm_absurd_invent_catchup();
    rbe_sched_post_admit(0);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
