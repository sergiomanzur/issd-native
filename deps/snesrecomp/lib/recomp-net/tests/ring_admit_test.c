#include "recomp_net/input.h"
#include "input/rnet_rings.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void expect_true(int cond, const char *msg)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_failures++;
    }
}

int main(void)
{
    RNetInputRing ring;
    RNetInputSample sample;
    RNetInputSample got;
    rnet_u8 delay = 3;
    rnet_u32 sim;
    rnet_u32 wire;

    rnet_ring_clear(&ring);

    /* Store a wire row and recover it. */
    memset(&sample, 0, sizeof(sample));
    sample.tick = 10;
    sample.size = 2;
    sample.bytes[0] = 0x11;
    sample.bytes[1] = 0x22;
    sample.valid = 1;
    rnet_ring_store(&ring, &sample);
    expect_true(rnet_ring_get(&ring, 10, &got) == 1, "get stored tick 10");
    expect_true(got.bytes[0] == 0x11 && got.bytes[1] == 0x22, "payload intact");
    expect_true(rnet_ring_get(&ring, 11, &got) == 0, "missing tick 11");

    /* Wrap: overwrite same slot index with a newer tick. */
    sample.tick = 10 + RNET_HISTORY_LENGTH;
    sample.bytes[0] = 0x99;
    rnet_ring_store(&ring, &sample);
    expect_true(rnet_ring_get(&ring, 10, &got) == 0, "old tick invalidated after wrap");
    expect_true(rnet_ring_get(&ring, 10 + RNET_HISTORY_LENGTH, &got) == 1, "wrapped tick present");
    expect_true(rnet_ring_highest_valid(&ring) == 10 + RNET_HISTORY_LENGTH, "highest valid");

    /* Admission mapping: remote wire for sim must equal sim + D. */
    for (sim = 0; sim < 8; ++sim)
    {
        wire = rnet_wire_tick_from_sim(sim, delay);
        expect_true(wire == sim + delay, "wire = sim + D");
        memset(&sample, 0, sizeof(sample));
        sample.tick = wire;
        sample.size = 1;
        sample.bytes[0] = (rnet_u8)sim;
        sample.valid = 1;
        rnet_ring_store(&ring, &sample);
        expect_true(rnet_ring_get(&ring, wire, &got) == 1, "remote wire row present for admit");
        expect_true(got.bytes[0] == (rnet_u8)sim, "wire payload matches sim");
    }

    if (g_failures == 0)
    {
        printf("ring_admit_test: ok\n");
        return 0;
    }
    fprintf(stderr, "ring_admit_test: %d failure(s)\n", g_failures);
    return 1;
}
