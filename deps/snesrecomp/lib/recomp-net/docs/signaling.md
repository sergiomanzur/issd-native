# Signaling contract

recomp-net does **not** implement a lobby or matchmaking server. ICE credentials
and SDP/candidate exchange are owned by the host (or a future custom lobby).

## Messages (`RNetSignal`)

| Type | Direction | Payload |
|------|-----------|---------|
| `RNET_SIGNAL_LOCAL_SDP` | library → host | Local SDP offer/answer text |
| `RNET_SIGNAL_REMOTE_SDP` | host → library | Peer SDP |
| `RNET_SIGNAL_LOCAL_CANDIDATE` | library → host | Trickle candidate line |
| `RNET_SIGNAL_REMOTE_CANDIDATE` | host → library | Peer candidate |
| `RNET_SIGNAL_GATHERING_DONE` | library → host | Gathering finished |
| `RNET_SIGNAL_SET_CONTROLLING` | host → library | `flag != 0` ⇒ controlling gather-order hint (libjuice has no public set-role API) |

`text` is NUL-terminated and truncated at 2047 characters.

## Host duties

1. Pass `on_signal` in `RNetHostVTable` before `rnet_session_start_ice`.
2. Forward every outbound signal to the peer through your lobby channel.
3. Deliver inbound signals with `rnet_session_push_signal`.
4. Supply STUN/TURN in `RNetIceConfig` before gather. The library does not
   fetch credentials; snesrecomp’s WS lobby client mints Coturn creds via
   `get_turn_credentials` / `turn_credentials` and passes them into
   `snes_netplay_start` (or use `SNES_NET_TURN_*` env overrides).

## Mapping to the WS lobby server

recomp-net-server’s WebSocket lobby treats each `RNetSignal` as a 1:1
envelope (`op:signal` type + flag + text). TURN minting is a separate
`get_turn_credentials` op (same HMAC shape as HTTP `/v1/turn-credentials`).

## LAN-only

Omit ICE: call `rnet_session_start_lan` and leave `on_signal` NULL. Manual file
exchange demo: `examples/ice_manual_signaling`.
