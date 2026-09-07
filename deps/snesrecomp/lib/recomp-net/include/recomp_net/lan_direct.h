#ifndef RNET_LAN_DIRECT_H
#define RNET_LAN_DIRECT_H

#include "recomp_net/lan_lobby.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UDP waiting-room seat claim for LAN/Direct IP across machines.
 *
 * Host binds the advertised game UDP port while the lobby is open, answers
 * JOIN_REQ, and later notifies START. Guest Join Direct sends JOIN_REQ to the
 * typed host IP:port (public IP + port-forward, or LAN IP). Same-machine play
 * can still use the file registry (rnet_lan_lobby); this path is for remote.
 *
 * Wire format is a small text datagram (RNETDJ1). Not encrypted. */

enum {
    RNET_LAN_DIRECT_OK = 0,
    RNET_LAN_DIRECT_ERR_IO = -1,
    RNET_LAN_DIRECT_ERR_FULL = -2,
    RNET_LAN_DIRECT_ERR_PASSWORD = -3,
    RNET_LAN_DIRECT_ERR_IDENTITY = -4,
    RNET_LAN_DIRECT_ERR_STARTED = -5,
    RNET_LAN_DIRECT_ERR_TIMEOUT = -6,
    RNET_LAN_DIRECT_ERR_ARGUMENT = -7
};

typedef struct RNetLanDirectHost RNetLanDirectHost;
typedef struct RNetLanDirectGuest RNetLanDirectGuest;

/* ---- host (waiting room) ------------------------------------------------- */

/* Bind UDP on bind_hostport (usually 0.0.0.0:<port>). Copies room fields.
 * Returns 0 on success. */
int rnet_lan_direct_host_open(RNetLanDirectHost **out, const char *bind_hostport,
                              const RNetLanLobby *room);

void rnet_lan_direct_host_close(RNetLanDirectHost **host);

/* Non-blocking: process JOIN_REQ / LEAVE / PING/PONG. Updates *room.
 * Returns 1 if seat state changed. When a PONG arrives, *out_rtt_ms (if
 * non-NULL) is set to the measured round-trip milliseconds. */
int rnet_lan_direct_host_pump(RNetLanDirectHost *host, RNetLanLobby *room,
                              int *out_rtt_ms);

/* Send a latency probe to the seated guest (no-op if none). */
int rnet_lan_direct_host_ping(RNetLanDirectHost *host);

/* Notify seated guest that the match is starting. */
int rnet_lan_direct_host_notify_start(RNetLanDirectHost *host,
                                      const RNetLanLobby *room);

/* Host: push updated match caps (D / rollback / P) while a guest is seated. */
int rnet_lan_direct_host_notify_caps(RNetLanDirectHost *host,
                                     const RNetLanLobby *room);

/* Tell guest they were kicked / host left. */
int rnet_lan_direct_host_notify_kick(RNetLanDirectHost *host);
int rnet_lan_direct_host_notify_close(RNetLanDirectHost *host);

/* ---- guest --------------------------------------------------------------- */

/* Blocking join to host_hostport. timeout_ms <= 0 → 2000.
 * On OK, *out_room is filled and *out_guest owns a socket for START/KICK. */
int rnet_lan_direct_guest_join(const char *host_hostport,
                               const char *expected_game,
                               const char *expected_version,
                               const char *password, const char *player_name,
                               const char *guest_bind_hostport, int timeout_ms,
                               RNetLanLobby *out_room,
                               RNetLanDirectGuest **out_guest);

void rnet_lan_direct_guest_close(RNetLanDirectGuest **guest);

/* Non-blocking: 1=START, 2=KICK/CLOSE, 3=RTT updated (*out_rtt_ms), 0=none,
 * <0 error. */
int rnet_lan_direct_guest_pump(RNetLanDirectGuest *guest, RNetLanLobby *room,
                               int *out_rtt_ms);

/* Send a latency probe to the host. */
int rnet_lan_direct_guest_ping(RNetLanDirectGuest *guest);

/* Guest leaves waiting room (best-effort notify). */
int rnet_lan_direct_guest_leave(RNetLanDirectGuest *guest);

#ifdef __cplusplus
}
#endif

#endif /* RNET_LAN_DIRECT_H */
