"""Convert named per-player looks into the pack format the loader reads.

A pack may ask for players by appearance:

    { "shirt_number": 10, "name": "Zidane",
      "skin": "light", "hair_color": "brown", "hair_style": 3 }

The loader knows nothing about `skin` or `hair_color`; it reads a team's two
`skin_tones` and each player's `skin_tone` nibble. This fitter closes the gap
offline, once, so nothing new is needed at runtime:

    python tools/fit_looks.py --pack mods/my_pack.json

It rewrites the pack in place (or with `--out`), replacing those two keys on
each requesting player with `skin_tone`, passing `hair_style` through, and
writing the winning palette pair as `skin_tones`. Teams without look requests
are left byte-for-byte untouched. The solver is `tools/looks.py` - the same
one the studio preview uses - so the warning a modder reads before a match is
the one the game's loader will produce.

The cartridge is located and hash-verified through
`tools/convert_rom_hack.py`'s `load_base`, so only the retail image is
accepted. Exit status is 1 only when the pack or the cartridge cannot be read;
a squad that cannot be shown exactly is a warning, because it is a real
limitation of two palettes and not a broken input.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from . import looks
    from .convert_rom_hack import ConversionError, load_base
    from .mod_studio.model import _strip_comments
except ImportError:                     # run from inside tools/, or as a script
    import looks
    from convert_rom_hack import ConversionError, load_base
    from mod_studio.model import _strip_comments


def _hex(rgb) -> str:
    return "#%02X%02X%02X" % tuple(rgb)


def _look_teams(pack):
    """The teams that actually carry look keys, in pack order."""
    out = []
    for team in pack.get("teams") or []:
        if not isinstance(team, dict):
            continue
        players = team.get("players")
        if not isinstance(players, list):
            continue
        if any(isinstance(p, dict) and any(key in p for key in looks.LOOK_KEYS)
               for p in players):
            out.append(team)
    return out


def _label(team, result) -> str:
    name = team.get("name") or result.name
    if result.team_id is None:
        return str(name)
    return "%s (%s)" % (result.team_id, name)


def fit_pack(pack, palettes, report=None):
    """Fit every team that carries look keys. Returns the number changed."""
    report = report or (lambda message: None)
    changed = 0
    for team in _look_teams(pack):
        result = looks.resolve(palettes, team)
        label = _label(team, result)

        tones = []
        for tone in result.tones:
            entry = {}
            if tone.emit_skin:
                entry["skin"] = _hex(tone.skin_rgb)
            entry["hair"] = _hex(tone.hair_rgb)
            tones.append(entry)
        if "skin_tones" in team:
            report("team %s: overwrites the pack's existing skin_tones"
                   % label)
        team["skin_tones"] = tones

        for slot, player in enumerate(team.get("players") or []):
            if not isinstance(player, dict):
                continue
            look = result.players[slot]
            if look.keeper and look.attempted:
                # The keeper's own palette is repainted from tone 0 by the
                # loader; its hair record has no hair slot at all. The nibble
                # is still pinned to 0 so the request leaves no trace.
                player["skin_tone"] = 0
            elif look.requested:
                player["skin_tone"] = look.tone
            for key in looks.LOOK_KEYS:
                player.pop(key, None)
        changed += 1

        for warning in result.warnings:
            report("team %s: %s" % (label, warning))
        missing = len(result.unmet)
        report("team %s: %d player(s) on tone 0, %d on tone 1; %s"
               % (label, result.tone_counts[0], result.tone_counts[1],
                  "all requests met" if not missing else
                  "%d cannot be shown exactly" % missing))
    return changed


def _serialise(pack, raw: bytes) -> bytes:
    """Plain JSON in the shipped packs' shape: two-space indent, same EOL."""
    text = json.dumps(pack, indent=2, ensure_ascii=False)
    if raw.endswith(b"\n"):
        text += "\n"
    if b"\r\n" in raw:
        text = text.replace("\n", "\r\n")
    return text.encode("utf-8")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pack", required=True, metavar="JSON",
                        help="the mod pack to fit")
    parser.add_argument("--base", metavar="ROM",
                        help="the retail cartridge (auto-discovered in the "
                             "working directory and hash-checked)")
    parser.add_argument("--out", metavar="JSON",
                        help="write here instead of rewriting --pack in place")
    parser.add_argument("--dry-run", action="store_true",
                        help="report what would change and write nothing")
    args = parser.parse_args(argv)

    pack_path = Path(args.pack)
    try:
        raw = pack_path.read_bytes()
        # The loader accepts // and /* */ in a pack, so hand-written files in
        # the wild carry them. Reuse the studio's stripper; strings are safe.
        pack = json.loads(_strip_comments(raw.decode("utf-8-sig")))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        print("fit_looks: cannot read %s: %s" % (pack_path, exc),
              file=sys.stderr)
        return 1
    if not isinstance(pack, dict):
        print("fit_looks: %s does not hold a pack object" % pack_path,
              file=sys.stderr)
        return 1

    teams = _look_teams(pack)
    palettes = None
    if teams:
        try:
            _, rom = load_base(args.base)
        except (ConversionError, OSError) as exc:
            print("fit_looks: %s" % exc, file=sys.stderr)
            return 1
        palettes = looks.Palettes(rom)

    changed = fit_pack(pack, palettes,
                       report=lambda message: print(message, file=sys.stderr))

    out_path = Path(args.out) if args.out else pack_path
    if args.dry_run:
        print("would write %s (%d team(s) fitted)" % (out_path, changed))
        return 0
    try:
        if changed:
            out_path.write_bytes(_serialise(pack, raw))
        elif out_path != pack_path:
            # Nothing to fit: copy the pack's bytes, so formatting survives.
            out_path.write_bytes(raw)
    except OSError as exc:
        print("fit_looks: cannot write %s: %s" % (out_path, exc),
              file=sys.stderr)
        return 1

    if changed:
        print("wrote %s (%d team(s) fitted)" % (out_path, changed))
    else:
        print("%s unchanged (no look requests)" % out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
