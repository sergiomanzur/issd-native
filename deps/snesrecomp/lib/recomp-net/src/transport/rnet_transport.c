#include "rnet_transport.h"

#include "protocol/rnet_protocol.h"

#include <string.h>

#define RNET_HUB_MIN_PACKET 14 /* magic(4)+type(2)+session(4)+checksum(4) */
#define RNET_HUB_HEADER_LEN 10

void rnet_transport_init(RNetTransport *t)
{
    if (t == NULL)
    {
        return;
    }
    memset(t, 0, sizeof(*t));
    t->sock = RNET_SOCKET_INVALID;
    t->mode = RNET_TRANSPORT_NONE;
}

void rnet_transport_shutdown(RNetTransport *t)
{
    if (t == NULL)
    {
        return;
    }
    if (t->mode == RNET_TRANSPORT_LAN_UDP)
    {
        rnet_os_socket_destroy(&t->sock);
    }
    t->mode = RNET_TRANSPORT_NONE;
    t->ice_send = NULL;
    t->ice_recv = NULL;
    t->ice_ctx = NULL;
    t->peer_known = 0;
    t->pending_peer_known = 0;
    t->accept_first_peer = 0;
    t->hub_mode = 0;
    memset(t->hub_slot_known, 0, sizeof(t->hub_slot_known));
    memset(t->hub_slot_addr, 0, sizeof(t->hub_slot_addr));
}

static int sockaddr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return a->sin_family == b->sin_family && a->sin_port == b->sin_port &&
           a->sin_addr.s_addr == b->sin_addr.s_addr;
}

static int lan_bind_socket(RNetTransport *t, const char *bind_hostport)
{
    char host[128];
    rnet_u16 port = 0;
    struct sockaddr_in bind_addr;

    if ((t == NULL) || (bind_hostport == NULL))
    {
        return -1;
    }
    rnet_transport_shutdown(t);
    rnet_os_startup();

    if (rnet_os_parse_hostport(bind_hostport, host, sizeof(host), &port) != 0)
    {
        return -1;
    }
    if (rnet_os_resolve_sockaddr(host, port, &bind_addr) != 0)
    {
        return -1;
    }

    t->sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(t->sock))
    {
        return -1;
    }
    (void)rnet_os_setsockopt_reuseaddr(t->sock, 1);
    (void)rnet_os_setsockopt_recvbuf(t->sock, 256 * 1024);
    if (rnet_os_bind(t->sock, &bind_addr) != 0)
    {
        rnet_os_socket_destroy(&t->sock);
        return -1;
    }
    if (rnet_os_set_nonblocking(t->sock) != 0)
    {
        rnet_os_socket_destroy(&t->sock);
        return -1;
    }
    t->mode = RNET_TRANSPORT_LAN_UDP;
    return 0;
}

int rnet_transport_start_lan(RNetTransport *t, const char *bind_hostport, const char *peer_hostport)
{
    char host[128];
    rnet_u16 port = 0;

    if (lan_bind_socket(t, bind_hostport) != 0)
    {
        return -1;
    }

    if (peer_hostport != NULL && peer_hostport[0] != '\0')
    {
        if (rnet_os_parse_hostport(peer_hostport, host, sizeof(host), &port) != 0 || port == 0)
        {
            rnet_os_socket_destroy(&t->sock);
            t->mode = RNET_TRANSPORT_NONE;
            return -1;
        }
        if (rnet_os_resolve_sockaddr(host, port, &t->peer) != 0)
        {
            rnet_os_socket_destroy(&t->sock);
            t->mode = RNET_TRANSPORT_NONE;
            return -1;
        }
        t->peer_known = 1;
    }
    else
    {
        t->peer_known = 0;
        t->accept_first_peer = 1;
    }
    return 0;
}

int rnet_transport_start_lan_hub(RNetTransport *t, const char *bind_hostport)
{
    if (lan_bind_socket(t, bind_hostport) != 0)
    {
        return -1;
    }
    t->hub_mode = 1;
    t->peer_known = 0;
    t->accept_first_peer = 0;
    return 0;
}

static int hub_packet_local_slot(const rnet_u8 *buf, int n, int *slot_out)
{
    rnet_u16 pkt_type;

    if (slot_out == NULL || n < RNET_HUB_MIN_PACKET)
    {
        return 0;
    }
    pkt_type = (rnet_u16)(buf[4] | ((rnet_u16)buf[5] << 8));
    /* START / DELAY_SYNC have no local_slot at body[0]. */
    if (pkt_type == RNET_PKT_START || pkt_type == RNET_PKT_DELAY_SYNC)
    {
        return 0;
    }
    *slot_out = (int)buf[RNET_HUB_HEADER_LEN];
    return 1;
}

static void hub_learn_and_forward(RNetTransport *t, const rnet_u8 *buf, int n, const struct sockaddr_in *src)
{
    int slot = -1;
    int i;

    if (t == NULL || buf == NULL || src == NULL || n < RNET_HUB_MIN_PACKET)
    {
        return;
    }

    if (hub_packet_local_slot(buf, n, &slot))
    {
        if (slot >= 0 && slot < RNET_MAX_SLOTS)
        {
            t->hub_slot_addr[slot] = *src;
            t->hub_slot_known[slot] = 1;
            t->peer_known = 1;
        }
    }
    else
    {
        /* Map by source if already known (START / DELAY_SYNC). */
        for (i = 0; i < RNET_MAX_SLOTS; ++i)
        {
            if (t->hub_slot_known[i] && sockaddr_equal(&t->hub_slot_addr[i], src))
            {
                slot = i;
                break;
            }
        }
    }

    for (i = 0; i < RNET_MAX_SLOTS; ++i)
    {
        if (!t->hub_slot_known[i])
        {
            continue;
        }
        if (sockaddr_equal(&t->hub_slot_addr[i], src))
        {
            continue;
        }
        (void)rnet_os_sendto(t->sock, buf, (size_t)n, &t->hub_slot_addr[i]);
    }
}

int rnet_transport_send(RNetTransport *t, const rnet_u8 *buf, size_t len)
{
    if ((t == NULL) || (buf == NULL) || (len == 0))
    {
        return -1;
    }
    if (t->mode == RNET_TRANSPORT_LAN_UDP)
    {
        int i;
        int sent = 0;
        int any = 0;

        if (!rnet_os_socket_valid(t->sock))
        {
            return -1;
        }
        if (t->hub_mode)
        {
            for (i = 0; i < RNET_MAX_SLOTS; ++i)
            {
                int r;
                if (!t->hub_slot_known[i])
                {
                    continue;
                }
                /* Never send host traffic back to seat 0 binding. */
                if (i == 0)
                {
                    continue;
                }
                r = rnet_os_sendto(t->sock, buf, len, &t->hub_slot_addr[i]);
                if (r >= 0)
                {
                    sent = r;
                    any = 1;
                }
            }
            return any ? sent : 0;
        }
        if (!t->peer_known)
        {
            return -1;
        }
        return rnet_os_sendto(t->sock, buf, len, &t->peer);
    }
    if (t->mode == RNET_TRANSPORT_ICE)
    {
        if (t->ice_send == NULL)
        {
            return -1;
        }
        return t->ice_send(t->ice_ctx, buf, len);
    }
    return -1;
}

int rnet_transport_recv(RNetTransport *t, rnet_u8 *buf, size_t cap)
{
    int would_block = 0;
    int n;
    struct sockaddr_in src;

    if ((t == NULL) || (buf == NULL) || (cap == 0))
    {
        return -1;
    }
    if (t->mode == RNET_TRANSPORT_LAN_UDP)
    {
        if (!rnet_os_socket_valid(t->sock))
        {
            return -1;
        }
        for (;;)
        {
            n = rnet_os_recvfrom(t->sock, buf, cap, &src, &would_block);
            if (n < 0)
            {
                return would_block ? 0 : -1;
            }
            if (t->hub_mode)
            {
                hub_learn_and_forward(t, buf, n, &src);
                return n;
            }
            if (!t->peer_known || sockaddr_equal(&src, &t->peer))
            {
                if (t->accept_first_peer && !t->peer_known)
                {
                    t->pending_peer = src;
                    t->pending_peer_known = 1;
                }
                return n;
            }
        }
    }
    if (t->mode == RNET_TRANSPORT_ICE)
    {
        size_t out_len = 0;
        if (t->ice_recv == NULL)
        {
            return -1;
        }
        if (t->ice_recv(t->ice_ctx, buf, cap, &out_len) != 0)
        {
            return 0;
        }
        return (int)out_len;
    }
    return 0;
}

void rnet_transport_accept_pending_peer(RNetTransport *t)
{
    if (t == NULL || t->hub_mode || !t->accept_first_peer || t->peer_known || !t->pending_peer_known)
    {
        return;
    }
    t->peer = t->pending_peer;
    t->peer_known = 1;
    t->pending_peer_known = 0;
}
