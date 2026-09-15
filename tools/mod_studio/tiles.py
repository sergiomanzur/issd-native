"""Tile packs: read a dump, look at it, put your own art back.

A tile pack replaces the cartridge's 8x8 background graphics - the grass, the
markings, the crowd - with pictures of your own at whatever size you like. The
game finds a replacement by the tile's *identity*, a hash of its pixels and
palette, so a pack is a folder of images named after the tile they stand in
for and nothing else.

Which is fine for the game and hopeless for a person: a dump is four hundred
files called `6bc41b4898647d62.bmp` and no way to tell the penalty spot from a
bit of sky. So this reads a dump, keeps the pictures, and lets the editor show
them - sorted so the pitch comes first, because that is what anyone authoring
a pitch is looking for.

The BMPs are the 32-bit top-down kind `issd_hd.c` reads. A replacement has to
be square and a multiple of 8 on a side, up to 512; anything dropped in is
resized to fit that rather than refused.
"""
from __future__ import annotations

import glob
import os
import struct

from PIL import Image

TILE = 8
MAX_EDGE = 512

_FILE_HEADER = "<HIHHI"
_INFO_HEADER = "<IiiHHIIiiII"


def read_bmp(path: str) -> Image.Image | None:
    """Both orders, because editors save bottom-up and the game writes top-down."""
    try:
        with open(path, "rb") as f:
            data = f.read()
        if len(data) < 54 or data[:2] != b"BM":
            return None
        offset = struct.unpack_from("<I", data, 10)[0]
        width, height, planes, bits = struct.unpack_from("<iiHH", data, 18)
        if bits != 32:
            return Image.open(path).convert("RGB")
        flip = height > 0
        h = abs(height)
        px = data[offset:offset + width * h * 4]
        im = Image.frombytes("RGBA", (width, h), px, "raw", "BGRA")
        return im.transpose(Image.FLIP_TOP_BOTTOM) if flip else im
    except Exception:
        return None


def write_bmp(path: str, im: Image.Image) -> None:
    """32-bit, top-down - what the game reads."""
    im = im.convert("RGBA")
    w, h = im.size
    rows = im.tobytes("raw", "BGRA")
    fh = struct.pack(_FILE_HEADER, 0x4D42, 14 + 40 + len(rows), 0, 0, 14 + 40)
    ih = struct.pack(_INFO_HEADER, 40, w, -h, 1, 32, 0, len(rows),
                     2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(fh + ih + rows)


def _grassiness(im: Image.Image) -> float:
    """How much of a tile is pitch. Sorting by this puts the grass first."""
    small = im.convert("RGB").resize((8, 8))
    green = 0
    for r, g, b in small.getdata():
        if g - r > 24 and g - b > 24:
            green += 1
    return green / 64.0


class Dump:
    """A folder of tiles the game drew, as produced by --dump-tiles."""

    def __init__(self, path: str):
        self.path = path
        self.tiles: list[tuple[str, Image.Image, float]] = []
        for f in sorted(glob.glob(os.path.join(path, "*.bmp"))):
            name = os.path.splitext(os.path.basename(f))[0]
            im = read_bmp(f)
            if im is None or im.width != im.height:
                continue
            self.tiles.append((name, im, _grassiness(im)))
        # pitch first, then everything else, each alphabetical so the order is
        # stable between openings
        self.tiles.sort(key=lambda t: (-round(t[2], 2), t[0]))

    def __len__(self):
        return len(self.tiles)

    @property
    def grass(self):
        return [t for t in self.tiles if t[2] >= 0.5]


def legal_edge(edge: int) -> int:
    """The nearest size the game will accept for a replacement."""
    edge = max(TILE, min(MAX_EDGE, int(edge)))
    return max(TILE, (edge // TILE) * TILE)


def put(pack_dir: str, tile_id: str, source: Image.Image, scale: int = 4) -> str:
    """Write one replacement into a pack, sized so the game will load it."""
    os.makedirs(pack_dir, exist_ok=True)
    edge = legal_edge(TILE * max(1, int(scale)))
    im = source.convert("RGBA").resize((edge, edge), Image.NEAREST)
    out = os.path.join(pack_dir, "%s.bmp" % tile_id)
    write_bmp(out, im)
    return out


def enlarge(im: Image.Image, scale: int = 4) -> Image.Image:
    """A tile blown up, for a pack that just wants the cartridge's own art bigger."""
    edge = legal_edge(im.width * max(1, int(scale)))
    return im.convert("RGBA").resize((edge, edge), Image.LANCZOS)


def pack_contents(pack_dir: str) -> set[str]:
    """Which tiles a pack already replaces."""
    if not os.path.isdir(pack_dir):
        return set()
    return {os.path.splitext(os.path.basename(f))[0]
            for f in glob.glob(os.path.join(pack_dir, "*.bmp"))}
