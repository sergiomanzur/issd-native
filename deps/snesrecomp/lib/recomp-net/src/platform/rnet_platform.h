#ifndef RNET_PLATFORM_H
#define RNET_PLATFORM_H

#include "recomp_net/types.h"

#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET rnet_socket;
#define RNET_SOCKET_INVALID INVALID_SOCKET
#else
#include <netinet/in.h>
typedef int rnet_socket;
#define RNET_SOCKET_INVALID (-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void rnet_os_startup(void);
int rnet_os_socket_valid(rnet_socket s);
rnet_socket rnet_os_socket_create_dgram(void);
void rnet_os_socket_destroy(rnet_socket *sock_ptr);
int rnet_os_setsockopt_reuseaddr(rnet_socket s, int reuse_bool);
int rnet_os_setsockopt_broadcast(rnet_socket s, int broadcast_bool);
int rnet_os_setsockopt_recvbuf(rnet_socket s, int bytes);
int rnet_os_bind(rnet_socket s, const struct sockaddr_in *addr);
int rnet_os_set_nonblocking(rnet_socket s);
int rnet_os_recvfrom(rnet_socket s, void *buf, size_t len, struct sockaddr_in *src_out, int *would_block_out);
int rnet_os_sendto(rnet_socket s, const void *buf, size_t len, const struct sockaddr_in *dst);
rnet_u64 rnet_os_wall_ms(void);
rnet_u64 rnet_os_monotonic_ms(void);
void rnet_os_sleep_micros(unsigned usec);
/* Block until sock is readable or timeout_ms elapses. 1=readable, 0=timeout, -1=error/invalid. */
int rnet_os_poll_recv(rnet_socket s, int timeout_ms);
int rnet_os_last_error(void);
int rnet_os_random_bytes(void *out, size_t len);

/* Parse "host:port" or ":port". host_out may be NULL. Returns 0 on success. */
int rnet_os_parse_hostport(const char *spec, char *host_out, size_t host_cap, rnet_u16 *port_out);
/* Resolve host to IPv4 sockaddr. Accepts dotted quads and DNS hostnames
 * (getaddrinfo). Empty / "0.0.0.0" → INADDR_ANY. Returns 0 on success. */
int rnet_os_resolve_sockaddr(const char *host, rnet_u16 port, struct sockaddr_in *out);

#ifdef __cplusplus
}
#endif

#endif /* RNET_PLATFORM_H */
