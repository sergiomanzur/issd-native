#!/usr/bin/env python3
"""Generate crisp 32-bit BMP flag assets for the 32 FIFA World Cup 2026 teams.
Stored under mods/world_cup_2026/flags/.
"""
import os
import math
from PIL import Image, ImageDraw

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                          "mods", "world_cup_2026", "flags")
os.makedirs(OUTPUT_DIR, exist_ok=True)

W, H = 72, 48

def save_bmp(im, name):
    # Ensure 32-bit RGBA BMP
    rgba = im.convert("RGBA")
    path = os.path.join(OUTPUT_DIR, f"{name}.bmp")
    rgba.save(path, format="BMP")
    print(f"Saved {path}")

def gen_argentina():
    im = Image.new("RGBA", (W, H), (117, 170, 219, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(255, 255, 255, 255))
    # Sun of May in center
    cx, cy = W // 2, H // 2
    r = 5
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(252, 209, 22, 255), outline=(212, 133, 0, 255))
    for i in range(16):
        angle = i * math.pi / 8
        x1 = cx + (r + 1) * math.cos(angle)
        y1 = cy + (r + 1) * math.sin(angle)
        x2 = cx + (r + 3) * math.cos(angle)
        y2 = cy + (r + 3) * math.sin(angle)
        d.line([x1, y1, x2, y2], fill=(252, 209, 22, 255))
    save_bmp(im, "argentina")

def gen_france():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(0, 38, 84, 255))
    d.rectangle([2 * W // 3, 0, W, H], fill=(237, 41, 57, 255))
    save_bmp(im, "france")

def gen_spain():
    im = Image.new("RGBA", (W, H), (170, 21, 27, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 4, W, 3 * H // 4], fill=(241, 191, 0, 255))
    # Coat of arms simplified
    cx, cy = W // 3, H // 2
    d.rectangle([cx - 4, cy - 4, cx + 4, cy + 5], fill=(170, 21, 27, 255))
    d.rectangle([cx - 2, cy - 2, cx + 2, cy + 3], fill=(241, 191, 0, 255))
    save_bmp(im, "spain")

def gen_england():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    cw = 8
    d.rectangle([W // 2 - cw // 2, 0, W // 2 + cw // 2, H], fill=(206, 17, 36, 255))
    d.rectangle([0, H // 2 - cw // 2, W, H // 2 + cw // 2], fill=(206, 17, 36, 255))
    save_bmp(im, "england")

def gen_brazil():
    im = Image.new("RGBA", (W, H), (0, 151, 57, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(W // 2, 4), (W - 6, H // 2), (W // 2, H - 4), (6, H // 2)], fill=(254, 221, 0, 255))
    r = 11
    cx, cy = W // 2, H // 2
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(1, 33, 105, 255))
    d.arc([cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2], start=190, end=350, fill=(255, 255, 255, 255), width=2)
    save_bmp(im, "brazil")

def gen_belgium():
    im = Image.new("RGBA", (W, H), (253, 218, 36, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(0, 0, 0, 255))
    d.rectangle([2 * W // 3, 0, W, H], fill=(239, 51, 64, 255))
    save_bmp(im, "belgium")

def gen_netherlands():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 3], fill=(174, 28, 40, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(33, 70, 139, 255))
    save_bmp(im, "netherlands")

def gen_portugal():
    im = Image.new("RGBA", (W, H), (255, 0, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, int(W * 0.38), H], fill=(0, 102, 0, 255))
    # Armillary sphere and shield
    cx, cy = int(W * 0.38), H // 2
    r = 8
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(254, 221, 0, 255), width=2)
    d.rectangle([cx - 3, cy - 4, cx + 3, cy + 4], fill=(255, 255, 255, 255), outline=(0, 38, 84, 255))
    save_bmp(im, "portugal")

def gen_colombia():
    im = Image.new("RGBA", (W, H), (252, 209, 22, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 2, W, 3 * H // 4], fill=(0, 56, 147, 255))
    d.rectangle([0, 3 * H // 4, W, H], fill=(206, 17, 38, 255))
    save_bmp(im, "colombia")

def gen_italy():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(0, 146, 70, 255))
    d.rectangle([2 * W // 3, 0, W, H], fill=(206, 43, 55, 255))
    save_bmp(im, "italy")

def gen_uruguay():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    stripe_h = H / 9.0
    for i in range(1, 9, 2):
        d.rectangle([0, int(i * stripe_h), W, int((i + 1) * stripe_h)], fill=(0, 56, 168, 255))
    # White canton
    cw = int(W * 0.4)
    ch = int(5 * stripe_h)
    d.rectangle([0, 0, cw, ch], fill=(255, 255, 255, 255))
    d.ellipse([cw // 2 - 5, ch // 2 - 5, cw // 2 + 5, ch // 2 + 5], fill=(252, 209, 22, 255))
    save_bmp(im, "uruguay")

def gen_croatia():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 3], fill=(255, 0, 0, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(0, 0, 255, 255))
    # Chequy shield in center
    cx, cy = W // 2, H // 2
    sw, sh = 10, 12
    d.rectangle([cx - sw, cy - sh // 2, cx + sw, cy + sh // 2 + 2], fill=(255, 255, 255, 255), outline=(0, 56, 168, 255))
    for r in range(3):
        for c in range(3):
            if (r + c) % 2 == 0:
                d.rectangle([cx - sw + c * 6 + 1, cy - sh // 2 + r * 4 + 1,
                             cx - sw + (c + 1) * 6, cy - sh // 2 + (r + 1) * 4], fill=(255, 0, 0, 255))
    save_bmp(im, "croatia")

def gen_germany():
    im = Image.new("RGBA", (W, H), (221, 0, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 3], fill=(0, 0, 0, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(255, 206, 0, 255))
    save_bmp(im, "germany")

def gen_morocco():
    im = Image.new("RGBA", (W, H), (193, 39, 45, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    r = 10
    pts = []
    for i in range(5):
        a = i * 4 * math.pi / 5 - math.pi / 2
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    for i in range(5):
        d.line([pts[i], pts[(i + 1) % 5]], fill=(0, 98, 51, 255), width=2)
    save_bmp(im, "morocco")

def gen_switzerland():
    im = Image.new("RGBA", (W, H), (218, 41, 28, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    arm_w, arm_l = 6, 12
    d.rectangle([cx - arm_w, cy - arm_l, cx + arm_w, cy + arm_l], fill=(255, 255, 255, 255))
    d.rectangle([cx - arm_l, cy - arm_w, cx + arm_l, cy + arm_w], fill=(255, 255, 255, 255))
    save_bmp(im, "switzerland")

def gen_usa():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    sh = H / 13.0
    for i in range(0, 13, 2):
        d.rectangle([0, int(i * sh), W, int((i + 1) * sh)], fill=(178, 34, 52, 255))
    # Canton
    cw, ch = int(W * 0.45), int(7 * sh)
    d.rectangle([0, 0, cw, ch], fill=(60, 59, 110, 255))
    for row in range(3):
        for col in range(4):
            x = 4 + col * 7
            y = 3 + row * 6
            d.rectangle([x, y, x + 2, y + 2], fill=(255, 255, 255, 255))
    save_bmp(im, "usa")

def gen_mexico():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(0, 104, 71, 255))
    d.rectangle([2 * W // 3, 0, W, H], fill=(206, 17, 38, 255))
    # Center eagle emblem
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=(150, 100, 40, 255))
    d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=(0, 104, 71, 255))
    save_bmp(im, "mexico")

def gen_japan():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    r = 13
    cx, cy = W // 2, H // 2
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(188, 0, 45, 255))
    save_bmp(im, "japan")

def gen_denmark():
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    cw = 6
    x0 = int(W * 0.35)
    d.rectangle([x0 - cw // 2, 0, x0 + cw // 2, H], fill=(255, 255, 255, 255))
    d.rectangle([0, H // 2 - cw // 2, W, H // 2 + cw // 2], fill=(255, 255, 255, 255))
    save_bmp(im, "denmark")

def gen_south_korea():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    r = 10
    # Taegeuk: red top, blue bottom
    d.pieslice([cx - r, cy - r, cx + r, cy + r], start=180, end=360, fill=(205, 46, 58, 255))
    d.pieslice([cx - r, cy - r, cx + r, cy + r], start=0, end=180, fill=(0, 71, 160, 255))
    # Small trigram dots in corners
    for tx, ty in [(10, 8), (W - 14, 8), (10, H - 12), (W - 14, H - 12)]:
        d.rectangle([tx, ty, tx + 6, ty + 2], fill=(0, 0, 0, 255))
        d.rectangle([tx, ty + 3, tx + 6, ty + 5], fill=(0, 0, 0, 255))
    save_bmp(im, "south_korea")

def gen_austria():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 3], fill=(237, 41, 57, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(237, 41, 57, 255))
    save_bmp(im, "austria")

def gen_turkey():
    im = Image.new("RGBA", (W, H), (227, 10, 23, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2 - 4, H // 2
    r = 11
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255, 255))
    d.ellipse([cx - r + 3, cy - r + 1, cx + r + 1, cy + r - 1], fill=(227, 10, 23, 255))
    # Star
    sx, sy = cx + r + 2, cy
    d.polygon([(sx, sy - 4), (sx + 2, sy - 1), (sx + 5, sy), (sx + 2, sy + 2),
               (sx + 3, sy + 5), (sx, sy + 3), (sx - 3, sy + 5), (sx - 2, sy + 2),
               (sx - 5, sy), (sx - 2, sy - 1)], fill=(255, 255, 255, 255))
    save_bmp(im, "turkey")

def gen_nigeria():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(0, 135, 81, 255))
    d.rectangle([2 * W // 3, 0, W, H], fill=(0, 135, 81, 255))
    save_bmp(im, "nigeria")

def gen_sweden():
    im = Image.new("RGBA", (W, H), (0, 82, 147, 255))
    d = ImageDraw.Draw(im)
    cw = 7
    x0 = int(W * 0.35)
    d.rectangle([x0 - cw // 2, 0, x0 + cw // 2, H], fill=(254, 204, 0, 255))
    d.rectangle([0, H // 2 - cw // 2, W, H // 2 + cw // 2], fill=(254, 204, 0, 255))
    save_bmp(im, "sweden")

def gen_norway():
    im = Image.new("RGBA", (W, H), (186, 12, 47, 255))
    d = ImageDraw.Draw(im)
    x0 = int(W * 0.35)
    # White base cross
    d.rectangle([x0 - 5, 0, x0 + 5, H], fill=(255, 255, 255, 255))
    d.rectangle([0, H // 2 - 5, W, H // 2 + 5], fill=(255, 255, 255, 255))
    # Inner blue cross
    d.rectangle([x0 - 2, 0, x0 + 2, H], fill=(0, 32, 91, 255))
    d.rectangle([0, H // 2 - 2, W, H // 2 + 2], fill=(0, 32, 91, 255))
    save_bmp(im, "norway")

def gen_poland():
    im = Image.new("RGBA", (W, H), (220, 20, 60, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 2], fill=(255, 255, 255, 255))
    save_bmp(im, "poland")

# --- 6 NEW NATIONS ---

def gen_canada():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    # 1:2:1 red-white-red
    d.rectangle([0, 0, W // 4, H], fill=(200, 16, 46, 255))
    d.rectangle([3 * W // 4, 0, W, H], fill=(200, 16, 46, 255))
    # Maple leaf stylized
    cx, cy = W // 2, H // 2
    d.polygon([(cx, cy - 11), (cx + 3, cy - 5), (cx + 8, cy - 7), (cx + 6, cy - 1),
               (cx + 10, cy + 2), (cx + 5, cy + 3), (cx + 2, cy + 8), (cx + 1, cy + 11),
               (cx - 1, cy + 11), (cx - 2, cy + 8), (cx - 5, cy + 3), (cx - 10, cy + 2),
               (cx - 6, cy - 1), (cx - 8, cy - 7), (cx - 3, cy - 5)], fill=(200, 16, 46, 255))
    save_bmp(im, "canada")

def gen_senegal():
    im = Image.new("RGBA", (W, H), (253, 239, 66, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(0, 133, 63, 255))
    d.rectangle([2 * W // 3, 0, W, H], fill=(227, 27, 35, 255))
    # Green 5-pointed star in center
    cx, cy = W // 2, H // 2
    r = 7
    pts = []
    for i in range(5):
        a = i * 4 * math.pi / 5 - math.pi / 2
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(pts, fill=(0, 133, 63, 255))
    save_bmp(im, "senegal")

def gen_iran():
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 3], fill=(35, 159, 64, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(218, 0, 0, 255))
    # Red emblem in center
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], outline=(218, 0, 0, 255), width=2)
    d.line([cx, cy - 5, cx, cy + 5], fill=(218, 0, 0, 255), width=2)
    save_bmp(im, "iran")

def gen_australia():
    im = Image.new("RGBA", (W, H), (0, 0, 139, 255))
    d = ImageDraw.Draw(im)
    # Union Jack canton
    cw, ch = int(W * 0.45), int(H * 0.5)
    d.rectangle([0, 0, cw, ch], fill=(1, 33, 105, 255))
    d.line([0, 0, cw, ch], fill=(255, 255, 255, 255), width=3)
    d.line([0, ch, cw, 0], fill=(255, 255, 255, 255), width=3)
    d.line([0, 0, cw, ch], fill=(200, 16, 46, 255), width=1)
    d.line([0, ch, cw, 0], fill=(200, 16, 46, 255), width=1)
    d.rectangle([cw // 2 - 3, 0, cw // 2 + 3, ch], fill=(255, 255, 255, 255))
    d.rectangle([0, ch // 2 - 3, cw, ch // 2 + 3], fill=(255, 255, 255, 255))
    d.rectangle([cw // 2 - 1, 0, cw // 2 + 1, ch], fill=(200, 16, 46, 255))
    d.rectangle([0, ch // 2 - 1, cw, ch // 2 + 1], fill=(200, 16, 46, 255))
    # Commonwealth star
    d.ellipse([cw // 2 - 4, int(H * 0.75) - 4, cw // 2 + 4, int(H * 0.75) + 4], fill=(255, 255, 255, 255))
    # Southern cross dots
    for x, y in [(W - 14, 12), (W - 8, 20), (W - 20, 22), (W - 15, 30), (W - 12, 38)]:
        d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=(255, 255, 255, 255))
    save_bmp(im, "australia")

def gen_ecuador():
    im = Image.new("RGBA", (W, H), (255, 221, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 2, W, 3 * H // 4], fill=(3, 47, 105, 255))
    d.rectangle([0, 3 * H // 4, W, H], fill=(218, 41, 28, 255))
    # Coat of arms oval in center
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 5, cy - 6, cx + 5, cy + 6], fill=(0, 150, 220, 255), outline=(150, 100, 40, 255))
    save_bmp(im, "ecuador")

def gen_ukraine():
    im = Image.new("RGBA", (W, H), (255, 215, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, H // 2], fill=(0, 91, 187, 255))
    save_bmp(im, "ukraine")

def main():
    gen_argentina()
    gen_france()
    gen_spain()
    gen_england()
    gen_brazil()
    gen_belgium()
    gen_netherlands()
    gen_portugal()
    gen_colombia()
    gen_italy()
    gen_uruguay()
    gen_croatia()
    gen_germany()
    gen_morocco()
    gen_switzerland()
    gen_usa()
    gen_mexico()
    gen_japan()
    gen_denmark()
    gen_south_korea()
    gen_austria()
    gen_turkey()
    gen_nigeria()
    gen_sweden()
    gen_norway()
    gen_poland()
    gen_canada()
    gen_senegal()
    gen_iran()
    gen_australia()
    gen_ecuador()
    gen_ukraine()
    print("All 32 World Cup 2026 flags generated successfully!")

if __name__ == "__main__":
    main()
