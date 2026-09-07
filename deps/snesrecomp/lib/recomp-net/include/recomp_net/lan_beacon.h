#ifndef RECOMP_NET_LAN_BEACON_H
#define RECOMP_NET_LAN_BEACON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local UDP broadcast discovery for online lobby hosts.
 *
 * Hosts announce lobby_id + LAN game endpoint on a well-known discovery port.
 * Guests listen and map lobby_id → RFC1918 endpoint for list RTT / join without
 * publishing private IPs to the matchmaking hub.
 *
 * Wire (text lines, UTF-8):
 *   RNETBC1
 *   ANNOUNCE
 *   <lobby_id>
 *   <game_endpoint>   e.g. 192.168.1.42:7777
 *   <game_name>       optional; empty line ok
 */

#define RNET_LAN_BEACON_DEFAULT_PORT 48777
#define RNET_LAN_BEACON_ID_MAX 40
#define RNET_LAN_BEACON_ENDPOINT_MAX 64
#define RNET_LAN_BEACON_GAME_MAX 64

typedef struct RNetLanBeacon RNetLanBeacon;

/* Publisher (online waiting-room host). discovery_port 0 → default. */
int rnet_lan_beacon_publish_open(RNetLanBeacon **out, unsigned short discovery_port);
int rnet_lan_beacon_publish_set(RNetLanBeacon *beacon, const char *lobby_id,
                                const char *game_endpoint, const char *game_name);
/* Send at most once per ~1s while set. Returns 0 ok. */
int rnet_lan_beacon_publish_tick(RNetLanBeacon *beacon);

/* Listener (lobby list browser). discovery_port 0 → default. */
int rnet_lan_beacon_listen_open(RNetLanBeacon **out, unsigned short discovery_port);
/* Drain announces into the cache. Returns count newly updated this call. */
int rnet_lan_beacon_listen_pump(RNetLanBeacon *beacon);

/* Lookup a fresh announce (seen within ~5s). Returns 1 and fills endpoint. */
int rnet_lan_beacon_lookup(const RNetLanBeacon *beacon, const char *lobby_id,
                           char *endpoint_out, size_t endpoint_cap);

void rnet_lan_beacon_close(RNetLanBeacon **beacon);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_LAN_BEACON_H */
