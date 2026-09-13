"""What the editor knows about the cartridge, read from the source of truth.

Every number in here is already written down somewhere in this repository -
the formation library, the measured kit table, the rating quantiser. Copying
them into the editor by hand is how an editor starts lying to you six months
later, so they are parsed out of the C instead.

A frozen .exe has no repository to read, so `bake()` writes a snapshot next to
this file and `load()` falls back to it. `tests/test_mod_studio.py` re-parses
the repository and asserts the snapshot still matches, which is what stops the
two drifting apart in silence.
"""
from __future__ import annotations

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BAKED = os.path.join(HERE, "baked.json")

# The quantiser in issd_mod_rom.c: ratings are 0-99 in a pack and four bits in
# the cartridge, and only 2..9 of those are used.
RATING_NIBBLE_MIN = 2
RATING_NIBBLE_MAX = 9

# What patch_name can encode. A full stop is one of them - the cartridge
# stores R.Banks with 0x54 - which it was not until reading a roster back
# out showed the encoder turning every one of them into a space.
NAME_CHARS = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "abcdefghijklmnopqrstuvwxyz"
              " .")
NAME_MAX = 8

POSITIONS = ("GK", "DF", "MF", "FW")
TACTICS = ("attacking", "balanced", "defensive")

# Slot 0 is the keeper, 1-10 the rest of the eleven, 11 the reserve keeper.
SQUAD_SLOTS = 20
STARTING_XI = 11
RESERVE_KEEPER = 11

ROLE_NAMES = {1: "DF", 2: "MF", 3: "FW", 5: "DF+", 6: "MF+"}


def rating_to_nibble(rating: int) -> int:
    """Exactly what the cartridge will store for a 0-99 rating."""
    rating = max(0, min(99, int(rating)))
    span = RATING_NIBBLE_MAX - RATING_NIBBLE_MIN
    n = RATING_NIBBLE_MIN + (rating * span + 49) // 99
    return min(n, RATING_NIBBLE_MAX)


def nibble_band(nibble: int) -> tuple[int, int]:
    """The 0-99 range that quantises to `nibble` - what the editor shows so a
    modder can see that 55 and 60 are the same player."""
    lo, hi = None, None
    for v in range(100):
        if rating_to_nibble(v) == nibble:
            if lo is None:
                lo = v
            hi = v
    return (lo if lo is not None else 0, hi if hi is not None else 99)


# --------------------------------------------------------------- parsing ---

def _repo_root() -> str | None:
    here = HERE
    for _ in range(5):
        here = os.path.dirname(here)
        if os.path.isfile(os.path.join(here, "ISSDNative", "issd_formation.c")):
            return here
    return None


def _read(root: str, *parts: str) -> str:
    with open(os.path.join(root, *parts), encoding="utf-8", errors="replace") as f:
        return f.read()


def parse_formations(root: str) -> list[dict]:
    """Name, printed label and the ten home positions of every shape."""
    src = _read(root, "ISSDNative", "issd_formation.c")

    labels = re.search(r"kLabelText\[16\]\s*=\s*\{(.*?)\}", src, re.S)
    label_text = re.findall(r'"([^"]+)"', labels.group(1))

    label_const = {}
    for name, value in re.findall(r"#define\s+(LBL_\w+)\s+(\d+)", src):
        label_const[name] = int(value)

    roles = {}
    for alias, macro in re.findall(r"#define\s+(DF|DFA|MF|MFA|FW)\s+(ISSD_ROLE_\w+)", src):
        roles[alias] = macro
    role_value = {}
    hdr = _read(root, "ISSDNative", "issd_formation.h")
    for macro, value in re.findall(r"(ISSD_ROLE_\w+)\s*=\s*(\d+)", hdr):
        role_value[macro] = int(value)

    body = src[src.index("kFormations[] = {"):]
    body = body[:body.index("\n};")]

    out = []
    # The slot block is brace-balanced, and a lazy match ending in three
    # closing braces stops one brace early - which silently dropped the
    # tenth player of every shape until a test counted them.
    entry = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*(LBL_\w+)\s*,\s*'
        r'\{((?:[^{}]*\{[^{}]*\})*[^{}]*)\}',
        re.S)
    for name, blurb, label, slots in entry.findall(body):
        pos = []
        for depth, width, role in re.findall(
                r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(DF|DFA|MF|MFA|FW)\s*\}", slots):
            pos.append({"depth": int(depth), "width": int(width),
                        "role": role_value[roles[role]]})
        out.append({"name": name, "blurb": blurb.strip(),
                    "label": label_text[label_const[label]], "slots": pos})
    return out


def parse_team_names(root: str) -> list[str]:
    """The 36 countries plus the six all-star sides.

    The cartridge draws these as graphics, so they are not text in it
    anywhere; kTeamName in issd_mod_rom.c is where they are written down,
    and this reads that rather than keeping a second copy."""
    src = _read(root, "ISSDNative", "issd_mod_rom.c")
    block = src[src.index("kTeamName[ROM_TEAMS] = {"):]
    block = block[:block.index("};")]
    return re.findall(r'"([^"]+)"', block)


def parse_stock_stadiums(root: str) -> list[dict]:
    """The eight the cartridge ships, from the modding guide's table."""
    doc = _read(root, "docs", "MODDING.md")
    rows = re.findall(r"^\|\s*(\d)\s*\|\s*([A-Z. ]+?)\s*\|\s*(\d+)\s*x\s*(\d+)\s*\|",
                      doc, re.M)
    return [{"id": int(i), "name": n.strip(), "length": int(a), "width": int(b)}
            for i, n, a, b in rows]


# ----------------------------------------------------------------- table ---

def gather(root: str) -> dict:
    # Imported late: cartridge.py reads this module back for the quantiser.
    from . import cartridge
    layout = cartridge._parse_c(root)
    layout["charset"] = {str(k): v for k, v in layout["charset"].items()}
    return {
        "formations": parse_formations(root),
        "team_names": parse_team_names(root),
        "stock_stadiums": parse_stock_stadiums(root),
        "cartridge": layout,
    }


def bake(root: str) -> dict:
    data = gather(root)
    with open(BAKED, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    return data


_cache: dict | None = None


def load() -> dict:
    """Live from the repository when there is one, from the snapshot when the
    editor is a frozen .exe."""
    global _cache
    if _cache is not None:
        return _cache
    root = None if getattr(sys, "frozen", False) else _repo_root()
    if root:
        try:
            _cache = gather(root)
            return _cache
        except Exception:            # a half-edited source must not stop the tool
            pass
    with open(BAKED, encoding="utf-8") as f:
        _cache = json.load(f)
    return _cache


def formation_names() -> list[str]:
    return [f["name"] for f in load()["formations"]]


def formation(name: str) -> dict | None:
    for f in load()["formations"]:
        if f["name"] == name:
            return f
    return None


def team_names() -> list[str]:
    return load()["team_names"]


STOCK_TEAMS = 36
ADDED_SLOTS = 6
STADIUM_MAX = 32
STOCK_STADIUMS = 8
PITCH_LENGTH = (100, 140)
PITCH_WIDTH = (64, 96)
PLATE_CHARS = 12
STADIUM_NAME_CHARS = 7
DISPLAY_NAME_CHARS = 12
KIT_RECORDS = 84

# Measured, not guessed: six teams wear one strip and two wear another, so
# recolouring one of them recolours the rest. See kKitRecord.
SHARED_KIT_TEAMS = {15, 18, 19, 20, 21, 22, 23, 26}


def main() -> int:
    root = _repo_root()
    if not root:
        print("not inside the repository")
        return 1
    data = bake(root)
    print("baked %d formations, %d teams, %d stadiums, %d offsets -> %s"
          % (len(data["formations"]), len(data["team_names"]),
             len(data["stock_stadiums"]),
             len(data["cartridge"]), BAKED))
    return 0


if __name__ == "__main__":
    # Run as a script there is no package to import from, so put the
    # parent on the path and come back in as one.
    if __package__ in (None, ""):
        sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
        sys.path.insert(0, os.path.dirname(HERE))
        from mod_studio import repo as _self
        raise SystemExit(_self.main())
    root = _repo_root()
    if not root:
        raise SystemExit("not inside the repository")
    data = bake(root)
    print("baked %d formations, %d teams, %d stadiums -> %s"
          % (len(data["formations"]), len(data["team_names"]),
             len(data["stock_stadiums"]), BAKED))
