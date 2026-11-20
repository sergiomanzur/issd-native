"""The pictures.

Things worth drawing rather than describing, because in each case the number
in the file is not what the player will see:

  - a strip, because the cartridge stores five bits per channel and shades the
    other two tones itself, so #C8102E is not the red that reaches the pitch;
  - a formation, because ten pairs of signed numbers are not a shape;
  - a pitch, because 138 x 90 and 114 x 74 are the same two numbers until you
    put them side by side;
  - a player's appearance byte, because it does not do what its name says;
  - a team's two skin tones and a player's look card, because the pack's hex
    is not the colour the loader stores and a hair style is a graphic, not a
    number.

Two of these are photographs of the game rather than drawings of it. The
pitch a formation sits on is the plan view the stadium screen draws, one per
ground, mowing pattern and all; the player shown beside the appearance byte
is that player, with that byte, in a match. Both are cut out of screenshots
by tools/capture_editor_assets.py and live in assets/. A drawn rectangle was
the right shape and the wrong pitch, and the head the editor used to draw
for skin tone was of something the cartridge does not have.
"""
from __future__ import annotations

from PIL import Image, ImageDraw

import os

from . import model, repo

try:                                    # imported as tools.mod_studio.preview
    from .. import looks
except ImportError:                     # run from inside tools/
    import looks

ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")
_cache: dict = {}


def _asset(*parts):
    """A captured picture, or None when the build has none."""
    key = parts
    if key not in _cache:
        path = os.path.join(ASSETS, *parts)
        try:
            _cache[key] = Image.open(path).convert("RGB")
        except Exception:
            _cache[key] = None
    return _cache[key]

GRASS_DARK = (28, 104, 40)
GRASS_LIGHT = (38, 124, 48)
LINE = (236, 240, 236)
INK = (24, 26, 32)


def _pitch(size, margin=8, stripes=10, stadium=None):
    """The ground, photographed if we have it and drawn if we do not.

    The capture is a plan view from the stadium screen, 62x46, so it is
    turned upright and enlarged. Its markings are already right, which is why
    nothing is drawn over it.
    """
    w, h = size
    if stadium is not None:
        shot = _asset("pitch", "slot%d.png" % (int(stadium) % 8))
        if shot is not None:
            im = shot.rotate(90, expand=True).resize(size, Image.NEAREST)
            return im, ImageDraw.Draw(im)

    im = Image.new("RGB", size, GRASS_DARK)
    d = ImageDraw.Draw(im)
    band = h / stripes
    for i in range(stripes):
        if i % 2:
            d.rectangle([0, i * band, w, (i + 1) * band - 1], fill=GRASS_LIGHT)
    d.rectangle([margin, margin, w - margin - 1, h - margin - 1], outline=LINE)
    d.line([margin, h / 2, w - margin - 1, h / 2], fill=LINE)
    r = min(w, h) * 0.13
    d.ellipse([w / 2 - r, h / 2 - r, w / 2 + r, h / 2 + r], outline=LINE)
    box_w, box_h = w * 0.34, h * 0.16
    for top in (margin, h - margin - box_h):
        d.rectangle([w / 2 - box_w / 2, top, w / 2 + box_w / 2, top + box_h],
                    outline=LINE)
    return im, d


def formation(name: str, tactics: str = "balanced", size=(210, 260),
              stadium: int = 0) -> Image.Image:
    """Ten outfield players where the shape puts them, keeper at the back.

    Attacking and defensive shift every line by six the way
    issd_formation_apply_tactics does, so the preview moves when the pack does.

    The grass underneath is the ground's own, taken from the stadium screen -
    every stadium is mowed differently and it is the quickest way to see
    which one a pack has selected.
    """
    im, d = _pitch(size, stadium=stadium)
    w, h = size
    shape = repo.formation(name)
    if not shape:
        d.text((10, h / 2), "unknown shape", fill=LINE)
        return im

    shift = {"attacking": -6, "defensive": 6}.get((tactics or "").lower(), 0)
    colour = {1: (86, 152, 240), 5: (120, 180, 250),
              2: (250, 214, 92), 6: (252, 232, 150),
              3: (240, 96, 96)}

    # The keeper is not in the record; he stands on his line.
    d.ellipse([w / 2 - 7, h - 30, w / 2 + 7, h - 16], fill=(120, 230, 140),
              outline=INK)
    d.text((w / 2 - 3, h - 27), "1", fill=INK)

    for i, slot in enumerate(shape["slots"]):
        depth = slot["depth"] + shift
        # depth runs from the back line forward; width is signed across.
        y = h * 0.72 - (h * 0.52) * (_row_of(shape, i) / 3.0) + depth * 0.9
        x = w / 2 + slot["width"] * (w * 0.0125)
        x = max(14, min(w - 14, x))
        y = max(24, min(h - 40, y))
        c = colour.get(slot["role"], (220, 220, 220))
        d.ellipse([x - 7, y - 7, x + 7, y + 7], fill=c, outline=INK)
        d.text((x - 4, y - 4), str(i + 2), fill=INK)

    d.rectangle([0, 0, w - 1, h - 1], outline=(70, 78, 86))
    return im


def _row_of(shape: dict, index: int) -> int:
    """0 for the back line, 1 midfield, 2 attack - taken from the role so the
    drawing follows the record rather than a guess about the label."""
    role = shape["slots"][index]["role"]
    return {1: 0, 5: 0, 2: 1, 6: 1, 3: 2}.get(role, 1)


def kit(shirt, shorts, socks, size=(150, 176)) -> Image.Image:
    """A strip in the three shades the cartridge will actually store."""
    w, h = size
    im = Image.new("RGB", size, (238, 240, 244))
    d = ImageDraw.Draw(im)

    s = model.parse_colour(shirt) or (200, 200, 200)
    p = model.parse_colour(shorts) or (60, 60, 60)
    k = model.parse_colour(socks) or (240, 240, 240)

    lit, mid, dark = (model.shade(s, 100), model.shade(s, 77), model.shade(s, 60))
    plit, pmid, pdark = (model.shade(p, 100), model.shade(p, 77), model.shade(p, 60))
    klit, kdark = (model.shade(k, 95), model.shade(k, 62))

    cx = w // 2
    # shirt: body in the lit shade, one sleeve mid, the other dark, so all
    # three tones the cartridge keeps are on screen at once.
    d.polygon([(cx - 30, 24), (cx - 16, 16), (cx + 16, 16), (cx + 30, 24),
               (cx + 30, 40), (cx + 20, 40), (cx + 20, 92),
               (cx - 20, 92), (cx - 20, 40), (cx - 30, 40)], fill=lit)
    d.polygon([(cx - 30, 24), (cx - 20, 20), (cx - 20, 40), (cx - 30, 40)], fill=mid)
    d.polygon([(cx + 30, 24), (cx + 20, 20), (cx + 20, 40), (cx + 30, 40)], fill=dark)
    d.polygon([(cx - 8, 16), (cx + 8, 16), (cx + 5, 24), (cx - 5, 24)],
              fill=(232, 226, 214))

    d.rectangle([cx - 20, 94, cx - 2, 124], fill=plit)
    d.rectangle([cx + 2, 94, cx + 20, 124], fill=pmid)
    d.rectangle([cx - 2, 94, cx + 2, 124], fill=pdark)

    d.rectangle([cx - 18, 128, cx - 6, 152], fill=klit)
    d.rectangle([cx + 6, 128, cx + 18, 152], fill=kdark)
    d.rectangle([cx - 20, 152, cx - 4, 158], fill=INK)
    d.rectangle([cx + 4, 152, cx + 20, 158], fill=INK)

    # The three tones spelled out, because on a small shirt they are hard
    # to tell apart and the whole point is that the cartridge picks two of
    # them for you.
    y = 160
    for i, tones in enumerate(((lit, mid, dark), (plit, pmid, pdark),
                               (klit, kdark))):
        for j, tone in enumerate(tones):
            x = 8 + i * 48 + j * 13
            d.rectangle([x, y, x + 11, y + 8], fill=tone,
                        outline=(150, 154, 162))
    d.rectangle([0, 0, w - 1, h - 1], outline=(180, 184, 192))
    return im


def pitch_size(length: int, width: int, size=(210, 140)) -> Image.Image:
    """This ground against the largest the cartridge has, to scale."""
    w, h = size
    im = Image.new("RGB", size, (238, 240, 244))
    d = ImageDraw.Draw(im)

    max_l, max_w = repo.PITCH_LENGTH[1], repo.PITCH_WIDTH[1]
    length = max(1, int(length or 0))
    width = max(1, int(width or 0))

    pad = 12
    full_w, full_h = w - pad * 2, h - pad * 2
    d.rectangle([pad, pad, pad + full_w, pad + full_h], outline=(190, 194, 202))

    bw = full_w * min(length, max_l) / max_l
    bh = full_h * min(width, max_w) / max_w
    x0, y0 = pad, pad + (full_h - bh)
    sub = Image.new("RGB", (max(4, int(bw)), max(4, int(bh))), GRASS_DARK)
    sd = ImageDraw.Draw(sub)
    for i in range(0, sub.height, 10):
        sd.rectangle([0, i, sub.width, i + 4], fill=GRASS_LIGHT)
    sd.rectangle([0, 0, sub.width - 1, sub.height - 1], outline=LINE)
    sd.line([sub.width / 2, 0, sub.width / 2, sub.height], fill=LINE)
    im.paste(sub, (int(x0), int(y0)))

    d.text((pad, 2), "%d x %d yards" % (length, width), fill=(60, 64, 72))
    d.text((pad, h - 11), "against %d x %d, the largest" % (max_l, max_w),
           fill=(130, 136, 146))
    return im


# What the appearance byte really is. The high nibble chooses which of a
# team's two skins the sprite is drawn with, and 2 and 3 are not skin tones at
# all: they select the goalkeeper's palette and the referee's, which is why a
# footballer drawn with one looks orange or green. The low nibble picks a hair
# actor variant; the cartridge's own squads use 0 to 13.
APPEARANCE = {
    0: ("Tone 0 (team default skin)", "the team's own skin and hair"),
    1: ("Tone 1 (team alternate skin)", "the team's second tone, when a pack "
                                        "gives one"),
    2: ("Goalkeeper palette", "the colours a goalkeeper is drawn with, not a "
                              "skin tone"),
    3: ("Referee palette", "the colours the referee is drawn with, not a skin "
                           "tone"),
}


def player_appearance(hair_colour: int, size=(104, 176)) -> Image.Image:
    """That player, with that byte, in a match.

    Four screenshots of the same frame with nothing changed but the
    appearance byte, so what is different between them is only ever what the
    byte did. If the build has no captures, say so rather than draw
    something invented.
    """
    value = int(hair_colour or 0) & 3
    shot = _asset("player", "%02X.png" % (value << 4))
    if shot is None:
        im = Image.new("RGB", size, (30, 34, 40))
        d = ImageDraw.Draw(im)
        d.text((8, size[1] / 2 - 4), "no capture", fill=(150, 150, 160))
        return im
    scale = min(size[0] / shot.width, size[1] / shot.height)
    out = shot.resize((max(1, int(shot.width * scale)),
                       max(1, int(shot.height * scale))), Image.NEAREST)
    im = Image.new("RGB", size, (30, 34, 40))
    im.paste(out, ((size[0] - out.width) // 2, (size[1] - out.height) // 2))
    ImageDraw.Draw(im).rectangle([0, 0, size[0] - 1, size[1] - 1],
                                outline=(70, 78, 86))
    return im


def stat_bar(value: int, size=(150, 14)) -> Image.Image:
    """A rating and, behind it, the band it quantises into - the thing that is
    genuinely surprising about this cartridge's stats."""
    w, h = size
    im = Image.new("RGB", size, (238, 240, 244))
    d = ImageDraw.Draw(im)
    nibble = repo.rating_to_nibble(value)
    lo, hi = repo.nibble_band(nibble)

    d.rectangle([0, 0, w - 1, h - 1], fill=(226, 229, 235))
    d.rectangle([w * lo / 99, 0, w * hi / 99, h - 1], fill=(206, 216, 234))
    ramp = (nibble - repo.RATING_NIBBLE_MIN) / (
        repo.RATING_NIBBLE_MAX - repo.RATING_NIBBLE_MIN)
    fill = (int(210 - 150 * ramp), int(90 + 120 * ramp), int(90 + 60 * ramp))
    d.rectangle([0, 4, max(2, w * value / 99), h - 5], fill=fill)
    d.rectangle([0, 0, w - 1, h - 1], outline=(190, 194, 202))
    return im


# ------------------------------------------------- skin and hair, resolved ----
# The hair graphics and the decode below are a port of the contributed patch's
# `tools/mod_studio/looks.py` (`decode_tiles` / `hair_graphic` /
# `standard_head`). Its offsets are measured, not guessed: $81:ED6D holds
# thirteen 24-bit pointers four bytes apart, each naming a 24-frame strip of
# 4bpp tiles at DATA_97CD58, and $A9:A7C4 is the standard body sprite, whose
# head is the middle two tiles of its top row. Overlay pixels are drawn almost
# entirely in palette index 1, the hair colour.
HAIR_TABLE = 0x0ED6D        # $81:ED6D: thirteen 24-bit pointers, 4 bytes apart
HAIR_STYLES = 13            # 14 and 15 are written but read past the table
HAIR_FRAME = 3              # the frame the studio shows: fourth of 24
BODY_SPRITE = 0xA9A7C4      # $A9:A7C4: a two-byte length, then the tiles

BACKDROP = (236, 240, 246)
TEXT = (60, 64, 72)
MUTED_TEXT = (96, 102, 116)

# What the pair picture says when it has had to invent a colour. The caption
# beside such a shade says "(assumed)"; this is the sentence underneath.
NO_CARTRIDGE_NOTE = ("No cartridge loaded: a colour marked (assumed) is the "
                     "editor's default, not this team's palette.")
NO_RECORD_NOTE = ("This team has no cartridge record: a colour marked "
                  "(assumed) is the editor's default, not this team's palette.")
TWO_TONES_NOTE = ("The cartridge can draw two skin and hair combinations per "
                  "team; every player is on one of them.")

# ISSDNative/issd_mod_rom.c:write_skin_steps. The loader does not copy a
# palette, it rebuilds one at these percentages of the authored colour, so a
# preview that shows the authored hex would be showing a colour the game never
# draws.
SKIN_STEPS = (46, 68, 82, 89, 100)


def _lorom(address: int) -> int:
    """A 24-bit SNES address as a file offset."""
    return ((address >> 16) & 0x7F) * 0x8000 + (address & 0x7FFF)


def decode_tiles(data: bytes, bpp: int, columns: int) -> list:
    """SNES planar tiles as rows of palette indices."""
    size = bpp * 8
    if bpp not in (2, 4, 8) or len(data) % size or (len(data) // size) % columns:
        raise ValueError("bad tile data")
    rows = len(data) // size // columns
    out = []
    for y in range(rows * 8):
        row = []
        for x in range(columns * 8):
            tile = (y // 8) * columns + x // 8
            colour = 0
            for plane in range(bpp):
                idx = tile * size + (plane // 2) * 16 + (y % 8) * 2 + plane % 2
                colour |= ((data[idx] >> (7 - x % 8)) & 1) << plane
            row.append(colour)
        out.append(row)
    return out


def hair_graphic(rom, style: int):
    """The overlay for hair_style 1-13 as 8x8 palette indices, or None."""
    if rom is None:
        return None
    try:
        style = int(style or 0)
    except (TypeError, ValueError):
        return None
    if not 1 <= style <= HAIR_STYLES:
        return None
    try:
        at = HAIR_TABLE + (style - 1) * 4
        address = rom[at] | (rom[at + 1] << 8) | (rom[at + 2] << 16)
        off = _lorom(address) + HAIR_FRAME * 32
        return decode_tiles(bytes(rom[off:off + 32]), 4, 1)
    except (IndexError, ValueError):
        return None


def standard_head(rom):
    """hair_style 0: the head of the standard body sprite, shirt cropped out."""
    if rom is None:
        return None
    try:
        off = _lorom(BODY_SPRITE) + 2
        body = decode_tiles(bytes(rom[off:off + 64]), 4, 2)
    except (IndexError, ValueError):
        return None
    return [[0 if px >= 7 else px for px in row[4:12]] for row in body]


def head(rom, style: int):
    """The 8x8 head for a hair style, or None when there is no graphic."""
    try:
        style = int(style or 0)
    except (TypeError, ValueError):
        return None
    return standard_head(rom) if style == 0 else hair_graphic(rom, style)


# --------------------------------------------------------------- the tones --
def skin_ramp(rgb) -> list:
    """The five stored shades write_skin_steps derives from an authored skin."""
    return [looks.to_rgb(looks.to555(
        tuple(min(255, channel * step // 100) for channel in rgb)))
        for step in SKIN_STEPS]


def _stored(rgb):
    """An authored colour as the cartridge stores it: five bits a channel."""
    return looks.to_rgb(looks.to555(rgb))


def _team_id(team):
    tid = team.get("team_id") if isinstance(team, dict) else None
    return tid if isinstance(tid, int) and not isinstance(tid, bool) else None


def _tone_entries(team):
    entries = team.get("skin_tones") if isinstance(team, dict) else None
    return entries if isinstance(entries, list) else []


def _has_looks(team) -> bool:
    """Whether any player still carries the named look vocabulary."""
    players = team.get("players") if isinstance(team, dict) else None
    for player in players or []:
        if isinstance(player, dict) and any(key in player for key in looks.LOOK_KEYS):
            return True
    return False


def _nibbles(team) -> dict:
    """How many players the pack already puts on each tone."""
    counts = {0: 0, 1: 0}
    players = team.get("players") if isinstance(team, dict) else None
    for player in players or []:
        if not isinstance(player, dict):
            continue
        try:
            tone = int(player.get("skin_tone", 0) or 0)
        except (TypeError, ValueError):
            tone = 0
        if tone in counts:
            counts[tone] += 1
    return counts


def tone_report(team: dict, palettes=None) -> list:
    """What each of a team's two tones is drawn with, and who wears it.

    A tone with no `skin` part keeps the cartridge's own ramp - the fitter
    writes such an entry when it leaves the team unchanged - so the report
    reads it when a `Palettes` is supplied. With no cartridge there is no ramp
    to read and the editor can only assume one; then the report says so
    (`skin_source` / `hair_source` are "default", never "cartridge").
    Counts and the closest-match warning come from `tools/looks.py`, the same
    solver the fitter uses; they are only meaningful while look keys remain in
    the pack.
    """
    team = team if isinstance(team, dict) else {}
    entries = _tone_entries(team)
    tid = _team_id(team)
    exact = palettes is not None and tid is not None and palettes.has_kit(tid)
    solving = _has_looks(team)
    res = looks.resolve(palettes, team) if solving else None
    counts = dict(res.tone_counts) if res is not None else _nibbles(team)
    warning = None
    if res is not None:
        warning = next((w for w in res.warnings if "closest" in w.lower()), None)

    report = []
    for tone in (0, 1):
        entry = (entries[tone] if tone < len(entries) and
                 isinstance(entries[tone], dict) else {})
        skin_text = entry.get("skin")
        skin_rgb = model.parse_colour(skin_text)
        if skin_rgb is not None:
            ramp = skin_ramp(skin_rgb)
            skin_source = "authored"
        elif exact:
            ramp = [looks.to_rgb(word)
                    for word in palettes.current_skin(tid, tone)]
            skin_source = "cartridge"
        else:
            ramp = skin_ramp(looks.SKIN_CANON["light" if tone == 0 else "dark"])
            skin_source = "default"

        hair_text = entry.get("hair")
        hair_rgb = model.parse_colour(hair_text)
        authored_hair = hair_rgb is not None
        if hair_rgb is not None:
            hair_rgb = _stored(hair_rgb)
            hair_source = "authored"
        elif exact:
            hair_rgb = looks.to_rgb(palettes.current_hair(tid, tone))
            hair_source = "cartridge"
        else:
            hair_rgb = _stored(looks.HAIR_PAINT["brown"])
            hair_source = "default"

        report.append({
            "tone": tone,
            "skin": skin_text if skin_rgb is not None else None,
            "hair": hair_text if authored_hair else None,
            "skin_source": skin_source,
            "hair_source": hair_source,
            "ramp": ramp,
            "hair_rgb": hair_rgb,
            "count": counts.get(tone, 0),
            "warning": warning if tone == 0 else None,
        })
    return report


def has_assumed(report) -> bool:
    """Whether any colour in the report is an invented default."""
    return any(entry.get("skin_source") == "default" or
               entry.get("hair_source") == "default" for entry in report)


def tone_caption(entry: dict, part: str) -> str:
    """The text beside a tone's swatches: its hex and where the colour is from.

    Only a colour actually read from `Palettes` is labelled "(cartridge)"; an
    invented default says "(assumed)". Authored colours stand alone because
    the pack, not the editor or the cartridge, chose them.
    """
    if part == "skin":
        text, source, shown = (entry["skin"], entry["skin_source"],
                               entry["ramp"][-1])
    elif part == "hair":
        text, source, shown = (entry["hair"], entry["hair_source"],
                               entry["hair_rgb"])
    else:
        raise ValueError("part must be 'skin' or 'hair'")
    text = text or "#%02X%02X%02X" % shown
    if source == "cartridge":
        return "%s (cartridge)" % text
    if source == "default":
        return "%s (assumed)" % text
    return text


def pair_note(report, palettes=None) -> str:
    """The sentence under the pair picture.

    "No cartridge" is only true when there is none. With a cartridge loaded,
    an assumed colour means this particular team has no record to read.
    """
    if not has_assumed(report):
        return TWO_TONES_NOTE
    return NO_CARTRIDGE_NOTE if palettes is None else NO_RECORD_NOTE


def _tone_colours(team, tone, palettes=None, report=None) -> list:
    """A tone as the sprite's sixteen palette entries: 0 clear, 1 hair, 2-6
    skin, the rest the strip."""
    report = report or tone_report(team, palettes)
    out = [None] * 16
    for index, colour in enumerate(report[tone]["ramp"]):
        out[looks.KIT_SKIN + index] = colour
    out[looks.KIT_HAIR] = report[tone]["hair_rgb"]
    tid = _team_id(team)
    if palettes is not None and tid is not None and palettes.has_kit(tid):
        kit = palettes.teams[tid]["kit"] or []
        for index in range(7, min(16, len(kit))):
            out[index] = looks.to_rgb(kit[index])
    return out


def _swatch_row(ramp, hair, size) -> Image.Image:
    """The five skin shades and the hair colour, as a strip."""
    w, h = size
    im = Image.new("RGB", size, BACKDROP)
    d = ImageDraw.Draw(im)
    cell = w / 6.0
    for i, colour in enumerate(ramp[:5]):
        d.rectangle([i * cell, 0, (i + 1) * cell - 2, h - 1], fill=colour)
    d.rectangle([5 * cell, 0, w - 1, h - 1], fill=hair)
    return im


def _wrapped(d, text, xy, width, fill, max_lines=3):
    words, lines, line = text.split(), [], ""
    for word in words:
        candidate = (line + " " + word).strip()
        if line and d.textlength(candidate) > width:
            lines.append(line)
            line = word
        else:
            line = candidate
    if line:
        lines.append(line)
    x, y = xy
    for i, line in enumerate(lines[:max_lines]):
        d.text((x, y + i * 11), line, fill=fill)


def palette_pair(team: dict, palettes=None, size=(366, 178)) -> Image.Image:
    """Both tones' five-shade skin ramps and hair, with who wears which.

    The colours are the loader's own derivation, not the pack's hex; the
    counts and the closest-match line come from the solver. This is the team
    pane's "Skin tones" picture.
    """
    report = tone_report(team, palettes)
    w, h = size
    im = Image.new("RGB", size, BACKDROP)
    d = ImageDraw.Draw(im)
    gap = 8
    column = (w - gap * 3) // 2

    for tone, entry in enumerate(report):
        x0 = gap + tone * (column + gap)
        d.text((x0, 6), "Tone %d" % tone, fill=INK)
        d.text((x0, 19), "%d player%s" % (entry["count"],
                                          "" if entry["count"] == 1 else "s"),
               fill=MUTED_TEXT)
        im.paste(_swatch_row(entry["ramp"], entry["hair_rgb"], (column, 28)),
                 (x0, 34))
        d.text((x0, 68), "skin %s" % tone_caption(entry, "skin"), fill=TEXT)
        d.text((x0, 82), "hair %s" % tone_caption(entry, "hair"), fill=TEXT)

    warning = next((entry["warning"] for entry in report if entry["warning"]),
                   None)
    if warning:
        d.rectangle([0, h - 42, w - 1, h - 1], fill=(250, 226, 200))
        _wrapped(d, warning, (8, h - 36), w - 16, (160, 80, 0))
    else:
        _wrapped(d, pair_note(report, palettes), (8, h - 32), w - 16,
                 MUTED_TEXT, 2)
    d.rectangle([0, 0, w - 1, h - 1], outline=(200, 204, 212))
    return im


def look_card(player: dict, team: dict, palettes=None,
              size=(250, 124)) -> Image.Image:
    """One player's head with his tone's swatches - the per-player look.

    The head is the real graphic: the standard sprite head for hair_style 0,
    or the overlay the cartridge's table names for 1-13, drawn in the palette
    his `skin_tone` selects. Without a cartridge, or for a style past the
    table, the card says so rather than inventing a face.
    """
    player = player if isinstance(player, dict) else {}
    try:
        tone = 1 if int(player.get("skin_tone", 0) or 0) == 1 else 0
    except (TypeError, ValueError):
        tone = 0
    try:
        style = int(player.get("hair_style", 0) or 0)
    except (TypeError, ValueError):
        style = 0

    report = tone_report(team, palettes)
    colours = _tone_colours(team, tone, palettes, report)
    rom = getattr(palettes, "rom", None) if palettes is not None else None

    w, h = size
    im = Image.new("RGB", size, BACKDROP)
    d = ImageDraw.Draw(im)
    scale = max(1, (h - 16) // 8)
    side = 8 * scale
    face = Image.new("RGB", (side, side), BACKDROP)
    fd = ImageDraw.Draw(face)
    grid = head(rom, style)
    if grid is None:
        fd.rectangle([0, 0, side - 1, side - 1], outline=(190, 194, 202))
        fd.text((6, side // 2 - 6),
                "no cartridge" if rom is None else "no graphic",
                fill=(120, 124, 134))
    else:
        for y, row in enumerate(grid):
            for x, pixel in enumerate(row):
                colour = colours[pixel] if pixel < len(colours) else None
                if colour is not None:
                    fd.rectangle([x * scale, y * scale, (x + 1) * scale - 1,
                                  (y + 1) * scale - 1], fill=colour)
    im.paste(face, (6, 6))

    x = 6 + side + 10
    im.paste(_swatch_row(report[tone]["ramp"], report[tone]["hair_rgb"],
                         (max(20, w - x - 6), 20)), (x, 8))
    d.text((x, 36), "Tone %d" % tone, fill=INK)
    caption = str(player.get("name") or player.get("position") or "this player")
    d.text((x, 50), caption[:28], fill=TEXT)
    d.text((x, 64), "hair style %d" % style, fill=MUTED_TEXT)
    if grid is None:
        d.text((x, 78), "no graphic" if rom is not None else "no cartridge",
               fill=(150, 120, 90))
    d.rectangle([0, 0, w - 1, h - 1], outline=(200, 204, 212))
    return im
