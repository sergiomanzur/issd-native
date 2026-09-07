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
  "formation": "4-4-2_A",
  "strategy": "NORMAL",
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
