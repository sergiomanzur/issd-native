#include "recomp_net/recomp_net.h"

#include <stdio.h>

static int g_failures;

static void expect_eq_u32(rnet_u32 got, rnet_u32 want, const char *msg)
{
    if (got != want)
    {
        fprintf(stderr, "FAIL: %s (got %u want %u)\n", msg, got, want);
        g_failures++;
    }
}

int main(void)
{
    rnet_u8 d;

    expect_eq_u32(rnet_wire_tick_from_sim(0, 0), 0, "D=0 identity");
    expect_eq_u32(rnet_sim_tick_from_wire(0, 0), 0, "D=0 reverse");

    for (d = 1; d <= 8; ++d)
    {
        rnet_u32 sim;
        for (sim = 0; sim < 64; ++sim)
        {
            rnet_u32 wire = rnet_wire_tick_from_sim(sim, d);
            expect_eq_u32(wire, sim + (rnet_u32)d, "forward map");
            expect_eq_u32(rnet_sim_tick_from_wire(wire, d), sim, "reverse map");
        }
        /* Underflow clamps to 0. */
        expect_eq_u32(rnet_sim_tick_from_wire((rnet_u32)(d - 1), d), 0, "underflow clamp");
    }

    /* Version string present. */
    if (rnet_version_string() == NULL || rnet_version_string()[0] == '\0')
    {
        fprintf(stderr, "FAIL: version string empty\n");
        g_failures++;
    }

    if (g_failures == 0)
    {
        printf("wire_map_test: ok (recomp-net %s)\n", rnet_version_string());
        return 0;
    }
    fprintf(stderr, "wire_map_test: %d failure(s)\n", g_failures);
    return 1;
}
