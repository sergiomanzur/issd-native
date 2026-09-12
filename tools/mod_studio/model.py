"""The pack, in memory.

A pack on disk is ordinary JSON and a modder may have written it by hand, so
the rule here is: keep whatever you do not understand. Anything the editor has
no field for is carried through to the file it writes, because a pack can hold
notes an editor should not quietly throw away.
"""
from __future__ import annotations

import copy
import json
import os
import re
from collections import OrderedDict

from . import repo

ATTRIBUTES = ("acceleration", "speed", "shooting", "technique", "balance",
              "intelligence", "dribbling", "jumping", "stamina", "goalkeeping")

# The ten are written as four and a half bytes, and goalkeeping is the half
# that is not there: the cartridge keeps it somewhere the patcher does not
# reach. The editor shows it, greyed, rather than pretending it lands.
ATTRIBUTES_WRITTEN = ATTRIBUTES[:-1]

PLAYER_KEYS = ("name", "shirt_number", "position", "skin_tone", "hair_style") \
    + ATTRIBUTES
TEAM_KEYS = ("new_team", "team_id", "name", "short_name", "country_code",
             "plate_name", "photo", "formation", "tactics",
             "shirt", "shorts", "socks", "kit_record", "players")
STADIUM_KEYS = ("stadium_id", "name", "display_name", "pitch_length",
                "pitch_width")
PACK_KEYS = ("name", "author", "version", "description", "stadium_count",
             "unlock_bonus_teams", "teams", "stadiums")


def _ordered(source: dict, keys, extra_first=False) -> OrderedDict:
    """Known keys in a stable order, then anything else the file carried."""
    out = OrderedDict()
    for k in keys:
        if k in source:
            out[k] = source[k]
    for k, v in source.items():
        if k not in out:
            out[k] = v
    return out


def new_player(slot: int) -> OrderedDict:
    position = "GK" if slot in (0, repo.RESERVE_KEEPER) else "MF"
    p = OrderedDict()
    p["name"] = "Player"
    p["shirt_number"] = slot + 1
    p["position"] = position
    p["skin_tone"] = 0
    p["hair_style"] = 0
    for a in ATTRIBUTES:
        p[a] = 50 if a != "goalkeeping" else (70 if position == "GK" else 6)
    return p


def new_team(added: bool = True) -> OrderedDict:
    t = OrderedDict()
    if added:
        t["new_team"] = True
    else:
        t["team_id"] = 0
    t["name"] = "New Team"
    t["plate_name"] = ""
    t["formation"] = "4-4-2"
    t["tactics"] = "balanced"
    t["shirt"] = "#FFFFFF"
    t["shorts"] = "#1030A0"
    t["socks"] = "#FFFFFF"
    t["players"] = [new_player(i) for i in range(repo.SQUAD_SLOTS)]
    return t


def new_stadium(stadium_id: int = repo.STOCK_STADIUMS) -> OrderedDict:
    s = OrderedDict()
    s["stadium_id"] = stadium_id
    s["name"] = "NEW"
    s["display_name"] = "NEW GROUND"
    s["pitch_length"] = 115
    s["pitch_width"] = 74
    return s


def new_pack() -> OrderedDict:
    p = OrderedDict()
    p["name"] = "Untitled Pack"
    p["author"] = ""
    p["version"] = "1.0.0"
    p["description"] = ""
    p["teams"] = []
    p["stadiums"] = []
    return p


# ------------------------------------------------------------ load & save --

_COMMENTS = re.compile(r"/\*.*?\*/|(?<![:\w])//[^\n]*", re.S)


def _strip_comments(text: str) -> str:
    """The loader accepts // and /* */, which JSON does not, so packs in the
    wild have them. Strings are left alone."""
    out = []
    i = 0
    in_string = False
    while i < len(text):
        c = text[i]
        if in_string:
            out.append(c)
            if c == "\\" and i + 1 < len(text):
                out.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue
        if text.startswith("//", i):
            while i < len(text) and text[i] != "\n":
                i += 1
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            i = len(text) if end < 0 else end + 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def load(path: str) -> OrderedDict:
    with open(path, encoding="utf-8-sig") as f:
        raw = f.read()
    data = json.loads(_strip_comments(raw), object_pairs_hook=OrderedDict)
    if not isinstance(data, dict):
        raise ValueError("a pack must be a JSON object")
    data.setdefault("teams", [])
    data.setdefault("stadiums", [])
    return data


def save(pack: dict, path: str) -> None:
    out = _ordered(pack, PACK_KEYS)
    out["teams"] = [_ordered(t, TEAM_KEYS) for t in pack.get("teams", [])]
    for t in out["teams"]:
        if "players" in t:
            t["players"] = [_ordered(p, PLAYER_KEYS) for p in t["players"]]
    out["stadiums"] = [_ordered(s, STADIUM_KEYS) for s in pack.get("stadiums", [])]
    if not out["stadiums"]:
        out.pop("stadiums")
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(out, f, indent=2)
        f.write("\n")
    os.replace(tmp, path)


# ---------------------------------------------------------------- helpers --

def team_label(team: dict) -> str:
    name = (team.get("name") or "").strip()
    if team.get("new_team"):
        return "%s  (added)" % (name or "unnamed")
    tid = team.get("team_id")
    names = repo.team_names()
    who = names[tid] if isinstance(tid, int) and 0 <= tid < len(names) else "?"
    return "%s  (%s, %s)" % (name or "unnamed", tid, who)


def stadium_label(st: dict) -> str:
    sid = st.get("stadium_id")
    shown = (st.get("display_name") or st.get("name") or "unnamed").strip()
    return "%s  (%s)" % (shown, sid)


def player_label(player: dict, slot: int) -> str:
    role = {0: "GK", repo.RESERVE_KEEPER: "sub GK"}.get(slot)
    if role is None:
        role = "XI" if slot < repo.STARTING_XI else "sub"
    return "%2d  %-9s %-3s %s" % (slot, (player.get("name") or "?")[:9],
                                  player.get("position") or "", role)


def parse_colour(text) -> tuple[int, int, int] | None:
    if not isinstance(text, str):
        return None
    t = text[1:] if text.startswith("#") else text
    if len(t) != 6 or any(c not in "0123456789abcdefABCDEF" for c in t):
        return None
    return int(t[0:2], 16), int(t[2:4], 16), int(t[4:6], 16)


def shade(rgb: tuple[int, int, int], num: int, den: int = 100) -> tuple[int, int, int]:
    """The cartridge shades a strip by darkening - the same arithmetic
    write_part uses, so the preview is what the game will draw."""
    def five(v):
        v = min(255, v * num // den)
        return (v * 31 // 255) * 255 // 31        # through the 5 bits and back
    return tuple(five(c) for c in rgb)


def clone_team(team: dict) -> OrderedDict:
    return copy.deepcopy(team)
