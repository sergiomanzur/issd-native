# LAN address discovery

LAN hosts should bind their UDP socket to `0.0.0.0:port`, but advertise a
concrete local address that the other player can reach. `rnet_ipv4_enumerate`
provides the address choices and their OS interface labels:

```c
int count = rnet_ipv4_enumerate(NULL, 0);
RNetIpv4Address *addresses = NULL;
if (count > 0) {
    addresses = calloc((size_t)count, sizeof(*addresses));
    count = rnet_ipv4_enumerate(addresses, (size_t)count);
}
```

The function uses `GetAdaptersAddresses` on Windows and `getifaddrs` on POSIX.
It excludes unspecified, multicast, down interfaces, and Windows addresses that
have not completed duplicate-address detection. Duplicate addresses are
removed. Loopback remains available for two instances on one machine.

Results have a deterministic, useful presentation order: the source address
selected by the default IPv4 route, RFC 1918 private addresses, RFC 6598 shared
space, other unicast, RFC 3927 link-local, and finally loopback. Numeric address
and interface label break ties. The default-route probe only connects a UDP
socket; it does not send a packet. A host UI should still let the player choose,
because VPNs and virtual adapters can be either intentional or unrelated to the
peer's LAN.

The return value is the total available count even when the output capacity is
smaller. Interface state can change between a size query and the fill call, so
retry if a complete list is required and the second result exceeds capacity.

This API discovers local interface addresses only. A public NAT address and its
UDP port mapping require STUN/ICE, explicit port forwarding, or a trusted lobby
service that reports the observed endpoint.

## External IPv4 through STUN

`rnet_external_ipv4_discover` sends a synchronous RFC 5389 Binding request and
returns the server-reflexive IPv4 address from `XOR-MAPPED-ADDRESS` (preferred)
or legacy `MAPPED-ADDRESS`. The response source, magic cookie, message framing,
attribute bounds, address family, and 96-bit transaction ID are validated.

```c
RNetExternalIpv4Config stun;
char external_ip[RNET_IPV4_ADDRESS_TEXT_MAX];
rnet_external_ipv4_config_init(&stun);
stun.timeout_ms = 750;
if (rnet_external_ipv4_discover(&stun, external_ip,
                                sizeof(external_ip)) ==
    RNET_EXTERNAL_IPV4_OK) {
    /* Display external_ip; advertise only after port-forwarding is ready. */
}
```

Passing `NULL` for the configuration uses `stun.l.google.com:19302` with a
750 ms UDP deadline. Hosts can replace the hostname, port, and timeout, and
should do so when they operate their own STUN infrastructure. Positive timeout
values are clamped to 1000 ms because the API is synchronous and may be called
from a launcher render thread. UDP requests are retransmitted with the same
cryptographically generated transaction ID until the deadline. Hostname lookup
happens before that deadline and remains bounded by the platform DNS resolver;
cache the result and avoid calling discovery every frame.

STUN reports the observed IP address, not whether the chosen game port is
forwarded and reachable. ICE or explicit router configuration is still needed
for general Internet traversal. Never bind a local socket to the discovered
public/NAT address: bind `0.0.0.0:port` and advertise `external_ip:port` as two
separate endpoint values.

## Lobby advertise endpoint (bind + mapped port)

Online lobby hosts should discover a server-reflexive **UDP** endpoint from the
same local game port they answer list/waiting-room RTT on:

```c
RNetExternalIpv4Config stun;
char advertise[RNET_ENDPOINT_TEXT_MAX];
rnet_external_ipv4_config_init(&stun);
/* Prefer the lobby Coturn STUN when turn credentials are available. */
if (rnet_external_udp_endpoint_discover(&stun, "0.0.0.0:7777", advertise,
                                        sizeof(advertise)) ==
    RNET_EXTERNAL_IPV4_OK) {
    /* Publish advertise via WS set_host_endpoint for list RTT probes. */
}
```

This binds `bind_hostport`, so close any other owner of that port (e.g. the
waiting-room RTT probe) for the duration of the call. The result includes the
NAT **mapped port**, which may differ from the local bind port. List latency
remains best-effort; symmetric NAT and hairpin failures still show as unknown
until post-join ICE.
