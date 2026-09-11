#!/usr/bin/env python3
"""Build a high-resolution tile pack from a dump.

    ISSDNative --dump-tiles tiles --dump-tiles-from 3650 ...
    python tools/upscale_tiles.py tiles mods/press_start_hd --scale 4

Every 8x8 tile the game drew is enlarged and written back under the same
name, which is how the running game finds it again. A pack is just a folder
of images: replace any of them by hand and the game picks that up instead.

Two details matter for quality, and both are about edges.

Tiles are enlarged one at a time with no knowledge of their neighbours, so a
filter that reaches past the edge of an 8x8 image invents a border that its
neighbour does not share, and the seams show up as a grid. Padding each tile
by replicating its edge pixels before filtering keeps the border close to the
colour that was already there.

Colour index 0 is transparent on a background layer, and the dump stores it
as transparent with black underneath. Filtering that black would draw a dark
halo around everything, so transparent pixels are first filled from their
nearest opaque neighbour; the alpha channel is enlarged separately without
filtering so the shape itself stays crisp.
"""
import argparse
import glob
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:                                    # pragma: no cover
    sys.exit("Pillow is required: python -m pip install pillow")

BMP_FILE_HEADER = "<HIHHI"
BMP_INFO_HEADER = "<IiiHHIIiiII"


def read_bmp32(path):
    """Read the 32-bit BMPs the game writes. Returns (w, h, list of ARGB)."""
    with open(path, "rb") as f:
        data = f.read()
    magic, _size, _r1, _r2, offbits = struct.unpack_from(BMP_FILE_HEADER, data, 0)
    if magic != 0x4D42:
        raise ValueError("%s is not a BMP" % path)
    (_hsize, w, h, _planes, bits, _comp,
     _imgsize, _xppm, _yppm, _used, _imp) = struct.unpack_from(BMP_INFO_HEADER, data, 14)
    if bits != 32:
        raise ValueError("%s is %d-bit; 32-bit is required" % (path, bits))
    top_down = h < 0
    h = abs(h)
    px = list(struct.unpack_from("<%dI" % (w * h), data, offbits))
    if not top_down:
        rows = [px[y * w:(y + 1) * w] for y in range(h)]
        rows.reverse()
        px = [v for row in rows for v in row]
    return w, h, px


def write_bmp32(path, w, h, px):
    """Write a 32-bit top-down BMP, the format the loader expects."""
    body = struct.pack("<%dI" % (w * h), *px)
    info = struct.pack(BMP_INFO_HEADER, 40, w, -h, 1, 32, 0, len(body),
                       2835, 2835, 0, 0)
    head = struct.pack(BMP_FILE_HEADER, 0x4D42, 14 + len(info) + len(body),
                       0, 0, 14 + len(info))
    with open(path, "wb") as f:
        f.write(head)
        f.write(info)
        f.write(body)


def fill_transparent(rgb, alpha, w, h):
    """Push opaque colour outwards so filtering never pulls in the void."""
    known = [a != 0 for a in alpha]
    if all(known) or not any(known):
        return rgb
    out = list(rgb)
    for _ in range(max(w, h)):
        changed = False
        for y in range(h):
            for x in range(w):
                i = y * w + x
                if known[i]:
                    continue
                acc, n = [0, 0, 0], 0
                for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < h and 0 <= nx < w and known[ny * w + nx]:
                        c = out[ny * w + nx]
                        acc[0] += (c >> 16) & 0xFF
                        acc[1] += (c >> 8) & 0xFF
                        acc[2] += c & 0xFF
                        n += 1
                if n:
                    out[i] = ((acc[0] // n) << 16) | ((acc[1] // n) << 8) | (acc[2] // n)
                    known[i] = True
                    changed = True
        if not changed:
            break
    return out


def upscale(path, scale, resample):
    w, h, px = read_bmp32(path)
    alpha = [(v >> 24) & 0xFF for v in px]
    rgb = fill_transparent([v & 0xFFFFFF for v in px], alpha, w, h)

    colour = Image.new("RGB", (w, h))
    colour.putdata([((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF) for v in rgb])

    # Replicate the edge, enlarge, then crop the padding back off, so the
    # filter never invents a border this tile's neighbour does not have.
    pad = 2
    padded = Image.new("RGB", (w + pad * 2, h + pad * 2))
    padded.paste(colour, (pad, pad))
    for i in range(pad):
        padded.paste(colour.crop((0, 0, w, 1)), (pad, i))
        padded.paste(colour.crop((0, h - 1, w, h)), (pad, h + pad + i))
    left = padded.crop((pad, 0, pad + 1, padded.height))
    right = padded.crop((pad + w - 1, 0, pad + w, padded.height))
    for i in range(pad):
        padded.paste(left, (i, 0))
        padded.paste(right, (pad + w + i, 0))

    big = padded.resize(((w + pad * 2) * scale, (h + pad * 2) * scale), resample)
    big = big.crop((pad * scale, pad * scale, (pad + w) * scale, (pad + h) * scale))

    mask = Image.new("L", (w, h))
    mask.putdata(alpha)
    mask = mask.resize((w * scale, h * scale), Image.NEAREST)

    out = []
    bd, md = list(big.getdata()), list(mask.getdata())
    for (r, g, b), a in zip(bd, md):
        out.append((a << 24) | (r << 16) | (g << 8) | b)
    return w * scale, h * scale, out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump_dir", help="directory written by --dump-tiles")
    ap.add_argument("out_dir", help="pack directory to create")
    ap.add_argument("--scale", type=int, default=4,
                    help="how many times larger each tile becomes (default 4)")
    ap.add_argument("--filter", default="lanczos",
                    choices=["lanczos", "bicubic", "nearest"],
                    help="how pixels are interpolated (default lanczos)")
    ap.add_argument("--only-frames", default=None, metavar="A:B",
                    help="restrict to tiles tiles.csv first saw in this frame range")
    args = ap.parse_args()

    resample = {"lanczos": Image.LANCZOS,
                "bicubic": Image.BICUBIC,
                "nearest": Image.NEAREST}[args.filter]

    wanted = None
    if args.only_frames:
        lo, hi = (int(v) for v in args.only_frames.split(":"))
        manifest = os.path.join(args.dump_dir, "tiles.csv")
        wanted = set()
        with open(manifest) as f:
            next(f, None)
            for line in f:
                name, _bpp, frame = line.strip().split(",")
                if lo <= int(frame) <= hi:
                    wanted.add(name)

    os.makedirs(args.out_dir, exist_ok=True)
    made = 0
    for path in sorted(glob.glob(os.path.join(args.dump_dir, "*.bmp"))):
        name = os.path.splitext(os.path.basename(path))[0]
        if wanted is not None and name not in wanted:
            continue
        w, h, px = upscale(path, args.scale, resample)
        write_bmp32(os.path.join(args.out_dir, name + ".bmp"), w, h, px)
        made += 1
    print("%d tile(s) enlarged %dx with %s -> %s"
          % (made, args.scale, args.filter, args.out_dir))


if __name__ == "__main__":
    main()
