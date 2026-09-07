#ifndef RECOMP_NET_RTT_PROBE_H
#define RECOMP_NET_RTT_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lightweight UDP PING/PONG for waiting-room peer latency (RNETDJ1 wire,
 * same as lan_direct). Not a seat claim — safe alongside a MotK WS lobby.
 *
 * Host: open on the advertised game bind (e.g. 0.0.0.0:7777), answer PINGs,
 * optionally ping the guest when their endpoint is known.
 * Guest: open on guest bind or ephemeral, set_peer(host_endpoint), ping.
 * Close before the game session binds the same port.
 */

typedef struct RNetRttProbe RNetRttProbe;

/* bind_hostport NULL/empty → ephemeral port. Returns 0 on success. */
int rnet_rtt_probe_open(RNetRttProbe **out, const char *bind_hostport);
void rnet_rtt_probe_close(RNetRttProbe **probe);

/* Destination for outbound PINGs. NULL/empty clears. Returns 0 on success. */
int rnet_rtt_probe_set_peer(RNetRttProbe *probe, const char *peer_hostport);
int rnet_rtt_probe_peer_known(const RNetRttProbe *probe);

/* Send one PING with a monotonic timestamp. Returns 0 on success.
 * When out_sent_ms is non-NULL, writes the stamp embedded in the PING. */
int rnet_rtt_probe_ping(RNetRttProbe *probe);
int rnet_rtt_probe_ping_ts(RNetRttProbe *probe, unsigned long long *out_sent_ms);

/* Drain socket: answer PINGs, compute RTT from PONGs.
 * Returns 1 when *out_rtt_ms was updated, 0 otherwise, <0 on hard error.
 * When out_echo_ms is non-NULL, writes the PONG's echoed send stamp (for
 * matching burst probes to lobby list rows). */
int rnet_rtt_probe_pump(RNetRttProbe *probe, int *out_rtt_ms);
int rnet_rtt_probe_pump_ex(RNetRttProbe *probe, int *out_rtt_ms,
                           unsigned long long *out_echo_ms);

/* Blocking one-shot: ephemeral socket, one PING, wait up to timeout_ms.
 * Returns 1 and sets *out_rtt_ms on success, 0 on timeout/failure. */
int rnet_rtt_probe_once(const char *peer_hostport, int timeout_ms, int *out_rtt_ms);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_RTT_PROBE_H */
