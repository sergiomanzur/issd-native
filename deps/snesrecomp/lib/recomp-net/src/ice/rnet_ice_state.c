#include "recomp_net/ice.h"

const char *rnet_ice_state_name(RNetIceState st)
{
    switch (st)
    {
    case RNET_ICE_STATE_IDLE:
        return "idle";
    case RNET_ICE_STATE_GATHERING:
        return "gathering";
    case RNET_ICE_STATE_CONNECTING:
        return "connecting";
    case RNET_ICE_STATE_CONNECTED:
        return "connected";
    case RNET_ICE_STATE_COMPLETED:
        return "completed";
    case RNET_ICE_STATE_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}
