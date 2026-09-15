"""The pictures.

Things worth drawing rather than describing, because in each case the number
in the file is not what the player will see:

  - a strip, because the cartridge stores five bits per channel and shades the
    other two tones itself, so #C8102E is not the red that reaches the pitch;
  - a formation, because ten pairs of signed numbers are not a shape;
  - a pitch, because 138 x 90 and 114 x 74 are the same two numbers until you
    put them side by side;
  - a player's appearance byte, because it does not do what its name says.

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


# What the appearance byte really is. The high nibble picks the palette the
# sprite is drawn with; measured by setting it and looking. 0 and 1 are the
# only ones the cartridge uses and the only ones that leave a footballer on
# the screen - 2 and 3 select palettes the strip does not fit and turn the
# whole player orange or green. The low nibble changes nothing in a match,
# though the cartridge's own squads vary it from 0 to 13.
APPEARANCE = {
    0: ("Dark hair", "what most of the cartridge's players have"),
    1: ("Fair hair", "the other one it uses - 101 of its 720 players"),
    2: ("Breaks the sprite", "the whole player turns orange"),
    3: ("Breaks the sprite", "the whole player turns green"),
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
