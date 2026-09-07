/* Feature macros before any system headers (strict C11 / -std=c11). */
#if !defined(_WIN32)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#endif

#include "rnet_platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <bcrypt.h>
#include <windows.h>

static int s_wsa_started;
static LARGE_INTEGER s_win_monotonic_freq;
static int s_win_monotonic_freq_init;
static HANDLE s_win_sleep_timer;
static int s_win_sleep_timer_init;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

/* High-res waitable timer on Win10+ (else auto-reset). Avoids Sleep()
 * ~1–15.6 ms granularity that stretches admit/ICE wait_recv slices. */
static void rnet_os_sleep_timer_ensure(void)
{
    if (s_win_sleep_timer_init != 0)
    {
        return;
    }
    s_win_sleep_timer_init = 1;
    s_win_sleep_timer =
        CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (s_win_sleep_timer == NULL)
    {
        s_win_sleep_timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    }
}

void rnet_os_startup(void)
{
    WSADATA wsa;

    if (s_wsa_started != 0)
    {
        return;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        return;
    }
    s_wsa_started = 1;
}

int rnet_os_socket_valid(rnet_socket s)
{
    return (s != RNET_SOCKET_INVALID) ? 1 : 0;
}

rnet_socket rnet_os_socket_create_dgram(void)
{
    rnet_os_startup();
    return socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

void rnet_os_socket_destroy(rnet_socket *sock_ptr)
{
    if ((sock_ptr == NULL) || (*sock_ptr == RNET_SOCKET_INVALID))
    {
        return;
    }
    closesocket(*sock_ptr);
    *sock_ptr = RNET_SOCKET_INVALID;
}

int rnet_os_setsockopt_reuseaddr(rnet_socket s, int reuse_bool)
{
    int v = reuse_bool ? 1 : 0;
    return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&v, (int)sizeof(v));
}

int rnet_os_setsockopt_broadcast(rnet_socket s, int broadcast_bool)
{
    int v = broadcast_bool ? 1 : 0;
    return setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&v, (int)sizeof(v));
}

int rnet_os_setsockopt_recvbuf(rnet_socket s, int bytes)
{
    return setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&bytes, (int)sizeof(bytes));
}

int rnet_os_bind(rnet_socket s, const struct sockaddr_in *addr)
{
    return bind(s, (const struct sockaddr *)addr, (int)sizeof(*addr));
}

int rnet_os_set_nonblocking(rnet_socket s)
{
    u_long mode = 1UL;
    return ioctlsocket(s, FIONBIO, &mode);
}

int rnet_os_recvfrom(rnet_socket s, void *buf, size_t len, struct sockaddr_in *src_out, int *would_block_out)
{
    int r;
    int src_len = (int)sizeof(*src_out);

    if (would_block_out != NULL)
    {
        *would_block_out = 0;
    }
    r = recvfrom(s, (char *)buf, (int)len, 0, (struct sockaddr *)src_out, src_out ? &src_len : NULL);
    if (r == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        if ((e == WSAEWOULDBLOCK) && (would_block_out != NULL))
        {
            *would_block_out = 1;
        }
        return -1;
    }
    return r;
}

int rnet_os_sendto(rnet_socket s, const void *buf, size_t len, const struct sockaddr_in *dst)
{
    int r = sendto(s, (const char *)buf, (int)len, 0, (const struct sockaddr *)dst, (int)sizeof(*dst));
    if (r == SOCKET_ERROR)
    {
        return -1;
    }
    return r;
}

rnet_u64 rnet_os_wall_ms(void)
{
    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (rnet_u64)((uli.QuadPart - 116444736000000000ULL) / 10000ULL);
}

rnet_u64 rnet_os_monotonic_ms(void)
{
    LARGE_INTEGER counter;

    if (s_win_monotonic_freq_init == 0)
    {
        if (QueryPerformanceFrequency(&s_win_monotonic_freq) == 0)
        {
            s_win_monotonic_freq.QuadPart = 0;
        }
        s_win_monotonic_freq_init = 1;
    }
    if (s_win_monotonic_freq.QuadPart == 0)
    {
        return (rnet_u64)GetTickCount64();
    }
    if (QueryPerformanceCounter(&counter) == 0)
    {
        return (rnet_u64)GetTickCount64();
    }
    return (rnet_u64)((counter.QuadPart * 1000ULL) / (rnet_u64)s_win_monotonic_freq.QuadPart);
}

void rnet_os_sleep_micros(unsigned usec)
{
    LARGE_INTEGER due;
    LONGLONG hundred_ns;

    if (usec == 0U)
    {
        return;
    }

    rnet_os_sleep_timer_ensure();
    if (s_win_sleep_timer != NULL)
    {
        /* Relative due time: negative 100-ns units. */
        hundred_ns = -((LONGLONG)usec * 10LL);
        if (hundred_ns >= 0)
        {
            hundred_ns = -1LL;
        }
        due.QuadPart = hundred_ns;
        if (SetWaitableTimer(s_win_sleep_timer, &due, 0, NULL, NULL, FALSE) != 0)
        {
            (void)WaitForSingleObject(s_win_sleep_timer, INFINITE);
            return;
        }
    }

    /* Fallback when waitable timer is unavailable or SetWaitableTimer fails. */
    if (usec < 1000U)
    {
        Sleep(1U);
        return;
    }
    Sleep((DWORD)(usec / 1000U));
}

int rnet_os_poll_recv(rnet_socket s, int timeout_ms)
{
    WSAPOLLFD pfd;
    int r;

    if (!rnet_os_socket_valid(s))
    {
        return -1;
    }
    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }
    pfd.fd = s;
    pfd.events = POLLIN;
    pfd.revents = 0;
    r = WSAPoll(&pfd, 1, timeout_ms);
    if (r == SOCKET_ERROR)
    {
        return -1;
    }
    if (r == 0)
    {
        return 0;
    }
    return ((pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0) ? 1 : 0;
}

int rnet_os_last_error(void)
{
    return (int)WSAGetLastError();
}

int rnet_os_random_bytes(void *out, size_t len)
{
    unsigned char *bytes = (unsigned char *)out;

    if (len != 0U && out == NULL)
    {
        return -1;
    }
    while (len != 0U)
    {
        ULONG chunk = len > (size_t)ULONG_MAX ? ULONG_MAX : (ULONG)len;
        if (BCryptGenRandom(NULL, bytes, chunk,
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        {
            return -1;
        }
        bytes += chunk;
        len -= chunk;
    }
    return 0;
}

#else /* !_WIN32 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void rnet_os_startup(void)
{
}

int rnet_os_socket_valid(rnet_socket s)
{
    return (s >= 0) ? 1 : 0;
}

rnet_socket rnet_os_socket_create_dgram(void)
{
    rnet_os_startup();
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void rnet_os_socket_destroy(rnet_socket *sock_ptr)
{
    if ((sock_ptr == NULL) || (*sock_ptr < 0))
    {
        return;
    }
    close(*sock_ptr);
    *sock_ptr = RNET_SOCKET_INVALID;
}

int rnet_os_setsockopt_reuseaddr(rnet_socket s, int reuse_bool)
{
    int v = reuse_bool;
    return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &v, (socklen_t)sizeof(v));
}

int rnet_os_setsockopt_broadcast(rnet_socket s, int broadcast_bool)
{
    int v = broadcast_bool ? 1 : 0;
    return setsockopt(s, SOL_SOCKET, SO_BROADCAST, &v, (socklen_t)sizeof(v));
}

int rnet_os_setsockopt_recvbuf(rnet_socket s, int bytes)
{
    return setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bytes, (socklen_t)sizeof(bytes));
}

int rnet_os_bind(rnet_socket s, const struct sockaddr_in *addr)
{
    return bind(s, (const struct sockaddr *)addr, (socklen_t)sizeof(*addr));
}

int rnet_os_set_nonblocking(rnet_socket s)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
    {
        return -1;
    }
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

int rnet_os_recvfrom(rnet_socket s, void *buf, size_t len, struct sockaddr_in *src_out, int *would_block_out)
{
    ssize_t r;
    socklen_t src_len = (socklen_t)sizeof(*src_out);

    if (would_block_out != NULL)
    {
        *would_block_out = 0;
    }
    r = recvfrom(s, buf, len, 0, (struct sockaddr *)src_out, src_out ? &src_len : NULL);
    if (r < 0)
    {
        if (((errno == EAGAIN) || (errno == EWOULDBLOCK)) && (would_block_out != NULL))
        {
            *would_block_out = 1;
        }
        return -1;
    }
    return (int)r;
}

int rnet_os_sendto(rnet_socket s, const void *buf, size_t len, const struct sockaddr_in *dst)
{
    ssize_t r = sendto(s, buf, len, 0, (const struct sockaddr *)dst, (socklen_t)sizeof(*dst));
    if (r < 0)
    {
        return -1;
    }
    return (int)r;
}

rnet_u64 rnet_os_wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        return 0ULL;
    }
    return (rnet_u64)ts.tv_sec * 1000ULL + (rnet_u64)(ts.tv_nsec / 1000000L);
}

rnet_u64 rnet_os_monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0ULL;
    }
    return (rnet_u64)ts.tv_sec * 1000ULL + (rnet_u64)(ts.tv_nsec / 1000000L);
}

void rnet_os_sleep_micros(unsigned usec)
{
    if (usec == 0U)
    {
        return;
    }
    usleep(usec);
}

int rnet_os_poll_recv(rnet_socket s, int timeout_ms)
{
    struct pollfd pfd;
    int r;

    if (!rnet_os_socket_valid(s))
    {
        return -1;
    }
    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }
    pfd.fd = s;
    pfd.events = POLLIN;
    pfd.revents = 0;
    r = poll(&pfd, 1, timeout_ms);
    if (r < 0)
    {
        return -1;
    }
    if (r == 0)
    {
        return 0;
    }
    return ((pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0) ? 1 : 0;
}

int rnet_os_last_error(void)
{
    return errno;
}

int rnet_os_random_bytes(void *out, size_t len)
{
    unsigned char *bytes = (unsigned char *)out;
    int fd;

    if (len != 0U && out == NULL)
    {
        return -1;
    }
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }
    while (len != 0U)
    {
        ssize_t count = read(fd, bytes, len);
        if (count < 0 && errno == EINTR)
        {
            continue;
        }
        if (count <= 0)
        {
            close(fd);
            return -1;
        }
        bytes += (size_t)count;
        len -= (size_t)count;
    }
    close(fd);
    return 0;
}

#endif /* _WIN32 */

int rnet_os_parse_hostport(const char *spec, char *host_out, size_t host_cap, rnet_u16 *port_out)
{
    const char *colon;
    char *port_end;
    size_t host_len;
    unsigned long port;

    if ((spec == NULL) || (port_out == NULL))
    {
        return -1;
    }
    colon = strrchr(spec, ':');
    if (colon == NULL)
    {
        return -1;
    }
    host_len = (size_t)(colon - spec);
    if (host_out != NULL && host_cap > 0)
    {
        if (host_len >= host_cap)
        {
            return -1;
        }
        if (host_len == 0)
        {
            snprintf(host_out, host_cap, "%s", "0.0.0.0");
        }
        else
        {
            memcpy(host_out, spec, host_len);
            host_out[host_len] = '\0';
        }
    }
    if (colon[1] == '\0')
    {
        return -1;
    }
    port = strtoul(colon + 1, &port_end, 10);
    if (*port_end != '\0' || port > 65535UL)
    {
        return -1;
    }
    *port_out = (rnet_u16)port;
    return 0;
}

int rnet_os_resolve_sockaddr(const char *host, rnet_u16 port, struct sockaddr_in *out)
{
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *result;
    char port_text[8];
    int status;

    if (out == NULL)
    {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(port);
    if ((host == NULL) || (host[0] == '\0') || strcmp(host, "0.0.0.0") == 0)
    {
        out->sin_addr.s_addr = htonl(INADDR_ANY);
        return 0;
    }

    /* Literal IPv4 — avoid DNS for dotted quads / bind wildcards. */
#ifdef _WIN32
    if (InetPtonA(AF_INET, host, &out->sin_addr) == 1)
    {
        return 0;
    }
#else
    if (inet_pton(AF_INET, host, &out->sin_addr) == 1)
    {
        return 0;
    }
#endif

    /* Hostname (lobby URL host, Coturn, etc.). */
    rnet_os_startup();
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    snprintf(port_text, sizeof(port_text), "%u", (unsigned)port);
    status = getaddrinfo(host, port_text, &hints, &results);
    if (status != 0 || results == NULL)
    {
        if (results != NULL)
        {
            freeaddrinfo(results);
        }
        return -1;
    }
    status = -1;
    for (result = results; result != NULL; result = result->ai_next)
    {
        if (result->ai_family == AF_INET && result->ai_addr != NULL &&
            (size_t)result->ai_addrlen >= sizeof(*out))
        {
            memcpy(out, result->ai_addr, sizeof(*out));
            status = 0;
            break;
        }
    }
    freeaddrinfo(results);
    return status;
}
