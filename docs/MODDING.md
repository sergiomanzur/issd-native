# ISSD Native — Modding Architecture & Specifications

**ISSD Native** features a dedicated modding pipeline that externalizes game content (teams, rosters, player statistics, kit designs, audio tracks) into clean, human-readable JSON formats and replacement assets.

---

## 1. Mod Folder Hierarchy

Mods reside in a `mods/` subfolder adjacent to the executable:

```text
ISSDNative/
├── mods/
│   ├── classic_1998/
│   │   ├── mod.json
│   │   ├── teams/
│   │   │   ├── brazil.json
│   │   │   ├── argentina.json
│   │   │   └── england.json
│   │   └── audio/
│   └── hd_kits/
│       └── textures/
```

---

## 2. Mod Manifest (`mod.json`)

```json
{
  "name": "1998 World Cup Edition",
  "version": "1.0.0",
  "author": "ISSD Modding Team",
  "description": "Complete roster and kit update for the 1998 World Cup squads.",
  "target_version": "1.0",
  "priority": 100
}
```

---

## 3. Team Data Schema (`teams/team_name.json`)

```json
{
  "id": 0,
  "name": "BRAZIL",
  "short_name": "BRA",
  "country_code": "BR",
  "flag_index": 0,
  "formation": "4-2-3-1",
  "tactics": "attacking",
  "kits": {
    "home": {
      "shirt_color": "#FFDF00",
      "shorts_color": "#002776",
      "socks_color": "#FFFFFF",
      "collar_color": "#009C3B"
    },
    "away": {
      "shirt_color": "#002776",
      "shorts_color": "#FFFFFF",
      "socks_color": "#002776",
      "collar_color": "#FFFFFF"
    },
    "goalkeeper": {
      "shirt_color": "#808080",
      "shorts_color": "#000000"
    }
  },
  "players": [
    {
      "shirt_number": 1,
      "name": "TAFFAREL",
      "position": "GK",
      "height": 182,
      "weight": 74,
      "attributes": {
        "acceleration": 75,
        "speed": 70,
        "shooting": 40,
        "technique": 60,
        "balance": 80,
        "intelligence": 85,
        "dribbling": 35,
        "jumping": 88,
        "stamina": 90,
        "goalkeeping": 92
      }
    },
    {
      "shirt_number": 7,
      "name": "ALLEJO",
      "position": "FW",
      "height": 178,
      "weight": 72,
      "attributes": {
        "acceleration": 98,
        "speed": 99,
        "shooting": 99,
        "technique": 98,
        "balance": 90,
        "intelligence": 95,
        "dribbling": 99,
        "jumping": 90,
        "stamina": 95,
        "goalkeeping": 10
      }
    }
  ]
}
```

---

## 4. Player Attribute Definitions

All attributes correspond 1:1 with ISSD's internal ROM stat matrices:

- **`acceleration`**: Rate at which player reaches maximum running speed.
- **`speed`**: Maximum horizontal/vertical sprint velocity.
- **`shooting`**: Shot velocity, curve, and accuracy on goal attempts.
- **`technique`**: First-touch ball control, curve passing, and volley accuracy.
- **`balance`**: Resistance to tackles, body charge stability, and recovery time from slides.
- **`intelligence / AI`**: Offensive positioning, offside avoidance, and defensive interception positioning.
- **`dribbling`**: Ball adhesion during sharp turns and skill moves.
- **`jumping`**: Maximum aerial height for headers and bicycle kicks.
- **`stamina`**: Energy conservation and sprint exhaustion resistance over match duration.
- **`goalkeeping`**: Reaction time, diving distance, and handling certainty for goalkeepers.

---

## 5. Formations and Tactics

A team's shape is not derived from its players' listed positions. The
cartridge keeps it in a record of its own, and a mod pack sets it with two
keys on the team:

```json
{
  "team_id": 33,
  "name": "Mexico",
  "formation": "4-2-3-1",
  "tactics": "attacking"
}
```

Both are optional. Leaving `formation` out keeps the team's original shape.
Names are matched with case and separators ignored, so `4-2-3-1`, `4231` and
`4 2 3 1` are the same formation.

### Available formations

| Name | Shape | Printed as |
|---|---|---|
| `4-4-2` | Flat back four, flat midfield four, two strikers | 4-4-2 |
| `4-4-2 diamond` | Back four, a holder, two wide, one behind the strikers | 4-4-2 |
| `4-3-3` | Back four, a midfield three, a front three | 4-3-3 |
| `4-2-3-1` | Two holding midfielders, three behind a lone striker | 4-5-1 |
| `4-1-4-1` | An anchor behind a midfield four, one striker | 4-5-1 |
| `4-5-1` | Back four, a flat five across midfield, one striker | 4-5-1 |
| `3-5-2` | Three at the back, wing backs, two strikers | 3-5-2 |
| `3-4-3` | Three at the back, four across, a front three | 3-4-3 |
| `3-4-2-1` | Three at the back, two shadow strikers behind a lone man | 3-4-3 |
| `5-3-2` | Five at the back, three in the middle, two up | 5-3-2 |
| `5-4-1` | Five at the back, four across, one striker | 5-4-1 |
| `4-2-4` | Back four, two in midfield, four forwards | 4-2-4 |
| `3-3-4` | Three at the back, three in the middle, four up | 3-3-4 |
| `2-3-5` | The pyramid. Two at the back and five forwards | 2-3-5 |

The **Printed as** column is what the team select screen shows. The screen
can only draw three numbers, so a four-band shape is labelled the way a
newspaper would have labelled it in 1995: a 4-2-3-1 reads as 4-5-1. What the
players actually do on the pitch follows the shape, not the label - the two
holding midfielders in a 4-2-3-1 really do sit behind the other three.

### Tactics

| Value | Effect |
|---|---|
| `attacking` | The whole side starts six units further up the pitch; full backs overlap and midfielders push on. |
| `balanced` (default) | The formation as authored. |
| `defensive` | The whole side sits six units deeper and nobody is given an attacking role. |

`strategy` is accepted as an alias for `tactics`.

Tactics never change how many players are in each line, so the printed label
stays true whichever preset is used.

### Squad order matters

A formation assigns positions by squad order, not by the `position` string.
Slot 0 in the `players` list is the goalkeeper, and players 1 to 10 fill the
formation's ten outfield slots in the order listed. For `4-2-3-1` that is:
four defenders, the two holding midfielders, the three behind the striker,
and the striker last.

If the `position` strings of the first eleven do not add up to the same
number of defenders, midfielders and forwards as the formation, the loader
says so on startup and carries on: the pitch follows the formation and the
squad list follows the positions, and which of the two is wrong is the
author's call.

### How this is stored

For anyone extending the tooling: `$8B:EF48` holds thirty-six 16-bit
pointers, one per team, into bank `$8B`. Each names a 31-byte record:

| Offset | Bytes | Meaning |
|---|---|---|
| `+0` | 1 | Label index, 0-15. The three-number string the team select screen prints. |
| `+1` | 20 | Ten `(depth, width)` signed byte pairs, one per outfield player in squad order. Negative depth is further up the pitch. |
| `+21` | 10 | Ten role bytes: 1 defender, 5 overlapping defender, 2 midfielder, 6 attacking midfielder, 3 forward. |

The label indices, all sixteen read off the screen one at a time, are:
`0` 4-5-1, `1` 4-4-2, `2` 4-3-3, `3` 4-2-4, `4` 3-5-2, `5` 3-4-3, `6` 3-3-4,
`7` 3-2-5, `8` 2-5-3, `9` 2-4-4, `10` 2-3-5, `11` 5-4-1, `12` 5-3-2,
`13` 5-2-3, `14` 1-5-4, `15` 1-4-5.

The label is independent of the record's own contents, so it has to be kept
consistent by hand. `tests/test_formation.c` asserts that every entry in the
library counts its roles the way its label claims.
