"""The pictures.

Three things are worth drawing rather than describing, because in each case
the number in the file is not what the player will see:

  - a strip, because the cartridge stores five bits per channel and shades the
    other two tones itself, so #C8102E is not the red that reaches the pitch;
  - a formation, because ten pairs of signed numbers are not a shape;
  - a pitch, because 138 x 90 and 114 x 74 are the same two numbers until you
    put them side by side.
"""
from __future__ import annotations

from PIL import Image, ImageDraw

from . import model, repo

GRASS_DARK = (28, 104, 40)
GRASS_LIGHT = (38, 124, 48)
LINE = (236, 240, 236)
INK = (24, 26, 32)


def _pitch(size, margin=8, stripes=10):
    w, h = size
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


def formation(name: str, tactics: str = "balanced", size=(210, 260)) -> Image.Image:
    """Ten outfield players where the shape puts them, keeper at the back.

    Attacking and defensive shift every line by six the way
    issd_formation_apply_tactics does, so the preview moves when the pack does.
    """
    im, d = _pitch(size)
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


def player_head(skin_tone: int, hair_style: int, size=(88, 88)) -> Image.Image:
    """Skin tone and hair style are the two appearance fields a pack can set.
    The shapes here are the editor's own - the cartridge's sprites are not
    readable from a pack - so this is a legend, not a portrait."""
    w, h = size
    im = Image.new("RGB", size, (238, 240, 244))
    d = ImageDraw.Draw(im)
    skins = [(228, 188, 154), (196, 142, 104), (140, 92, 62)]
    skin = skins[max(0, min(len(skins) - 1, int(skin_tone or 0)))]
    hair = (38, 28, 22)

    cx, cy = w // 2, h // 2 + 6
    d.ellipse([cx - 22, cy - 26, cx + 22, cy + 22], fill=skin)
    style = int(hair_style or 0) % 16
    if style == 0:
        d.chord([cx - 23, cy - 32, cx + 23, cy + 6], 180, 360, fill=hair)
    elif style in (1, 9):
        d.chord([cx - 23, cy - 34, cx + 23, cy + 2], 180, 360, fill=hair)
        d.rectangle([cx - 23, cy - 16, cx - 17, cy + 6], fill=hair)
        d.rectangle([cx + 17, cy - 16, cx + 23, cy + 6], fill=hair)
    elif style in (2, 10):
        d.chord([cx - 23, cy - 30, cx + 23, cy - 2], 180, 360, fill=hair)
    elif style in (3, 11):
        for i in range(-3, 4):
            d.polygon([(cx + i * 6, cy - 24), (cx + i * 6 + 4, cy - 36),
                       (cx + i * 6 + 7, cy - 22)], fill=hair)
        d.chord([cx - 23, cy - 30, cx + 23, cy - 4], 180, 360, fill=hair)
    elif style in (4, 12):
        d.ellipse([cx - 25, cy - 34, cx + 25, cy + 4], fill=hair)
        d.ellipse([cx - 18, cy - 22, cx + 18, cy + 20], fill=skin)
    elif style in (5, 13):
        d.chord([cx - 23, cy - 30, cx + 23, cy - 6], 180, 360, fill=hair)
        d.rectangle([cx - 6, cy - 34, cx + 6, cy - 22], fill=hair)
    elif style in (6, 14):
        d.chord([cx - 23, cy - 28, cx + 23, cy - 8], 180, 360, fill=hair)
        d.rectangle([cx - 23, cy - 18, cx + 23, cy - 14], fill=hair)
    else:
        pass                                    # 7, 15: no hair at all
    d.ellipse([cx - 10, cy - 8, cx - 5, cy - 3], fill=INK)
    d.ellipse([cx + 5, cy - 8, cx + 10, cy - 3], fill=INK)
    d.arc([cx - 8, cy + 2, cx + 8, cy + 12], 20, 160, fill=INK)
    d.rectangle([0, 0, w - 1, h - 1], outline=(180, 184, 192))
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
