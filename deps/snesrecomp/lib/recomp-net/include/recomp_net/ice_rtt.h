#ifndef RECOMP_NET_ICE_RTT_H
#define RECOMP_NET_ICE_RTT_H

#include "recomp_net/ice.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lightweight ICE data-channel RTT probe for lobby waiting rooms.
 * Exchanges SDP/candidates via the host-supplied emit callback (WS lobby
 * signal), then measures application ping/pong over the selected ICE pair
 * (host / srflx / relay). Use force_relay in RNetIceConfig for Force TURN.
 */
typedef struct RNetIceRttProbe RNetIceRttProbe;

typedef void (*RNetIceRttSignalEmitFn)(const RNetSignal *msg, void *user);

/* 0 on success. emit may be NULL only for controlled unit tests. */
int rnet_ice_rtt_open(RNetIceRttProbe **out, const RNetIceConfig *ice,
                      RNetIceRttSignalEmitFn emit, void *user);
void rnet_ice_rtt_close(RNetIceRttProbe **probe);

void rnet_ice_rtt_push_signal(RNetIceRttProbe *probe, const RNetSignal *msg);
/* Poll ICE, optional relay fallback, send/recv app PING/PONG. */
void rnet_ice_rtt_pump(RNetIceRttProbe *probe);

RNetIceState rnet_ice_rtt_state(const RNetIceRttProbe *probe);
/* Latest RTT in ms, or -1 if unknown. */
int rnet_ice_rtt_ms(const RNetIceRttProbe *probe);
/* selected path: host|srflx|prflx|relay|pending|failed|unknown */
void rnet_ice_rtt_selected_path(const RNetIceRttProbe *probe, char *path_out,
                                size_t path_len);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_ICE_RTT_H */
