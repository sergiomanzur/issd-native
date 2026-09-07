#ifndef RECOMP_NET_TRANSPORT_H
#define RECOMP_NET_TRANSPORT_H

#include "recomp_net/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RNetTransportMode
{
    RNET_TRANSPORT_NONE = 0,
    RNET_TRANSPORT_LAN_UDP = 1,
    RNET_TRANSPORT_ICE = 2
} RNetTransportMode;

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_TRANSPORT_H */
