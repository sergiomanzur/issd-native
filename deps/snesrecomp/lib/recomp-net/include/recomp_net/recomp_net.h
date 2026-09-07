#ifndef RECOMP_NET_H
#define RECOMP_NET_H

#include "recomp_net/address.h"
#include "recomp_net/config.h"
#include "recomp_net/ice.h"
#include "recomp_net/ice_rtt.h"
#include "recomp_net/input.h"
#include "recomp_net/input_contract.h"
#include "recomp_net/rollback.h"
#include "recomp_net/lan_lobby.h"
#include "recomp_net/lan_direct.h"
#include "recomp_net/lan_beacon.h"
#include "recomp_net/session.h"
#include "recomp_net/transport.h"
#include "recomp_net/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Library version: 0.1.0 */
#define RNET_VERSION_MAJOR 0
#define RNET_VERSION_MINOR 1
#define RNET_VERSION_PATCH 0

const char *rnet_version_string(void);

/* Same FNV-1a-style hash used for wire payload_crc / STATE probe. */
rnet_u32 rnet_checksum(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_H */
