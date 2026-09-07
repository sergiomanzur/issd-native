#include "recomp_net/address.h"
#include "platform/rnet_address_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

#define IPV4(a, b, c, d) \
    (((rnet_u32)(a) << 24) | ((rnet_u32)(b) << 16) | \
     ((rnet_u32)(c) << 8) | (rnet_u32)(d))

static void deterministic_normalization_test(void)
{
    const RNetIpv4Candidate candidates[] = {
        { IPV4(169, 254, 2, 3), "Link local" },
        { IPV4(8, 8, 8, 8), "Global" },
        { IPV4(192, 168, 1, 20), "Ethernet" },
        { IPV4(100, 64, 1, 2), "Carrier or VPN" },
        { IPV4(10, 0, 0, 7), "Wi-Fi Z" },
        { IPV4(10, 0, 0, 7), "Wi-Fi A" },
        { IPV4(127, 0, 0, 1), "Loopback" },
        { IPV4(0, 0, 0, 0), "Unspecified" },
        { IPV4(224, 0, 0, 1), "Multicast" },
    };
    RNetIpv4Address out[8];
    RNetIpv4Address truncated[2];
    int count;

    count = rnet_ipv4_normalize_candidates(
        candidates, sizeof(candidates) / sizeof(candidates[0]),
        IPV4(192, 168, 1, 20), out, sizeof(out) / sizeof(out[0]));
    expect_true(count == 6, "invalid and duplicate addresses are removed");
    expect_true(strcmp(out[0].address, "192.168.1.20") == 0,
                "default-route source ranks first");
    expect_true(strcmp(out[1].address, "10.0.0.7") == 0,
                "remaining RFC 1918 address ranks next");
    expect_true(strcmp(out[1].interface_label, "Wi-Fi A") == 0,
                "duplicate tie uses deterministic interface label");
    expect_true(strcmp(out[2].address, "100.64.1.2") == 0,
                "shared address ranks after private LAN");
    expect_true(strcmp(out[3].address, "8.8.8.8") == 0,
                "other unicast ranks after shared space");
    expect_true(strcmp(out[4].address, "169.254.2.3") == 0,
                "link-local ranks after LAN-capable unicast");
    expect_true(strcmp(out[5].address, "127.0.0.1") == 0,
                "loopback ranks last for same-machine play");

    memset(truncated, 0, sizeof(truncated));
    count = rnet_ipv4_normalize_candidates(
        candidates, sizeof(candidates) / sizeof(candidates[0]), 0,
        truncated, sizeof(truncated) / sizeof(truncated[0]));
    expect_true(count == 6, "truncated output still returns total count");
    expect_true(truncated[0].address[0] != '\0' &&
                truncated[1].address[0] != '\0',
                "truncated output fills available records");
    expect_true(rnet_ipv4_normalize_candidates(NULL, 1, 0, NULL, 0) == -1,
                "invalid candidate input is rejected");
}

static void live_enumeration_smoke_test(void)
{
    RNetIpv4Address *addresses = NULL;
    int queried = rnet_ipv4_enumerate(NULL, 0);
    int listed;
    int i;
    int j;

    expect_true(queried >= 0, "live IPv4 size query succeeds");
    if (queried <= 0)
    {
        return; /* An unusually restricted CI network may expose no address. */
    }
    addresses = (RNetIpv4Address *)calloc((size_t)queried,
                                           sizeof(*addresses));
    expect_true(addresses != NULL, "live IPv4 result allocation succeeds");
    if (addresses == NULL)
    {
        return;
    }
    listed = rnet_ipv4_enumerate(addresses, (size_t)queried);
    expect_true(listed >= 0, "live IPv4 enumeration succeeds");
    for (i = 0; i < listed && i < queried; ++i)
    {
        expect_true(addresses[i].address[0] != '\0',
                    "live IPv4 address text is nonempty");
        expect_true(addresses[i].interface_label[0] != '\0',
                    "live interface label is nonempty");
        for (j = 0; j < i; ++j)
        {
            expect_true(strcmp(addresses[i].address, addresses[j].address) != 0,
                        "live IPv4 addresses are deduplicated");
        }
    }
    free(addresses);
}

int main(void)
{
    deterministic_normalization_test();
    live_enumeration_smoke_test();
    if (failures == 0)
    {
        printf("address_test: ok\n");
        return 0;
    }
    fprintf(stderr, "address_test: %d failure(s)\n", failures);
    return 1;
}
