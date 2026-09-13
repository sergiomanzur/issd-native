"""Read what is already in the cartridge, so a pack can start from it.

Editing a team from a blank sheet is not editing a team. This reads the
squads, the shapes, the strips and the grounds out of a `.sfc` and hands them
back as pack entries, so a modder opens Mexico, changes two players and saves.

Every offset, the character set and the rating quantiser are parsed out of
`ISSDNative/issd_mod_rom.c` rather than copied here. That matters more for
reading than anywhere else: a decoder that disagrees with the encoder turns a
round trip into silent corruption, and this way they cannot disagree.
"""
from __future__ import annotations

import os
import re

from . import repo

ROM_SIZE_MIN = 0x100000


class NotACartridge(Exception):
    pass


# ------------------------------------------------------------- constants ---

_NEEDED = (
    "ROM_NAME_BASE", "ROM_ATTR_BASE", "ROM_PLAYERS_PER_TEAM", "ROM_NAME_BYTES",
    "ROM_ATTR_BYTES", "ROM_STOCK_TEAMS", "ROM_TEAMS",
    "ROM_FORMATION_PTRS", "ROM_FORMATION_BANK", "ROM_FORMATION_RECORD_BYTES",
    "ROM_KIT_BASE", "ROM_KIT_STRIDE", "ROM_KIT_SHIRT", "ROM_KIT_SHORTS",
    "ROM_KIT_SOCKS", "ROM_STADIUMS", "ROM_STADIUM_NAME_BASE",
    "ROM_STADIUM_NAME_BYTES", "ROM_STADIUM_PITCH_BASE",
    "ROM_CELL_TABLE", "ROM_CELLS", "ROM_ROSTER_PTRS",
)


def _parse_c(root: str) -> dict:
    with open(os.path.join(root, "ISSDNative", "issd_mod_rom.c"),
              encoding="utf-8", errors="replace") as f:
        src = f.read()

    out = {}
    for name in _NEEDED:
        m = re.search(r"#define\s+%s\s+\(?([^)\n/]+)" % name, src)
        if not m:
            continue
        text = m.group(1).strip().rstrip("u").rstrip("U").strip()
        try:
            out[name] = int(text, 0)
        except ValueError:
            pass
    out["ROM_ROSTER_BYTES"] = (out.get("ROM_PLAYERS_PER_TEAM", 20) *
                               out.get("ROM_NAME_BYTES", 8))

    # The character set, so the decoder is the encoder read backwards.
    charset = {}
    block = src[src.index("kCharset[] = {"):]
    block = block[:block.index("};")]
    for code, ch in re.findall(r"\{\s*(0x[0-9A-Fa-f]+)\s*,\s*'(.)'\s*\}", block):
        charset[int(code, 16)] = ch
    charset.setdefault(0x00, " ")          # padding renders as a space
    out["charset"] = charset

    # Which kit palette each team wears - measured, not derivable.
    kit = []
    block = src[src.index("kKitRecord[ROM_TEAMS] = {"):]
    block = block[:block.index("};")]
    for value in re.findall(r"(-?\d+)\s*,\s*/\*", block):
        kit.append(int(value))
    out["kit_record"] = kit

    # How a rating comes back out of four bits.
    out["nibble_min"] = repo.RATING_NIBBLE_MIN
    out["nibble_max"] = repo.RATING_NIBBLE_MAX
    return out


_C: dict | None = None
_BAKED_KEY = "cartridge"


def constants() -> dict:
    global _C
    if _C is not None:
        return _C
    root = repo._repo_root()
    if root:
        try:
            _C = _parse_c(root)
            return _C
        except Exception:
            pass
    data = repo.load()
    _C = data.get(_BAKED_KEY)
    if not _C:
        raise NotACartridge("the cartridge layout is not available in this build")
    _C["charset"] = {int(k): v for k, v in _C["charset"].items()}
    return _C


# ----------------------------------------------------------------- decode --

def decode_name(raw: bytes) -> str:
    """Trailing padding goes, leading padding stays.

    Both are 0x00 and both render as a space, but they are not the same
    thing: the cartridge writes " Pabi" to sit the name where it wants it
    on screen, while the bytes after a name are just the rest of the
    field. Strip the lot and every imported name shifts left when it is
    written back."""
    cs = constants()["charset"]
    return "".join(cs.get(b, " ") for b in raw).rstrip()


def nibble_to_rating(nibble: int) -> int:
    """The middle of the band, so writing it back stores the same nibble.

    Anything in the band would round-trip; the middle is the one that reads as
    a rating rather than as an artefact of the arithmetic.
    """
    lo, hi = repo.nibble_band(max(repo.RATING_NIBBLE_MIN,
                                  min(repo.RATING_NIBBLE_MAX, nibble)))
    return (lo + hi) // 2


POSITION_BY_CODE = {1: "GK", 2: "DF", 4: "MF", 6: "FW"}


def decode_position(code: int) -> str:
    """3 and 5 have no name; they are kept as numbers so they survive."""
    return POSITION_BY_CODE.get(code, str(code))


def _word(rom: bytes, off: int) -> int:
    return rom[off] | (rom[off + 1] << 8)


def load_rom(path: str) -> bytes:
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % 1024 == 512:                 # a copier header
        data = data[512:]
    if len(data) < ROM_SIZE_MIN:
        raise NotACartridge("%s is too small to be a cartridge"
                            % os.path.basename(path))
    c = constants()
    if len(data) <= c["ROM_ATTR_BASE"]:
        raise NotACartridge("%s does not reach the roster tables"
                            % os.path.basename(path))
    # The first squad has to read as names, or this is some other cartridge.
    first = decode_name(data[c["ROM_NAME_BASE"]:c["ROM_NAME_BASE"] + 8])
    if not first or not first[0].isalpha():
        raise NotACartridge("%s is not International Superstar Soccer Deluxe "
                            "(USA), or is a revision these offsets do not fit"
                            % os.path.basename(path))
    return data


def roster_base(rom: bytes, team: int) -> int:
    """Where a team's twenty names are.

    Stock teams sit in the table at their own index; the pointer table is
    followed anyway, because a cartridge a pack has already been applied to
    has added teams whose names are somewhere else entirely.
    """
    c = constants()
    ptr = c["ROM_ROSTER_PTRS"] + team * 2
    if ptr + 1 < len(rom):
        addr = _word(rom, ptr)
        if 0x8000 <= addr <= 0xFFFF:
            off = 0x38000 + (addr - 0x8000)
            if off + c["ROM_ROSTER_BYTES"] <= len(rom):
                return off
    return c["ROM_NAME_BASE"] + team * c["ROM_ROSTER_BYTES"]


def read_team(rom: bytes, team: int) -> dict:
    c = constants()
    players = []
    names = roster_base(rom, team)
    for p in range(c["ROM_PLAYERS_PER_TEAM"]):
        n = decode_name(rom[names + p * c["ROM_NAME_BYTES"]:
                            names + (p + 1) * c["ROM_NAME_BYTES"]])
        a = c["ROM_ATTR_BASE"] + (team * c["ROM_PLAYERS_PER_TEAM"] + p) * c["ROM_ATTR_BYTES"]
        b = rom[a:a + c["ROM_ATTR_BYTES"]]
        players.append({
            "name": n,
            "shirt_number": p + 1,
            "position": decode_position(b[4] >> 4),
            "skin_tone": b[6] >> 4,
            "hair_style": b[6] & 0x0F,
            "acceleration": nibble_to_rating(b[0] >> 4),
            "speed":        nibble_to_rating(b[0] & 0x0F),
            "shooting":     nibble_to_rating(b[1] >> 4),
            "technique":    nibble_to_rating(b[1] & 0x0F),
            "balance":      nibble_to_rating(b[2] >> 4),
            "intelligence": nibble_to_rating(b[2] & 0x0F),
            "dribbling":    nibble_to_rating(b[3] >> 4),
            "jumping":      nibble_to_rating(b[3] & 0x0F),
            "stamina":      nibble_to_rating(b[4] & 0x0F),
        })

    out = {"team_id": team, "name": repo.team_names()[team]
           if team < len(repo.team_names()) else "Team %d" % team,
           "players": players}

    label = formation_label(rom, team)
    if label:
        out["_label"] = label
        for shape in repo.load()["formations"]:
            if shape["label"] == label:
                out["formation"] = shape["name"]
                break

    kit = read_kit(rom, team)
    if kit:
        out.update(kit)
    return out


def formation_label(rom: bytes, team: int) -> str | None:
    """What the select screen prints for this team's shape."""
    c = constants()
    ptr = c["ROM_FORMATION_PTRS"] + team * 2
    if ptr + 1 >= len(rom):
        return None
    addr = _word(rom, ptr)
    if not 0x8000 <= addr <= 0xFFFF:
        return None
    off = c["ROM_FORMATION_BANK"] * 0x8000 + (addr - 0x8000)
    if off >= len(rom):
        return None
    labels = ["4-5-1", "4-4-2", "4-3-3", "4-2-4", "3-5-2", "3-4-3", "3-3-4",
              "3-2-5", "2-5-3", "2-4-4", "2-3-5", "5-4-1", "5-3-2", "5-2-3",
              "1-5-4", "1-4-5"]
    value = rom[off]
    return labels[value] if value < len(labels) else None


def _bgr555_to_rgb(word: int) -> tuple[int, int, int]:
    r = (word & 31) * 255 // 31
    g = ((word >> 5) & 31) * 255 // 31
    b = ((word >> 10) & 31) * 255 // 31
    return r, g, b


def read_kit(rom: bytes, team: int) -> dict | None:
    """The lit shade of each part - the colour a pack would name."""
    c = constants()
    table = c.get("kit_record") or []
    if team >= len(table) or table[team] < 0:
        return None
    rec = c["ROM_KIT_BASE"] + table[team] * c["ROM_KIT_STRIDE"]
    if rec + c["ROM_KIT_STRIDE"] > len(rom):
        return None
    out = {}
    for key, slot, lit in (("shirt", c["ROM_KIT_SHIRT"], 2),
                           ("shorts", c["ROM_KIT_SHORTS"], 2),
                           ("socks", c["ROM_KIT_SOCKS"], 1)):
        rgb = _bgr555_to_rgb(_word(rom, rec + (slot + lit) * 2))
        out[key] = "#%02X%02X%02X" % rgb
    return out


def read_stadium(rom: bytes, slot: int) -> dict:
    c = constants()
    base = c["ROM_STADIUM_NAME_BASE"] + slot * c["ROM_STADIUM_NAME_BYTES"]
    # A ground is right-aligned in its field and patch_stadium re-aligns it
    # on the way back, so the blanks are the writer's business, not the
    # pack's. A player's name is the other way round - see decode_name.
    name = decode_name(rom[base:base + c["ROM_STADIUM_NAME_BYTES"]]).strip()
    pitch = c["ROM_STADIUM_PITCH_BASE"] + slot * 2
    return {"stadium_id": slot, "name": name,
            "display_name": name,
            "pitch_length": rom[pitch], "pitch_width": rom[pitch + 1]}


def team_count(rom: bytes) -> int:
    return constants().get("ROM_STOCK_TEAMS", 36)


def stadium_count(rom: bytes) -> int:
    return constants().get("ROM_STADIUMS", 8)


def summary(rom: bytes) -> list[tuple[int, str, str]]:
    """(id, name, first few players) for the team picker."""
    out = []
    for team in range(team_count(rom)):
        base = roster_base(rom, team)
        c = constants()
        who = ", ".join(
            decode_name(rom[base + p * c["ROM_NAME_BYTES"]:
                            base + (p + 1) * c["ROM_NAME_BYTES"]])
            for p in range(3))
        names = repo.team_names()
        out.append((team, names[team] if team < len(names) else "?", who))
    return out
