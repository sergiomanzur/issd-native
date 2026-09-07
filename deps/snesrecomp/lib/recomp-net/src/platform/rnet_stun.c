/* Feature macros before any system headers (strict C11 / -std=c11). */
#if !defined(_WIN32)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#endif

#include "recomp_net/address.h"

#include "platform/rnet_platform.h"
#include "platform/rnet_stun_internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <netdb.h>
#endif

#define RNET_STUN_HEADER_SIZE 20U
#define RNET_STUN_MAGIC_COOKIE 0x2112a442U
#define RNET_STUN_BINDING_REQUEST 0x0001U
#define RNET_STUN_BINDING_SUCCESS 0x0101U
#define RNET_STUN_ATTR_MAPPED_ADDRESS 0x0001U
#define RNET_STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020U
#define RNET_STUN_RECV_CAPACITY 2048U
#define RNET_STUN_INITIAL_RTO_MS 250

static rnet_u16 read_be16(const rnet_u8 *data)
{
    return (rnet_u16)(((rnet_u16)data[0] << 8) | (rnet_u16)data[1]);
}

static rnet_u32 read_be32(const rnet_u8 *data)
{
    return ((rnet_u32)data[0] << 24) | ((rnet_u32)data[1] << 16) |
           ((rnet_u32)data[2] << 8) | (rnet_u32)data[3];
}

static void write_be16(rnet_u8 *data, rnet_u16 value)
{
    data[0] = (rnet_u8)(value >> 8);
    data[1] = (rnet_u8)value;
}

static void write_be32(rnet_u8 *data, rnet_u32 value)
{
    data[0] = (rnet_u8)(value >> 24);
    data[1] = (rnet_u8)(value >> 16);
    data[2] = (rnet_u8)(value >> 8);
    data[3] = (rnet_u8)value;
}

static int mapped_ipv4_is_valid(rnet_u32 address)
{
    rnet_u32 first = address >> 24;
    return first != 0U && first < 224U && address != 0xffffffffU;
}

int rnet_stun_parse_binding_response_ex(
    const rnet_u8 *packet, size_t packet_len,
    const rnet_u8 transaction_id[RNET_STUN_TRANSACTION_ID_SIZE],
    rnet_u32 *address_out, rnet_u16 *port_out)
{
    size_t message_len;
    size_t offset;
    rnet_u32 mapped_address = 0;
    rnet_u32 xor_mapped_address = 0;
    rnet_u16 mapped_port = 0;
    rnet_u16 xor_mapped_port = 0;
    int have_mapped = 0;
    int have_xor_mapped = 0;

    if (packet == NULL || transaction_id == NULL || address_out == NULL ||
        packet_len < RNET_STUN_HEADER_SIZE)
    {
        return RNET_STUN_PARSE_ERROR;
    }
    if (read_be16(packet) != RNET_STUN_BINDING_SUCCESS ||
        read_be32(packet + 4) != RNET_STUN_MAGIC_COOKIE)
    {
        return RNET_STUN_PARSE_ERROR;
    }
    if (memcmp(packet + 8, transaction_id,
               RNET_STUN_TRANSACTION_ID_SIZE) != 0)
    {
        return RNET_STUN_PARSE_TRANSACTION_MISMATCH;
    }
    message_len = (size_t)read_be16(packet + 2);
    if ((message_len & 3U) != 0U ||
        message_len != packet_len - RNET_STUN_HEADER_SIZE)
    {
        return RNET_STUN_PARSE_ERROR;
    }
    offset = RNET_STUN_HEADER_SIZE;
    while (offset < packet_len)
    {
        rnet_u16 type;
        size_t value_len;
        size_t padded_len;
        const rnet_u8 *value;

        if (packet_len - offset < 4U)
        {
            return RNET_STUN_PARSE_ERROR;
        }
        type = read_be16(packet + offset);
        value_len = (size_t)read_be16(packet + offset + 2U);
        offset += 4U;
        if (value_len > packet_len - offset || value_len > SIZE_MAX - 3U)
        {
            return RNET_STUN_PARSE_ERROR;
        }
        padded_len = (value_len + 3U) & ~(size_t)3U;
        if (padded_len > packet_len - offset)
        {
            return RNET_STUN_PARSE_ERROR;
        }
        value = packet + offset;
        if (type == RNET_STUN_ATTR_MAPPED_ADDRESS ||
            type == RNET_STUN_ATTR_XOR_MAPPED_ADDRESS)
        {
            rnet_u32 address;
            rnet_u16 port;
            if (value_len != 8U || value[0] != 0U || value[1] != 0x01U)
            {
                return RNET_STUN_PARSE_ERROR;
            }
            port = read_be16(value + 2U);
            address = read_be32(value + 4U);
            if (type == RNET_STUN_ATTR_XOR_MAPPED_ADDRESS)
            {
                port ^= (rnet_u16)(RNET_STUN_MAGIC_COOKIE >> 16);
                address ^= RNET_STUN_MAGIC_COOKIE;
                if (!mapped_ipv4_is_valid(address) || port == 0U)
                {
                    return RNET_STUN_PARSE_ERROR;
                }
                xor_mapped_address = address;
                xor_mapped_port = port;
                have_xor_mapped = 1;
            }
            else
            {
                if (!mapped_ipv4_is_valid(address) || port == 0U)
                {
                    return RNET_STUN_PARSE_ERROR;
                }
                mapped_address = address;
                mapped_port = port;
                have_mapped = 1;
            }
        }
        offset += padded_len;
    }
    if (have_xor_mapped)
    {
        *address_out = xor_mapped_address;
        if (port_out)
            *port_out = xor_mapped_port;
        return RNET_STUN_PARSE_OK;
    }
    if (have_mapped)
    {
        *address_out = mapped_address;
        if (port_out)
            *port_out = mapped_port;
        return RNET_STUN_PARSE_OK;
    }
    return RNET_STUN_PARSE_ERROR;
}

int rnet_stun_parse_binding_response(
    const rnet_u8 *packet, size_t packet_len,
    const rnet_u8 transaction_id[RNET_STUN_TRANSACTION_ID_SIZE],
    rnet_u32 *address_out)
{
    return rnet_stun_parse_binding_response_ex(packet, packet_len,
                                               transaction_id, address_out,
                                               NULL);
}

static int resolve_stun_server(const char *host, unsigned short port,
                               struct sockaddr_in *out)
{
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *result;
    char port_text[6];
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    snprintf(port_text, sizeof(port_text), "%u", (unsigned)port);
    rnet_os_startup();
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
        if (result->ai_family == AF_INET &&
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

static int sockaddr_ipv4_equal(const struct sockaddr_in *lhs,
                               const struct sockaddr_in *rhs)
{
    return lhs->sin_family == rhs->sin_family &&
           lhs->sin_port == rhs->sin_port &&
           lhs->sin_addr.s_addr == rhs->sin_addr.s_addr;
}

static void build_binding_request(
    rnet_u8 packet[RNET_STUN_HEADER_SIZE],
    const rnet_u8 transaction_id[RNET_STUN_TRANSACTION_ID_SIZE])
{
    memset(packet, 0, RNET_STUN_HEADER_SIZE);
    write_be16(packet, RNET_STUN_BINDING_REQUEST);
    write_be16(packet + 2, 0);
    write_be32(packet + 4, RNET_STUN_MAGIC_COOKIE);
    memcpy(packet + 8, transaction_id, RNET_STUN_TRANSACTION_ID_SIZE);
}

static void format_ipv4(rnet_u32 address,
                        char out[RNET_IPV4_ADDRESS_TEXT_MAX])
{
    snprintf(out, RNET_IPV4_ADDRESS_TEXT_MAX, "%u.%u.%u.%u",
             (unsigned)((address >> 24) & 0xffU),
             (unsigned)((address >> 16) & 0xffU),
             (unsigned)((address >> 8) & 0xffU),
             (unsigned)(address & 0xffU));
}

void rnet_external_ipv4_config_init(RNetExternalIpv4Config *config)
{
    if (config == NULL)
    {
        return;
    }
    config->stun_host = RNET_STUN_DEFAULT_HOST;
    config->stun_port = RNET_STUN_DEFAULT_PORT;
    config->timeout_ms = RNET_STUN_DEFAULT_TIMEOUT_MS;
}

static void stun_config_resolve(const RNetExternalIpv4Config *config,
                                const char **host_out, unsigned short *port_out,
                                int *timeout_out)
{
    const char *host = RNET_STUN_DEFAULT_HOST;
    unsigned short port = RNET_STUN_DEFAULT_PORT;
    int timeout_ms = RNET_STUN_DEFAULT_TIMEOUT_MS;

    if (config != NULL)
    {
        if (config->stun_host != NULL && config->stun_host[0] != '\0')
            host = config->stun_host;
        if (config->stun_port != 0U)
            port = config->stun_port;
        if (config->timeout_ms > 0)
            timeout_ms = config->timeout_ms;
    }
    if (timeout_ms > RNET_STUN_MAX_TIMEOUT_MS)
        timeout_ms = RNET_STUN_MAX_TIMEOUT_MS;
    *host_out = host;
    *port_out = port;
    *timeout_out = timeout_ms;
}

/* Bind NULL/empty → ephemeral. On success writes address+port (host order). */
static int stun_binding_discover(const RNetExternalIpv4Config *config,
                                 const char *bind_hostport, rnet_u32 *address_out,
                                 rnet_u16 *port_out)
{
    const char *host;
    unsigned short stun_port;
    int timeout_ms;
    struct sockaddr_in server;
    struct sockaddr_in bind_addr;
    rnet_socket sock = RNET_SOCKET_INVALID;
    rnet_u8 transaction_id[RNET_STUN_TRANSACTION_ID_SIZE];
    rnet_u8 request[RNET_STUN_HEADER_SIZE];
    rnet_u8 response[RNET_STUN_RECV_CAPACITY];
    rnet_u64 start_ms;
    rnet_u64 now_ms;
    int rto_ms = RNET_STUN_INITIAL_RTO_MS;
    int saw_protocol_error = 0;
    int result = RNET_EXTERNAL_IPV4_ERR_TIMEOUT;

    if (address_out == NULL || port_out == NULL)
        return RNET_EXTERNAL_IPV4_ERR_ARGUMENT;
    *address_out = 0;
    *port_out = 0;

    stun_config_resolve(config, &host, &stun_port, &timeout_ms);
    if (resolve_stun_server(host, stun_port, &server) != 0)
        return RNET_EXTERNAL_IPV4_ERR_RESOLVE;
    if (rnet_os_random_bytes(transaction_id, sizeof(transaction_id)) != 0)
        return RNET_EXTERNAL_IPV4_ERR_RANDOM;
    build_binding_request(request, transaction_id);

    sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(sock))
        return RNET_EXTERNAL_IPV4_ERR_SOCKET;
    (void)rnet_os_setsockopt_reuseaddr(sock, 1);
    (void)rnet_os_set_nonblocking(sock);

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = 0;
    if (bind_hostport && bind_hostport[0])
    {
        char bh[128];
        rnet_u16 bp = 0;
        if (rnet_os_parse_hostport(bind_hostport, bh, sizeof(bh), &bp) != 0 ||
            bp == 0)
        {
            rnet_os_socket_destroy(&sock);
            return RNET_EXTERNAL_IPV4_ERR_ARGUMENT;
        }
        if (rnet_os_resolve_sockaddr(bh[0] ? bh : "0.0.0.0", bp, &bind_addr) !=
            0)
        {
            rnet_os_socket_destroy(&sock);
            return RNET_EXTERNAL_IPV4_ERR_RESOLVE;
        }
    }
    if (rnet_os_bind(sock, &bind_addr) != 0)
    {
        rnet_os_socket_destroy(&sock);
        return RNET_EXTERNAL_IPV4_ERR_BIND;
    }

    start_ms = rnet_os_monotonic_ms();
    for (;;)
    {
        int remaining_ms;
        int wait_ms;
        int readable;

        now_ms = rnet_os_monotonic_ms();
        if (now_ms - start_ms >= (rnet_u64)timeout_ms)
            break;
        if (rnet_os_sendto(sock, request, sizeof(request), &server) < 0)
        {
            result = RNET_EXTERNAL_IPV4_ERR_SOCKET;
            break;
        }
        remaining_ms = timeout_ms - (int)(now_ms - start_ms);
        wait_ms = rto_ms < remaining_ms ? rto_ms : remaining_ms;
        for (;;)
        {
            struct sockaddr_in source;
            int would_block = 0;
            int received;
            rnet_u32 address = 0;
            rnet_u16 mapped_port = 0;
            int parsed;

            readable = rnet_os_poll_recv(sock, wait_ms);
            if (readable < 0)
            {
                result = RNET_EXTERNAL_IPV4_ERR_SOCKET;
                goto done;
            }
            if (readable == 0)
                break;
            memset(&source, 0, sizeof(source));
            received = rnet_os_recvfrom(sock, response, sizeof(response),
                                        &source, &would_block);
            if (received < 0)
            {
                if (would_block)
                    break;
                result = RNET_EXTERNAL_IPV4_ERR_SOCKET;
                goto done;
            }
            if (!sockaddr_ipv4_equal(&source, &server))
                continue;
            parsed = rnet_stun_parse_binding_response_ex(
                response, (size_t)received, transaction_id, &address,
                &mapped_port);
            if (parsed == RNET_STUN_PARSE_OK)
            {
                *address_out = address;
                *port_out = mapped_port;
                result = RNET_EXTERNAL_IPV4_OK;
                goto done;
            }
            if (parsed == RNET_STUN_PARSE_ERROR)
                saw_protocol_error = 1;
            now_ms = rnet_os_monotonic_ms();
            if (now_ms - start_ms >= (rnet_u64)timeout_ms)
                goto done;
            remaining_ms = timeout_ms - (int)(now_ms - start_ms);
            wait_ms = rto_ms < remaining_ms ? rto_ms : remaining_ms;
        }
        if (rto_ms < timeout_ms / 2)
            rto_ms *= 2;
        else
            rto_ms = timeout_ms;
    }
done:
    rnet_os_socket_destroy(&sock);
    if (result == RNET_EXTERNAL_IPV4_ERR_TIMEOUT && saw_protocol_error)
        return RNET_EXTERNAL_IPV4_ERR_PROTOCOL;
    return result;
}

int rnet_external_ipv4_discover(const RNetExternalIpv4Config *config,
                                char *out, size_t out_len)
{
    rnet_u32 address = 0;
    rnet_u16 port = 0;
    char text[RNET_IPV4_ADDRESS_TEXT_MAX];
    size_t text_len;
    int result;

    if (out == NULL || out_len == 0U)
        return RNET_EXTERNAL_IPV4_ERR_ARGUMENT;
    out[0] = '\0';
    result = stun_binding_discover(config, NULL, &address, &port);
    if (result != RNET_EXTERNAL_IPV4_OK)
        return result;
    format_ipv4(address, text);
    text_len = strlen(text);
    if (text_len + 1U > out_len)
        return RNET_EXTERNAL_IPV4_ERR_ARGUMENT;
    memcpy(out, text, text_len + 1U);
    return RNET_EXTERNAL_IPV4_OK;
}

int rnet_external_udp_endpoint_discover(const RNetExternalIpv4Config *config,
                                        const char *bind_hostport,
                                        char *out_endpoint,
                                        size_t out_endpoint_len)
{
    rnet_u32 address = 0;
    rnet_u16 port = 0;
    char text[RNET_IPV4_ADDRESS_TEXT_MAX];
    int n;
    int result;

    if (out_endpoint == NULL || out_endpoint_len == 0U || !bind_hostport ||
        !bind_hostport[0])
        return RNET_EXTERNAL_IPV4_ERR_ARGUMENT;
    out_endpoint[0] = '\0';
    result = stun_binding_discover(config, bind_hostport, &address, &port);
    if (result != RNET_EXTERNAL_IPV4_OK)
        return result;
    format_ipv4(address, text);
    n = snprintf(out_endpoint, out_endpoint_len, "%s:%u", text, (unsigned)port);
    if (n < 0 || (size_t)n >= out_endpoint_len)
    {
        out_endpoint[0] = '\0';
        return RNET_EXTERNAL_IPV4_ERR_ARGUMENT;
    }
    return RNET_EXTERNAL_IPV4_OK;
}
