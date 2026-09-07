/* Feature macros before any system headers (strict C11 / -std=c11). */
#if !defined(_WIN32)
#if defined(__APPLE__)
#if !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#else
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#endif
#endif

#include "recomp_net/address.h"

#include "platform/rnet_address_internal.h"
#include "platform/rnet_platform.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <iphlpapi.h>
#include <windows.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#endif

typedef struct CandidateList {
    RNetIpv4Candidate *items;
    size_t count;
    size_t capacity;
} CandidateList;

typedef struct RankedCandidate {
    RNetIpv4Candidate candidate;
    int preferred;
    int address_class;
} RankedCandidate;

static int candidate_list_append(CandidateList *list, rnet_u32 address,
                                 const char *interface_label)
{
    RNetIpv4Candidate *next;
    size_t next_capacity;

    if (list == NULL)
    {
        return -1;
    }
    if (list->count == list->capacity)
    {
        next_capacity = list->capacity ? list->capacity * 2U : 16U;
        if (next_capacity < list->capacity ||
            next_capacity > SIZE_MAX / sizeof(*list->items))
        {
            return -1;
        }
        next = (RNetIpv4Candidate *)realloc(
            list->items, next_capacity * sizeof(*list->items));
        if (next == NULL)
        {
            return -1;
        }
        list->items = next;
        list->capacity = next_capacity;
    }
    list->items[list->count].address = address;
    snprintf(list->items[list->count].interface_label,
             sizeof(list->items[list->count].interface_label), "%s",
             (interface_label != NULL && interface_label[0] != '\0')
                 ? interface_label : "Unnamed interface");
    list->count++;
    return 0;
}

static int ipv4_is_usable(rnet_u32 address)
{
    rnet_u32 first = address >> 24;

    /* Exclude unspecified/this-network, multicast and reserved. Loopback is a
     * valid final-ranked choice for two instances on one machine. */
    return first != 0U && first < 224U && address != 0xffffffffU;
}

static int ipv4_address_class(rnet_u32 address)
{
    /* RFC 1918 private ranges are the most likely same-LAN choices. */
    if ((address & 0xff000000U) == 0x0a000000U ||
        (address & 0xfff00000U) == 0xac100000U ||
        (address & 0xffff0000U) == 0xc0a80000U)
    {
        return 0;
    }
    /* RFC 6598 shared space is commonly used by VPNs and carrier networks. */
    if ((address & 0xffc00000U) == 0x64400000U)
    {
        return 1;
    }
    /* RFC 3927 link-local is usable, but only on the attached link. */
    if ((address & 0xffff0000U) == 0xa9fe0000U)
    {
        return 3;
    }
    if ((address & 0xff000000U) == 0x7f000000U)
    {
        return 4;
    }
    return 2;
}

static int ranked_candidate_compare(const void *lhs_ptr, const void *rhs_ptr)
{
    const RankedCandidate *lhs = (const RankedCandidate *)lhs_ptr;
    const RankedCandidate *rhs = (const RankedCandidate *)rhs_ptr;

    if (lhs->preferred != rhs->preferred)
    {
        return rhs->preferred - lhs->preferred;
    }
    if (lhs->address_class != rhs->address_class)
    {
        return lhs->address_class - rhs->address_class;
    }
    if (lhs->candidate.address < rhs->candidate.address)
    {
        return -1;
    }
    if (lhs->candidate.address > rhs->candidate.address)
    {
        return 1;
    }
    return strcmp(lhs->candidate.interface_label,
                  rhs->candidate.interface_label);
}

static void ipv4_format(rnet_u32 address, char out[RNET_IPV4_ADDRESS_TEXT_MAX])
{
    snprintf(out, RNET_IPV4_ADDRESS_TEXT_MAX, "%u.%u.%u.%u",
             (unsigned)((address >> 24) & 0xffU),
             (unsigned)((address >> 16) & 0xffU),
             (unsigned)((address >> 8) & 0xffU),
             (unsigned)(address & 0xffU));
}

int rnet_ipv4_normalize_candidates(const RNetIpv4Candidate *candidates,
                                   size_t candidate_count,
                                   rnet_u32 preferred_address,
                                   RNetIpv4Address *out,
                                   size_t capacity)
{
    RankedCandidate *ranked;
    size_t ranked_count = 0;
    size_t unique_count = 0;
    size_t i;
    rnet_u32 last_address = 0;
    int have_last = 0;

    if ((candidate_count != 0U && candidates == NULL) ||
        (capacity != 0U && out == NULL) || candidate_count > (size_t)INT_MAX ||
        candidate_count > SIZE_MAX / sizeof(*ranked))
    {
        return -1;
    }
    ranked = candidate_count
        ? (RankedCandidate *)malloc(candidate_count * sizeof(*ranked)) : NULL;
    if (candidate_count != 0U && ranked == NULL)
    {
        return -1;
    }
    for (i = 0; i < candidate_count; ++i)
    {
        if (!ipv4_is_usable(candidates[i].address))
        {
            continue;
        }
        ranked[ranked_count].candidate = candidates[i];
        ranked[ranked_count].candidate.interface_label[
            RNET_INTERFACE_LABEL_MAX - 1] = '\0';
        ranked[ranked_count].preferred =
            preferred_address != 0U && candidates[i].address == preferred_address;
        ranked[ranked_count].address_class =
            ipv4_address_class(candidates[i].address);
        ranked_count++;
    }
    if (ranked_count > 1U)
    {
        qsort(ranked, ranked_count, sizeof(*ranked), ranked_candidate_compare);
    }
    for (i = 0; i < ranked_count; ++i)
    {
        if (have_last && ranked[i].candidate.address == last_address)
        {
            continue;
        }
        last_address = ranked[i].candidate.address;
        have_last = 1;
        if (unique_count < capacity)
        {
            ipv4_format(last_address, out[unique_count].address);
            snprintf(out[unique_count].interface_label,
                     sizeof(out[unique_count].interface_label), "%s",
                     ranked[i].candidate.interface_label[0]
                         ? ranked[i].candidate.interface_label
                         : "Unnamed interface");
        }
        unique_count++;
    }
    free(ranked);
    return unique_count <= (size_t)INT_MAX ? (int)unique_count : -1;
}

static rnet_u32 default_route_source(void)
{
    rnet_socket sock;
    struct sockaddr_in destination;
    struct sockaddr_in local;
#ifdef _WIN32
    int local_length = (int)sizeof(local);
#else
    socklen_t local_length = (socklen_t)sizeof(local);
#endif

    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(53);
    destination.sin_addr.s_addr = htonl(0x08080808U);
    memset(&local, 0, sizeof(local));
    sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(sock))
    {
        return 0;
    }
#ifdef _WIN32
    if (connect(sock, (const struct sockaddr *)&destination,
                (int)sizeof(destination)) != 0 ||
        getsockname(sock, (struct sockaddr *)&local, &local_length) != 0)
#else
    if (connect(sock, (const struct sockaddr *)&destination,
                (socklen_t)sizeof(destination)) != 0 ||
        getsockname(sock, (struct sockaddr *)&local, &local_length) != 0)
#endif
    {
        rnet_os_socket_destroy(&sock);
        return 0;
    }
    rnet_os_socket_destroy(&sock);
    return ntohl(local.sin_addr.s_addr);
}

#ifdef _WIN32
static void windows_interface_label(const IP_ADAPTER_ADDRESSES *adapter,
                                    char out[RNET_INTERFACE_LABEL_MAX])
{
    out[0] = '\0';
    if (adapter->FriendlyName != NULL && adapter->FriendlyName[0] != L'\0')
    {
        if (WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, out,
                                RNET_INTERFACE_LABEL_MAX, NULL, NULL) > 0)
        {
            out[RNET_INTERFACE_LABEL_MAX - 1] = '\0';
            return;
        }
    }
    snprintf(out, RNET_INTERFACE_LABEL_MAX, "%s",
             adapter->AdapterName != NULL ? adapter->AdapterName
                                          : "Unnamed interface");
}

static int collect_platform_candidates(CandidateList *list)
{
    IP_ADAPTER_ADDRESSES *addresses = NULL;
    IP_ADAPTER_ADDRESSES *adapter;
    ULONG buffer_size = 15U * 1024U;
    ULONG result;
    int attempts;

    for (attempts = 0; attempts < 3; ++attempts)
    {
        addresses = (IP_ADAPTER_ADDRESSES *)malloc(buffer_size);
        if (addresses == NULL)
        {
            return -1;
        }
        result = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_DNS_SERVER,
            NULL, addresses, &buffer_size);
        if (result != ERROR_BUFFER_OVERFLOW)
        {
            break;
        }
        free(addresses);
        addresses = NULL;
    }
    if (addresses != NULL && result == ERROR_NO_DATA)
    {
        free(addresses);
        return 0;
    }
    if (addresses == NULL || result != NO_ERROR)
    {
        free(addresses);
        return -1;
    }
    for (adapter = addresses; adapter != NULL; adapter = adapter->Next)
    {
        IP_ADAPTER_UNICAST_ADDRESS *unicast;
        char label[RNET_INTERFACE_LABEL_MAX];

        if (adapter->OperStatus != IfOperStatusUp)
        {
            continue;
        }
        windows_interface_label(adapter, label);
        for (unicast = adapter->FirstUnicastAddress; unicast != NULL;
             unicast = unicast->Next)
        {
            const struct sockaddr_in *address;
            if (unicast->Address.lpSockaddr == NULL ||
                unicast->Address.lpSockaddr->sa_family != AF_INET ||
                unicast->DadState != IpDadStatePreferred)
            {
                continue;
            }
            address = (const struct sockaddr_in *)unicast->Address.lpSockaddr;
            if (candidate_list_append(list, ntohl(address->sin_addr.s_addr),
                                      label) != 0)
            {
                free(addresses);
                return -1;
            }
        }
    }
    free(addresses);
    return 0;
}
#else
static int collect_platform_candidates(CandidateList *list)
{
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *interface;

    if (getifaddrs(&interfaces) != 0)
    {
        return -1;
    }
    for (interface = interfaces; interface != NULL; interface = interface->ifa_next)
    {
        const struct sockaddr_in *address;
        if (interface->ifa_addr == NULL ||
            interface->ifa_addr->sa_family != AF_INET ||
            (interface->ifa_flags & IFF_UP) == 0)
        {
            continue;
        }
        address = (const struct sockaddr_in *)interface->ifa_addr;
        if (candidate_list_append(list, ntohl(address->sin_addr.s_addr),
                                  interface->ifa_name) != 0)
        {
            freeifaddrs(interfaces);
            return -1;
        }
    }
    freeifaddrs(interfaces);
    return 0;
}
#endif

int rnet_ipv4_enumerate(RNetIpv4Address *out, size_t capacity)
{
    CandidateList candidates;
    int result;

    if (capacity != 0U && out == NULL)
    {
        return -1;
    }
    memset(&candidates, 0, sizeof(candidates));
    if (collect_platform_candidates(&candidates) != 0)
    {
        free(candidates.items);
        return -1;
    }
    result = rnet_ipv4_normalize_candidates(
        candidates.items, candidates.count, default_route_source(), out, capacity);
    free(candidates.items);
    return result;
}

int rnet_udp_port_available(int port)
{
    rnet_socket sock;
    struct sockaddr_in addr;
    int ok;

    if (port <= 0 || port > 65535)
    {
        return 0;
    }
    sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(sock))
    {
        return 0;
    }
    /* Intentionally no SO_REUSEADDR — a second host on the same lobby port
     * must fail so create can surface "port in use". */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    ok = (rnet_os_bind(sock, &addr) == 0) ? 1 : 0;
    rnet_os_socket_destroy(&sock);
    return ok;
}

int rnet_udp_find_free_port(int preferred, int span)
{
    int i;

    if (preferred <= 0 || preferred > 65535)
    {
        preferred = 7777;
    }
    if (span <= 0)
    {
        span = 32;
    }
    for (i = 0; i < span; ++i)
    {
        const int port = preferred + i;
        if (port > 65535)
        {
            break;
        }
        if (rnet_udp_port_available(port))
        {
            return port;
        }
    }
    return -1;
}

int rnet_endpoint_set_port(char *endpoint, size_t cap, int port)
{
    char host[64];
    char parsed_host[64];
    rnet_u16 parsed_port = 0;
    int n;

    if (endpoint == NULL || cap < 4U || port <= 0 || port > 65535)
    {
        return -1;
    }
    if (rnet_os_parse_hostport(endpoint, parsed_host, sizeof(parsed_host),
                               &parsed_port) == 0 &&
        parsed_host[0] != '\0')
    {
        snprintf(host, sizeof(host), "%s", parsed_host);
    }
    else if (endpoint[0] == '\0' || endpoint[0] == ':')
    {
        snprintf(host, sizeof(host), "0.0.0.0");
    }
    else
    {
        const char *colon = strrchr(endpoint, ':');
        size_t host_len;
        if (colon == NULL)
        {
            snprintf(host, sizeof(host), "%s", endpoint);
        }
        else
        {
            host_len = (size_t)(colon - endpoint);
            if (host_len >= sizeof(host))
            {
                host_len = sizeof(host) - 1U;
            }
            memcpy(host, endpoint, host_len);
            host[host_len] = '\0';
        }
        if (host[0] == '\0')
        {
            snprintf(host, sizeof(host), "0.0.0.0");
        }
    }
    n = snprintf(endpoint, cap, "%s:%d", host, port);
    return (n > 0 && (size_t)n < cap) ? 0 : -1;
}
