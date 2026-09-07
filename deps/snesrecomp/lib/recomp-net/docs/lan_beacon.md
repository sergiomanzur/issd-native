# LAN beacon (local discovery)

Online lobby hosts must not publish RFC1918 addresses to the matchmaking hub.
Same-LAN guests discover the host’s game UDP endpoint with a short UDP
broadcast announce instead.

## Wire

Discovery port: **48777** (`RNET_LAN_BEACON_DEFAULT_PORT`).

```
RNETBC1
ANNOUNCE
<lobby_id>
<game_endpoint>
<game_name>
```

`game_endpoint` must be RFC1918 (no loopback). Hosts send to
`255.255.255.255:48777` about once per second while the waiting room is open.
Guests bind the discovery port, cache announces (~5 s TTL), and look up by
`lobby_id` when probing lobby-list latency.

## API

See `include/recomp_net/lan_beacon.h`:

- `rnet_lan_beacon_publish_*` — host announce
- `rnet_lan_beacon_listen_*` / `rnet_lan_beacon_lookup` — guest cache
