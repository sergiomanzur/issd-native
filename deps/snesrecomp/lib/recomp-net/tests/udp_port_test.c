#include "recomp_net/address.h"

#include "platform/rnet_platform.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    rnet_socket held;
    struct sockaddr_in addr;
    char endpoint[64];
    int free_port;

    rnet_os_startup();

    free_port = rnet_udp_find_free_port(57777, 8);
    expect(free_port >= 57777 && free_port <= 57784, "find free port in range");
    expect(rnet_udp_port_available(free_port) == 1, "reported free port binds");

    held = rnet_os_socket_create_dgram();
    expect(rnet_os_socket_valid(held), "hold socket created");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)free_port);
    expect(rnet_os_bind(held, &addr) == 0, "hold exclusive bind");
    expect(rnet_udp_port_available(free_port) == 0, "held port reports busy");
    expect(rnet_udp_find_free_port(free_port, 8) != free_port,
           "find skips busy preferred port");

    snprintf(endpoint, sizeof(endpoint), "192.168.1.20:%d", free_port);
    expect(rnet_endpoint_set_port(endpoint, sizeof(endpoint), free_port + 1) == 0,
           "endpoint rewrite ok");
    expect(strcmp(endpoint, "192.168.1.20") != 0, "host preserved with new port");
    {
        char expect_ep[64];
        snprintf(expect_ep, sizeof(expect_ep), "192.168.1.20:%d", free_port + 1);
        expect(strcmp(endpoint, expect_ep) == 0, "endpoint port rewritten");
    }

    rnet_os_socket_destroy(&held);

    if (failures == 0)
    {
        puts("udp_port_test: ok");
        return 0;
    }
    return 1;
}
