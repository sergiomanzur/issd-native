#ifndef RNET_ICE_INTERNAL_H
#define RNET_ICE_INTERNAL_H

#include "recomp_net/ice.h"
#include "recomp_net/types.h"

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RNetIceAgent RNetIceAgent;

typedef void (*RNetIceSignalEmitFn)(const RNetSignal *msg, void *user);

#if defined(RNET_ENABLE_ICE)

RNetIceAgent *rnet_ice_agent_create(const RNetIceConfig *cfg, RNetIceSignalEmitFn emit, void *user);
void rnet_ice_agent_destroy(RNetIceAgent *agent);
int rnet_ice_agent_start_gathering(RNetIceAgent *agent);
void rnet_ice_agent_poll(RNetIceAgent *agent);
void rnet_ice_agent_push_signal(RNetIceAgent *agent, const RNetSignal *msg);
RNetIceState rnet_ice_agent_state(const RNetIceAgent *agent);
int rnet_ice_agent_send(RNetIceAgent *agent, const rnet_u8 *buf, size_t len);
int rnet_ice_agent_recv(RNetIceAgent *agent, rnet_u8 *buf, size_t cap, size_t *out_len);
/* path_out: host|srflx|prflx|relay|unknown. Addresses may be empty. */
void rnet_ice_agent_selected_info(const RNetIceAgent *agent, char *path_out, size_t path_len,
                                  char *local_addr_out, size_t local_addr_len,
                                  char *remote_addr_out, size_t remote_addr_len);
int rnet_ice_agent_has_turn(const RNetIceAgent *agent);
/* Nonzero when remotes were seen and every IPv4 address was private. */
int rnet_ice_agent_remote_only_private(const RNetIceAgent *agent);
int rnet_ice_agent_is_force_relay(const RNetIceAgent *agent);
int rnet_ice_agent_relay_fallback_done(const RNetIceAgent *agent);
/* Mark fallback as consumed without restarting (e.g. skipped for LAN peers). */
void rnet_ice_agent_mark_relay_fallback_done(RNetIceAgent *agent);
/* Destroy/recreate juice with force_relay=1 and restart gathering. */
int rnet_ice_agent_restart_force_relay(RNetIceAgent *agent);

#else

static inline RNetIceAgent *rnet_ice_agent_create(const RNetIceConfig *cfg, RNetIceSignalEmitFn emit, void *user)
{
    (void)cfg;
    (void)emit;
    (void)user;
    return NULL;
}
static inline void rnet_ice_agent_destroy(RNetIceAgent *agent)
{
    (void)agent;
}
static inline int rnet_ice_agent_start_gathering(RNetIceAgent *agent)
{
    (void)agent;
    return -1;
}
static inline void rnet_ice_agent_poll(RNetIceAgent *agent)
{
    (void)agent;
}
static inline void rnet_ice_agent_push_signal(RNetIceAgent *agent, const RNetSignal *msg)
{
    (void)agent;
    (void)msg;
}
static inline RNetIceState rnet_ice_agent_state(const RNetIceAgent *agent)
{
    (void)agent;
    return RNET_ICE_STATE_IDLE;
}
static inline int rnet_ice_agent_send(RNetIceAgent *agent, const rnet_u8 *buf, size_t len)
{
    (void)agent;
    (void)buf;
    (void)len;
    return -1;
}
static inline int rnet_ice_agent_recv(RNetIceAgent *agent, rnet_u8 *buf, size_t cap, size_t *out_len)
{
    (void)agent;
    (void)buf;
    (void)cap;
    (void)out_len;
    return -1;
}
static inline void rnet_ice_agent_selected_info(const RNetIceAgent *agent, char *path_out,
                                                size_t path_len, char *local_addr_out,
                                                size_t local_addr_len, char *remote_addr_out,
                                                size_t remote_addr_len)
{
    (void)agent;
    if (path_out && path_len)
        snprintf(path_out, path_len, "none");
    if (local_addr_out && local_addr_len)
        local_addr_out[0] = '\0';
    if (remote_addr_out && remote_addr_len)
        remote_addr_out[0] = '\0';
}
static inline int rnet_ice_agent_has_turn(const RNetIceAgent *agent)
{
    (void)agent;
    return 0;
}
static inline int rnet_ice_agent_remote_only_private(const RNetIceAgent *agent)
{
    (void)agent;
    return 0;
}
static inline int rnet_ice_agent_is_force_relay(const RNetIceAgent *agent)
{
    (void)agent;
    return 0;
}
static inline int rnet_ice_agent_relay_fallback_done(const RNetIceAgent *agent)
{
    (void)agent;
    return 0;
}
static inline void rnet_ice_agent_mark_relay_fallback_done(RNetIceAgent *agent)
{
    (void)agent;
}
static inline int rnet_ice_agent_restart_force_relay(RNetIceAgent *agent)
{
    (void)agent;
    return -1;
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* RNET_ICE_INTERNAL_H */
