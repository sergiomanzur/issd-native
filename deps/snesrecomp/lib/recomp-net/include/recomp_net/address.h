#ifndef RNET_ADDRESS_H
#define RNET_ADDRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RNET_IPV4_ADDRESS_TEXT_MAX 16
#define RNET_ENDPOINT_TEXT_MAX 64
#define RNET_INTERFACE_LABEL_MAX 128
#define RNET_STUN_DEFAULT_HOST "stun.l.google.com"
#define RNET_STUN_DEFAULT_PORT 19302
#define RNET_STUN_DEFAULT_TIMEOUT_MS 750
#define RNET_STUN_MAX_TIMEOUT_MS 1000

/* One usable local IPv4 unicast address and the OS-provided interface label.
 * Results are ordered for presentation: the default-route source first,
 * private LAN addresses next, then other unicast and link-local addresses. */
typedef struct RNetIpv4Address {
    char address[RNET_IPV4_ADDRESS_TEXT_MAX];
    char interface_label[RNET_INTERFACE_LABEL_MAX];
} RNetIpv4Address;

/* Enumerate usable local IPv4 addresses. Loopback is included as the final
 * choice for same-machine sessions, after all LAN-capable addresses.
 *
 * Returns the total number of available unique addresses, or -1 on an OS or
 * allocation error. Up to capacity entries are written to out. Passing NULL
 * with capacity 0 is a supported size query. Because interfaces can change
 * between calls, callers should tolerate a second result larger than the
 * queried size and retry when they require the complete list. */
int rnet_ipv4_enumerate(RNetIpv4Address *out, size_t capacity);

/* Synchronous RFC 5389 STUN discovery. A NULL config, or zero/empty config
 * fields, selects the defaults above. Positive timeout values are clamped to
 * RNET_STUN_MAX_TIMEOUT_MS.
 *
 * rnet_external_ipv4_discover returns an IPv4 address only (ephemeral socket).
 * rnet_external_udp_endpoint_discover binds bind_hostport (e.g. "0.0.0.0:7777")
 * and returns the server-reflexive "A.B.C.D:mapped_port" for lobby advertise /
 * list RTT. Close any other owner of that UDP port first. */
typedef struct RNetExternalIpv4Config {
    const char *stun_host;
    unsigned short stun_port;
    int timeout_ms;
} RNetExternalIpv4Config;

enum {
    RNET_EXTERNAL_IPV4_OK = 0,
    RNET_EXTERNAL_IPV4_ERR_ARGUMENT = -1,
    RNET_EXTERNAL_IPV4_ERR_RESOLVE = -2,
    RNET_EXTERNAL_IPV4_ERR_SOCKET = -3,
    RNET_EXTERNAL_IPV4_ERR_TIMEOUT = -4,
    RNET_EXTERNAL_IPV4_ERR_PROTOCOL = -5,
    RNET_EXTERNAL_IPV4_ERR_RANDOM = -6,
    RNET_EXTERNAL_IPV4_ERR_BIND = -7
};

void rnet_external_ipv4_config_init(RNetExternalIpv4Config *config);
int rnet_external_ipv4_discover(const RNetExternalIpv4Config *config,
                                char *out, size_t out_len);
int rnet_external_udp_endpoint_discover(const RNetExternalIpv4Config *config,
                                        const char *bind_hostport,
                                        char *out_endpoint,
                                        size_t out_endpoint_len);

/* Exclusive UDP bind probe (no SO_REUSEADDR) so a busy lobby port is detected.
 * Returns 1 when the port can be bound on INADDR_ANY, else 0. */
int rnet_udp_port_available(int port);

/* Online host create: try preferred, then the next ports. span <= 0 selects 32
 * (MotK / recomp-ui contract). Returns a free port, or -1 when none in range. */
int rnet_udp_find_free_port(int preferred, int span);

/* Rewrite the port in a "host:port" endpoint (capacity includes NUL).
 * Preserves the host; empty/missing host becomes 0.0.0.0. Returns 0 on success. */
int rnet_endpoint_set_port(char *endpoint, size_t cap, int port);

#ifdef __cplusplus
}
#endif

#endif /* RNET_ADDRESS_H */
