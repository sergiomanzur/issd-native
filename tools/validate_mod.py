#!/usr/bin/env python3
"""Check a mod pack before the game sees it.

    python tools/validate_mod.py mods/world_cup_2026_mexico.json

Exits non-zero if anything is wrong. Written for whoever - or whatever - is
generating packs: most of what goes wrong with a pack is not a syntax error
but a silent one, where the file loads and the game plays as if you had not
written it.

The four that actually happen:

  * a team with no `team_id`, which names nothing
  * more players than the 20 slots a squad has
  * names longer than the 8 characters the cartridge stores
  * every rating between 70 and 90, which quantises to two or three steps and
    makes a whole squad play identically

The formation names come from ISSDNative/issd_formation.c so this file cannot
drift away from what the game accepts.
"""
import argparse
import json
import os
import re
import sys
from pathlib import Path

try:
    from .stadium_profile import resolve_asset, validate_pack_data
except ImportError:
    from stadium_profile import resolve_asset, validate_pack_data

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 36 squads. The select screen can offer 42 once a pack unlocks the
# seventh group, but the last six assemble their players from the group
# they belong to, so a pack's names cannot reach them.
TEAM_COUNT = 36
TEAM_SLOTS = 42
# The seventh group's six cells can be turned into teams of their own.
ADDED_SLOTS = 6
STADIUM_COUNT = 8          # what the cartridge ships
STADIUM_MAX = 32           # what its tables can be extended to
PLATE_CHARS = 12           # what fits on the select screen's name plate
STADIUM_NAME_CHARS = 7
# The range the cartridge's own stadiums span.
PITCH_LENGTH = (100, 140)
PITCH_WIDTH = (64, 96)
SQUAD_SLOTS = 20

# Kit palettes: records 0-83, the numbers a pack's `kit_record` may name.
# Measured on the retail image, all 86 home and away pointers name distinct
# records, so no team's strip is another's. A static "this team shares its
# strip" list was therefore wrong: the runtime counts real pointer sharing
# when it applies a strip, and a pack's tone-1 records are written fresh, so
# there is no pack-time sharing for this checker to predict.
KIT_RECORDS = 84

# What the host-drawn team plate fits.
PLATE_CHARS = 12
NAME_CHARS = 8
POSITIONS = ("GK", "DF", "MF", "FW")
# The cartridge has six position codes, not four: 3 and 5 sit between
# defence and midfield and between midfield and attack, and fifty-one of
# its seven hundred and twenty players have one. A pack may name the raw
# number so a squad read out of the cartridge and written back is
# unchanged rather than flattened to the nearest word.
POSITION_CODES = range(16)
TACTICS = ("attacking", "balanced", "normal", "defensive", "defence", "defense")
ATTRIBUTES = ("acceleration", "speed", "shooting", "technique", "balance",
              "intelligence", "dribbling", "jumping", "stamina", "goalkeeping")

# The cartridge stores each attribute in four bits and only ever uses 2..9.
RATING_MIN, RATING_MAX = 2, 9


def rating_to_step(rating):
    """Mirror of rating_to_nibble in ISSDNative/issd_mod_rom.c."""
    rating = max(0, min(99, int(rating)))
    span = RATING_MAX - RATING_MIN
    return min(RATING_MAX, RATING_MIN + (rating * span + 49) // 99)


# A caller that already knows the list can hand it over. The editor does:
# packaged as an .exe it has no repository to read, but it carries a
# snapshot of the same file, and a checker that shrugs at every formation
# name is worse than no checker.
FORMATION_NAMES = None


def known_formations():
    if FORMATION_NAMES:
        return list(FORMATION_NAMES)
    path = os.path.join(REPO, "ISSDNative", "issd_formation.c")
    try:
        with open(path, encoding="utf-8") as f:
            source = f.read()
    except OSError:
        return None
    names = re.findall(r'^\s*\{\s*"([^"]+)",\s*"', source, re.M)
    return names or None


class Report:
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.diagnostics = []

    def error(self, where, message):
        self.errors.append("%s: %s" % (where, message))

    def warn(self, where, message):
        self.warnings.append("%s: %s" % (where, message))


def check_name(report, where, name):
    if not isinstance(name, str):
        report.error(where, "name must be text")
        return
    if len(name) > NAME_CHARS:
        report.error(where, 'name "%s" is %d characters; the cartridge stores %d'
                     % (name, len(name), NAME_CHARS))
    # A full stop is a real character here: the cartridge is full of them
    # - R.Banks, T.Keegan - and stores one as 0x54.
    bad = sorted({c for c in name
                  if not (c.isascii() and (c.isalpha() or c in " ."))})
    if bad:
        report.error(where, 'name "%s" has characters the cartridge cannot show: %s'
                     % (name, " ".join(repr(c) for c in bad)))


def check_player(report, where, player, slot):
    if not isinstance(player, dict):
        report.error(where, "each player must be an object")
        return
    if "shirt_number" not in player:
        report.warn(where, "no shirt_number")
    if "name" in player:
        check_name(report, where, player["name"])
    else:
        report.warn(where, "no name; the cartridge's own will be left in place")

    position = player.get("position")
    if position is not None and position not in POSITIONS:
        number = str(position).strip()
        if number.isdigit() and int(number) in POSITION_CODES:
            pass          # a raw code: kept exactly, nothing to report
        else:
            report.error(where, 'position "%s" is not one of %s, nor a '
                                'position code 0-15'
                         % (position, ", ".join(POSITIONS)))
    if slot == 0 and position not in (None, "GK"):
        report.warn(where, "slot 0 is the goalkeeper, but this player is %s" % position)
    if slot == 11 and position not in (None, "GK"):
        report.warn(where, "slot 11 is the reserve goalkeeper, but this player is %s"
                    % position)

    for key in ATTRIBUTES:
        if key not in player:
            continue
        value = player[key]
        if not isinstance(value, int) or isinstance(value, bool):
            report.error(where, "%s must be a whole number" % key)
        elif not 0 <= value <= 99:
            report.error(where, "%s is %s; the scale is 0 to 99" % (key, value))

    # The appearance byte in two halves. The high nibble picks which of the
    # team's two skin tones the player wears: 0 is the kit record's own skin
    # and hair, 1 the team's alternate, and 2 and 3 the goalkeeper's and
    # referee's palettes. The low nibble is the hair actor variant, 0 to 13.
    # The names are kept so packs already written still load.
    for key in ("skin_tone", "hair_style"):
        if key in player:
            value = player[key]
            if (not isinstance(value, int) or isinstance(value, bool)
                    or not 0 <= value <= 15):
                report.error(where, "%s is %s; it is half a byte, 0 to 15"
                             % (key, value))
            elif key == "skin_tone" and value > 1:
                report.warn(where, "skin_tone is %s; it picks the palette "
                                   "the player is drawn with. Only 0 and 1 "
                                   "are the team's own tones; 2 is the "
                                   "goalkeeper's palette and 3 the "
                                   "referee's" % value)


def check_spread(report, where, players):
    """Ratings that all sit in the same band come out as the same player.

    Eight steps cover 0..99, so anything closer together than about 12 points
    is the same number once it reaches the cartridge.
    """
    for key in ATTRIBUTES:
        # Goalkeeping is two clusters by design - the keeper and everyone
        # else - so a narrow spread there is correct, not a mistake.
        if key == "goalkeeping":
            continue
        values = [p[key] for p in players
                  if isinstance(p, dict) and isinstance(p.get(key), int)]
        if len(values) < 6:
            continue
        steps = {rating_to_step(v) for v in values}
        if len(steps) <= 2:
            report.warn(where,
                        "%s uses %d of the 8 steps the cartridge has (%s) - "
                        "these players will feel identical"
                        % (key, len(steps),
                           "%d..%d" % (min(values), max(values))))


def check_team(report, team, index, formations, seen_ids):
    where = "team[%d]" % index
    if not isinstance(team, dict):
        report.error(where, "each team must be an object")
        return

    added = bool(team.get("new_team"))
    team_id = team.get("team_id")
    if added and team_id is not None:
        report.warn(where, "new_team gives it one of the six added slots, "
                           "so team_id is ignored")
        team_id = None
    if added:
        where = "added team %r" % (team.get("name") or "?",)
    elif team_id is None:
        report.error(where, "no team_id and no new_team, so it names no "
                            "team and is dropped")
    elif not isinstance(team_id, int) or isinstance(team_id, bool):
        report.error(where, "team_id must be a number, not %r" % (team_id,))
    elif not 0 <= team_id < TEAM_SLOTS:
        report.error(where, "team_id %d does not exist; there are %d team slots "
                            "(0 to %d); to add one, say new_team"
                     % (team_id, TEAM_SLOTS, TEAM_SLOTS - 1))
    else:
        where = "team %d" % team_id
        if team_id in seen_ids:
            report.error(where, "listed twice in this pack; the later one wins")
        seen_ids.add(team_id)
        if team_id >= TEAM_COUNT and team.get("players"):
            report.error(where, "team_id %d is an all-star side, which picks "
                                "its players from its group. To add a team "
                                "with a squad of its own, drop team_id and "
                                "say \"new_team\": true" % team_id)

    formation = team.get("formation")
    if formation is not None:
        if formations is None:
            report.warn(where, "cannot check the formation name from here")
        elif formation not in formations:
            report.error(where, 'formation "%s" is not one the game knows.\n'
                                '    Known: %s'
                         % (formation, ", ".join(formations)))

    plate = team.get("plate_name")
    if plate is not None:
        if not isinstance(plate, str):
            report.error(where, "plate_name must be text")
        elif len(plate) > PLATE_CHARS:
            report.error(where, 'plate_name "%s" is %d characters; the plate'
                                ' the host draws fits %d'
                         % (plate, len(plate), PLATE_CHARS))

    photo = team.get("photo")
    if photo is not None and not isinstance(photo, str):
        report.error(where, "photo must be the name of a .bmp beside the pack")
    elif isinstance(photo, str) and not photo.lower().endswith(".bmp"):
        report.error(where, "photo must be a 32-bit .bmp; %r is not" % (photo,))

    flag = team.get("flag")
    if flag is not None and not isinstance(flag, str):
        report.error(where, "flag must be the name of a .bmp beside the pack")
    elif isinstance(flag, str) and not flag.lower().endswith(".bmp"):
        report.error(where, "flag must be a 32-bit .bmp; %r is not" % (flag,))

    for key in ("shirt", "shorts", "socks"):
        colour = team.get(key)
        if colour is None:
            continue
        text = colour[1:] if isinstance(colour, str) and colour.startswith("#") else colour
        if (not isinstance(colour, str) or len(str(text)) != 6 or
                any(c not in "0123456789abcdefABCDEF" for c in str(text))):
            report.error(where, '%s must be a colour like "#C8102E", not %r'
                         % (key, colour))

    kit = team.get("kit_record")
    if kit is not None:
        if not isinstance(kit, int) or isinstance(kit, bool):
            report.error(where, "kit_record must be a number")
        elif not 0 <= kit < KIT_RECORDS:
            report.error(where, "kit_record %d does not exist; there are %d"
                         % (kit, KIT_RECORDS))

    stripes = team.get("stripes")
    if stripes is not None and not isinstance(stripes, bool):
        report.error(where, "stripes must be true or false")

    # The two skin/hair tones the cartridge offers a team. Index 0 is the
    # default carried in the kit record's colours 1-6, index 1 the alternate
    # reached through the per-team pointer tables; a player's skin_tone
    # nibble is the index. A part left out keeps the cartridge's own.
    tones = team.get("skin_tones")
    one_tone = False
    if "skin_tones" in team:
        if not isinstance(tones, list):
            report.error(where, "skin_tones must be an array of one or two "
                                "tone objects")
        elif not 1 <= len(tones) <= 2:
            report.error(where, "skin_tones has %d entries; the cartridge "
                                "offers two tones, one per skin_tone value"
                         % len(tones))
        else:
            one_tone = len(tones) == 1
            for tone_index, tone in enumerate(tones):
                tone_where = "%s skin_tones[%d]" % (where, tone_index)
                if not isinstance(tone, dict):
                    report.error(tone_where,
                                 "each skin tone must be an object")
                    continue
                given = [key for key in ("skin", "hair")
                         if tone.get(key) is not None]
                if not given:
                    report.error(tone_where, "gives neither skin nor hair, so "
                                             "it changes nothing")
                for key in given:
                    colour = tone[key]
                    text = (colour[1:] if isinstance(colour, str)
                            and colour.startswith("#") else colour)
                    if (not isinstance(colour, str) or len(str(text)) != 6 or
                            any(c not in "0123456789abcdefABCDEF"
                                for c in str(text))):
                        report.error(tone_where, '%s must be a colour like '
                                                 '"#EAC086", not %r'
                                     % (key, colour))

    tactics = team.get("tactics", team.get("strategy"))
    if tactics is not None and str(tactics).lower() not in TACTICS:
        report.error(where, 'tactics "%s" is not attacking, balanced or defensive'
                     % tactics)

    players = team.get("players")
    if players is None:
        if formation is None:
            report.warn(where, "no players and no formation, so it changes nothing")
        return
    if not isinstance(players, list):
        report.error(where, "players must be an array")
        return
    if len(players) > SQUAD_SLOTS:
        report.error(where, "%d players; a squad has %d slots and the rest are "
                            "ignored" % (len(players), SQUAD_SLOTS))
    for slot, player in enumerate(players):
        check_player(report, "%s slot %d" % (where, slot), player, slot)
    check_spread(report, where, players[:SQUAD_SLOTS])

    # A team that authored one tone but fields players on tone 1 gets the
    # cartridge's tone-1 palette for them, which for most teams only
    # overrides hair. Say so rather than let the pack look half applied.
    if one_tone:
        users = sum(1 for p in players[:SQUAD_SLOTS]
                    if isinstance(p, dict) and p.get("skin_tone") == 1)
        if users:
            who = ("a player uses" if users == 1
                   else "%d players use" % users)
            report.warn(where, "%s skin_tone 1, but the team gives one tone; "
                               "the cartridge's own tone-1 palette is used "
                               "instead, which for most teams only changes "
                               "hair" % who)


def check_stadium(report, st, index, seen_ids):
    where = "stadium[%d]" % index
    if not isinstance(st, dict):
        report.error(where, "each stadium must be an object")
        return

    sid = st.get("stadium_id")
    if sid is None:
        report.error(where, "no stadium_id, so it names no stadium")
    elif not isinstance(sid, int) or isinstance(sid, bool):
        report.error(where, "stadium_id must be a number")
    elif not 0 <= sid < STADIUM_MAX:
        report.error(where, "stadium_id %d does not exist; there are at most "
                            "%d stadium slots (0 to %d)"
                     % (sid, STADIUM_MAX, STADIUM_MAX - 1))
    else:
        where = "stadium %d" % sid
        if sid in seen_ids:
            report.error(where, "listed twice in this pack")
        seen_ids.add(sid)

    display = st.get("display_name")
    if display is not None:
        if not isinstance(display, str):
            report.error(where, "display_name must be text")
        elif len(display) > PLATE_CHARS:
            report.error(where, 'display_name "%s" is %d characters; the plate'
                                ' holds %d'
                         % (display, len(display), PLATE_CHARS))

    name = st.get("name")
    if name is not None:
        if not isinstance(name, str):
            report.error(where, "name must be text")
        else:
            if len(name) > STADIUM_NAME_CHARS:
                report.error(where, 'name "%s" is %d characters; the plate '
                                    'holds %d'
                             % (name, len(name), STADIUM_NAME_CHARS))
            bad = sorted({c for c in name
                          if not (c.isascii() and (c.isalpha() or c in " ."))})
            if bad:
                report.error(where, 'name "%s" has characters the cartridge '
                                    'cannot show: %s'
                             % (name, " ".join(repr(c) for c in bad)))

    for key, limits in (("pitch_length", PITCH_LENGTH),
                        ("pitch_width", PITCH_WIDTH)):
        if key not in st:
            continue
        v = st[key]
        if not isinstance(v, int) or isinstance(v, bool):
            report.error(where, "%s must be a whole number" % key)
        elif not limits[0] <= v <= limits[1]:
            report.warn(where, "%s %s is outside the %d-%d the cartridge's own "
                               "stadiums use, and will be clamped"
                        % (key, v, limits[0], limits[1]))

    if (name is None and display is None and "pitch_length" not in st
            and "pitch_width" not in st):
        report.warn(where, "changes nothing")


def check_stadium_count(report, path, pack, stadiums):
    slots = pack.get("stadium_count")
    highest = max((st.get("stadium_id", -1) for st in stadiums
                   if isinstance(st, dict)), default=-1)
    if slots is None:
        if highest >= STADIUM_COUNT:
            report.error(path, 'stadium_id %d needs "stadium_count": %d; the'
                               ' cartridge ships %d'
                         % (highest, highest + 1, STADIUM_COUNT))
        return
    if not isinstance(slots, int) or isinstance(slots, bool):
        report.error(path, "stadium_count must be a number")
        return
    if not STADIUM_COUNT <= slots <= STADIUM_MAX:
        report.error(path, "stadium_count %d is outside %d to %d"
                     % (slots, STADIUM_COUNT, STADIUM_MAX))
        return
    if highest >= slots:
        report.error(path, "stadium_id %d needs stadium_count of at least %d"
                     % (highest, highest + 1))
    # Past the cartridge's own eight there is no plate graphic, so the host
    # draws one - but only for a slot the pack actually names.
    named = {st.get("stadium_id") for st in stadiums
             if isinstance(st, dict) and (st.get("name") or st.get("display_name"))}
    for sid in range(STADIUM_COUNT, slots):
        if sid not in named:
            report.warn(path, "stadium %d is added but never named, so its"
                              " plate will be an unused cartridge graphic" % sid)


def check_cartridge_file(report, key, name, pack_dir):
    """One of the two substitutions: a headerless image or an IPS patch."""
    suffix = ".sfc" if key == "image" else ".ips"
    if not isinstance(name, str) or not name:
        report.error("cartridge", "%s must be the name of a %s beside the pack"
                     % (key, suffix))
        return
    if not name.lower().endswith(suffix):
        if key == "image" and name.lower().endswith(".smc"):
            report.error("cartridge", "image %r has a copier header; only a "
                                      "headerless .sfc is accepted, so drop "
                                      "the 512-byte header" % (name,))
        elif key == "image":
            report.error("cartridge", "image must be a headerless .sfc; %r is "
                                      "not" % (name,))
        else:
            report.error("cartridge", "patch must be an .ips; %r is not"
                         % (name,))
        return
    try:
        resolve_asset(pack_dir, name)
    except ValueError as exc:
        if str(exc) == "Asset file does not exist":
            report.error("cartridge", "%s %r is not beside the pack"
                         % (key, name))
        else:
            report.error("cartridge", "%s %r: %s" % (key, name, exc))


def check_cartridge(report, pack, pack_dir):
    """A cartridge pack swaps the image; regular packs may layer on top."""
    if "cartridge" not in pack:
        return
    cartridge = pack["cartridge"]
    if not isinstance(cartridge, dict):
        report.error("cartridge", "must be an object, not %r" % (cartridge,))
        return

    given = [key for key in ("image", "patch")
             if cartridge.get(key) is not None]
    if len(given) != 1:
        report.error("cartridge", "needs exactly one of image or patch, not %s"
                     % ("both" if given else "neither"))
    for key in given:
        check_cartridge_file(report, key, cartridge[key], pack_dir)

    sha = cartridge.get("sha256")
    if sha is not None:
        if not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{64}", sha):
            report.error("cartridge", "sha256 must be 64 lowercase hex "
                                      "characters, not %r" % (sha,))


def validate(path, report):
    try:
        with open(path, encoding="utf-8") as f:
            pack = json.load(f)
    except FileNotFoundError:
        report.error(path, "no such file")
        return
    except json.JSONDecodeError as exc:
        report.error(path, "line %d: %s" % (exc.lineno, exc.msg))
        return

    validate_data(pack, report, Path(path).resolve().parent, path)


def validate_data(pack, report, pack_dir, path='<draft>'):
    """Check in-memory drafts using the actual pack directory for resources."""
    if not isinstance(pack, dict):
        report.error(path, "the file must hold one object")
        return
    if not pack.get("name"):
        report.error(path, 'no "name"; it is how the menu lists the pack and how '
                           "the saved selection finds it again")

    check_cartridge(report, pack, pack_dir)

    stadiums = pack.get("stadiums")
    if isinstance(stadiums, list):
        seen_st = set()
        for i, st in enumerate(stadiums):
            check_stadium(report, st, i, seen_st)
    elif stadiums is not None:
        report.error(path, '"stadiums" must be an array')
    check_stadium_count(report, path, pack,
                        stadiums if isinstance(stadiums, list) else [])
    validate_pack_data(pack, Path(pack_dir).resolve(), report)

    teams = pack.get("teams")
    if teams is None:
        if not stadiums and "cartridge" not in pack:
            report.error(path, 'no "teams", "stadiums" or "cartridge", so the '
                               'pack changes nothing')
        return
    if not isinstance(teams, list):
        report.error(path, '"teams" must be an array')
        return
    if not teams and not stadiums and "cartridge" not in pack:
        report.warn(path, '"teams" is empty, so the pack changes nothing')

    formations = known_formations()
    seen = set()
    added = sum(1 for t in teams if isinstance(t, dict) and t.get("new_team"))
    if added > ADDED_SLOTS:
        report.error("teams", "%d added teams; there are %d slots for them"
                     % (added, ADDED_SLOTS))

    for i, team in enumerate(teams):
        check_team(report, team, i, formations, seen)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("packs", nargs="+", help="one or more .json mod packs")
    ap.add_argument("--strict", action="store_true",
                    help="treat warnings as failures too")
    args = ap.parse_args()

    failed = False
    for path in args.packs:
        report = Report()
        validate(path, report)
        print("== %s" % path)
        for message in report.errors:
            print("   ERROR   %s" % message)
        for message in report.warnings:
            print("   warning %s" % message)
        if not report.errors and not report.warnings:
            print("   looks good")
        if report.errors or (args.strict and report.warnings):
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
