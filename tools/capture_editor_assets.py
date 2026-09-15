"""Cut the editor's reference pictures out of screenshots of the real game.

The editor draws two things it used to invent: the pitch a stadium actually
has, and what a player's appearance byte actually does. Both come from the
game rather than from a guess, which is the point - the schematic pitch the
editor drew before was the right shape and the wrong proportions, and the
head it drew for "skin tone" was of something the cartridge does not have.

    python tools/capture_editor_assets.py <shots-dir>

expects, in that directory:

    pd0.bmp .. pd7.bmp    the stadium select screen, one per stock slot
    look_s0h0.bmp         a match with every player's appearance byte 0x00
    look_s1hF.bmp          ... 0x1F
    look_s2h0.bmp          ... 0x20
    look_s3h0.bmp          ... 0x30

and writes tools/mod_studio/assets/. How the shots are taken is in
docs/REVERSE_ENGINEERING.md; they are deterministic, so the same script and
frame gives the same pixels every time and only the appearance byte differs.
"""
import io
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "mod_studio", "assets")

# The player is at the same place in every appearance shot: the shots differ
# only by one byte, and that byte does not move anybody.
PLAYER_BOX = (218, 96, 244, 142)
APPEARANCE = [("00", "look_s0h0.bmp"), ("10", "look_s1hF.bmp"),
              ("20", "look_s2h0.bmp"), ("30", "look_s3h0.bmp")]


def pitch_diagram(path):
    """The plan-view pitch off the stadium screen, cropped to the grass.

    The diagram is the same 62x46 for every ground - the yardages are
    printed beside it rather than drawn to scale - but the mowing is not:
    each stadium has its own pattern of stripes or checks, which is what
    makes it worth showing behind a formation instead of a drawn rectangle.

    Two things make this less trivial than "find the green". The screen has
    other green on it - the panel's border and the little photographs of the
    grounds along the bottom - so blobs are measured and the biggest wins. And
    the halfway line cuts the grass in two, so the halves have to be put back
    together: any blob level with the biggest one belongs to the same pitch.
    """
    im = Image.open(path).convert("RGB")
    px = im.load()
    w, h = im.size

    def grassy(x, y):
        r, g, b = px[x, y]
        return g - r > 40 and g - b > 40

    seen = [[False] * w for _ in range(h)]
    blobs = []
    for y0 in range(h):
        for x0 in range(w):
            if seen[y0][x0] or not grassy(x0, y0):
                continue
            stack = [(x0, y0)]
            seen[y0][x0] = True
            xs, ys, n = [], [], 0
            while stack:
                cx, cy = stack.pop()
                n += 1
                xs.append(cx)
                ys.append(cy)
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        if not seen[ny][nx] and grassy(nx, ny):
                            seen[ny][nx] = True
                            stack.append((nx, ny))
            if n < 200:
                continue
            bx0, by0 = min(xs), min(ys)
            bx1, by1 = max(xs), max(ys)
            area = (bx1 - bx0 + 1) * (by1 - by0 + 1)
            # A pitch is a solid block. The panel's border is green too
            # and has a huge bounding box around almost no pixels, so
            # how full the box is separates them cleanly.
            if n * 2 < area:
                continue
            blobs.append((n, bx0, by0, bx1, by1))
    if not blobs:
        return None

    blobs.sort(reverse=True)
    _, bx0, by0, bx1, by1 = blobs[0]
    x0, y0, x1, y1 = bx0, by0, bx1, by1
    for _, cx0, cy0, cx1, cy1 in blobs[1:]:
        level = min(by1, cy1) - max(by0, cy0) > (by1 - by0) * 0.6
        near = cx0 - bx1 < 8 and bx0 - cx1 < 8
        if level and near:
            x0, y0 = min(x0, cx0), min(y0, cy0)
            x1, y1 = max(x1, cx1), max(y1, cy1)
    return im.crop((x0, y0, x1 + 1, y1 + 1))


def main(shots):
    os.makedirs(os.path.join(ASSETS, "pitch"), exist_ok=True)
    os.makedirs(os.path.join(ASSETS, "player"), exist_ok=True)

    made = 0
    for slot in range(8):
        src = os.path.join(shots, "pd%d.bmp" % slot)
        if not os.path.isfile(src):
            continue
        crop = pitch_diagram(src)
        if crop is None:
            print("  slot %d: no pitch found in %s" % (slot, src))
            continue
        out = os.path.join(ASSETS, "pitch", "slot%d.png" % slot)
        crop.save(out)
        print("  pitch slot %d -> %s  (%dx%d)" % (slot, os.path.basename(out),
                                                  crop.width, crop.height))
        made += 1

    for name, src_name in APPEARANCE:
        src = os.path.join(shots, src_name)
        if not os.path.isfile(src):
            continue
        crop = Image.open(src).convert("RGB").crop(PLAYER_BOX)
        out = os.path.join(ASSETS, "player", "%s.png" % name)
        crop.save(out)
        print("  appearance $%s -> %s" % (name, os.path.basename(out)))
        made += 1

    print("%d asset(s) written to %s" % (made, ASSETS))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: capture_editor_assets.py <shots-dir>")
    main(sys.argv[1])
