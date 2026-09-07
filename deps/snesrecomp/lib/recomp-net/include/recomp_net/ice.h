#ifndef RECOMP_NET_ICE_H
#define RECOMP_NET_ICE_H

#include "recomp_net/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RNetIceState
{
    RNET_ICE_STATE_IDLE = 0,
    RNET_ICE_STATE_GATHERING,
    RNET_ICE_STATE_CONNECTING,
    RNET_ICE_STATE_CONNECTED,
    RNET_ICE_STATE_COMPLETED,
    RNET_ICE_STATE_FAILED
} RNetIceState;

typedef enum RNetSignalType
{
    RNET_SIGNAL_LOCAL_SDP = 1,
    RNET_SIGNAL_REMOTE_SDP = 2,
    RNET_SIGNAL_LOCAL_CANDIDATE = 3,
    RNET_SIGNAL_REMOTE_CANDIDATE = 4,
    RNET_SIGNAL_GATHERING_DONE = 5,
    RNET_SIGNAL_SET_CONTROLLING = 6
} RNetSignalType;

/*
 * Out-of-band signaling blob for a future custom lobby server.
 * No ticket/automatch fields — host maps these 1:1 onto its lobby protocol.
 */
typedef struct RNetSignal
{
    RNetSignalType type;
    /* For SET_CONTROLLING: non-zero = controlling (gather-order hint; libjuice has no set_role API). */
    rnet_u8 flag;
    /* NUL-terminated SDP or candidate line (truncated if longer). */
    char text[2048];
} RNetSignal;

typedef struct RNetIceConfig
{
    const char *stun_host;
    rnet_u16 stun_port;
    const char *turn_host;
    rnet_u16 turn_port;
    const char *turn_user;
    const char *turn_pass;
    /* Optional bind address (NULL = any). */
    const char *bind_address;
    rnet_u16 bind_port;
    /* Non-zero = offerer: gather immediately. Zero = answerer: wait for remote SDP. */
    rnet_u8 controlling;
    /* Non-zero = only use typ relay candidates (requires TURN in this config). */
    rnet_u8 force_relay;
} RNetIceConfig;

static inline void rnet_ice_config_init_defaults(RNetIceConfig *cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    cfg->stun_host = "stun.l.google.com";
    cfg->stun_port = 19302;
    cfg->turn_host = NULL;
    cfg->turn_port = 0;
    cfg->turn_user = NULL;
    cfg->turn_pass = NULL;
    cfg->bind_address = NULL;
    cfg->bind_port = 0;
    cfg->controlling = 1;
    cfg->force_relay = 0;
}

const char *rnet_ice_state_name(RNetIceState st);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_ICE_H */
