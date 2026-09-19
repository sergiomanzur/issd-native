import os
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_PHOTO = os.path.join(REPO, 'mods', 'chivas_guadalajara', 'team_photo.bmp')
OUT_DIR = os.path.join(REPO, 'mods', 'world_cup_2026', 'photos')
os.makedirs(OUT_DIR, exist_ok=True)

src = Image.open(BASE_PHOTO).convert('RGBA')

teams = {
    'canada': {
        'shirt1': (200, 16, 46, 255),
        'shirt2': (175, 12, 38, 255),
        'shorts': (200, 16, 46, 255),
        'socks': (240, 240, 240, 255),
        'gk': (80, 20, 120, 255),
        'gk_shorts': (50, 10, 80, 255)
    },
    'ecuador': {
        'shirt1': (255, 221, 0, 255),
        'shirt2': (235, 195, 0, 255),
        'shorts': (0, 42, 92, 255),
        'socks': (200, 16, 46, 255),
        'gk': (40, 40, 45, 255),
        'gk_shorts': (30, 30, 35, 255)
    },
    'australia': {
        'shirt1': (255, 205, 0, 255),
        'shirt2': (235, 185, 0, 255),
        'shorts': (0, 85, 58, 255),
        'socks': (240, 240, 240, 255),
        'gk': (180, 40, 100, 255),
        'gk_shorts': (140, 30, 80, 255)
    },
    'ukraine': {
        'shirt1': (255, 215, 0, 255),
        'shirt2': (235, 195, 0, 255),
        'shorts': (255, 215, 0, 255),
        'socks': (0, 87, 183, 255),
        'gk': (30, 140, 70, 255),
        'gk_shorts': (20, 100, 50, 255)
    },
    'iran': {
        'shirt1': (245, 245, 245, 255),
        'shirt2': (220, 220, 220, 255),
        'shorts': (200, 16, 46, 255),
        'socks': (245, 245, 245, 255),
        'gk': (40, 40, 45, 255),
        'gk_shorts': (30, 30, 35, 255)
    },
    'senegal': {
        'shirt1': (245, 245, 245, 255),
        'shirt2': (220, 220, 220, 255),
        'shorts': (0, 133, 63, 255),
        'socks': (245, 245, 245, 255),
        'gk': (255, 215, 0, 255),
        'gk_shorts': (210, 175, 0, 255)
    }
}

for name, cfg in teams.items():
    dst = src.copy()
    w, h = dst.size
    for y in range(h):
        for x in range(w):
            c = src.getpixel((x, y))
            # GK jersey
            if c == (34, 130, 70, 255) or (c[:3] == (34, 130, 70) and y < 45):
                dst.putpixel((x, y), cfg['gk'])
            # GK shorts
            elif 44 <= y <= 55 and 68 <= x <= 78 and c[1] > 100 and c[0] < 50 and c[2] < 50:
                dst.putpixel((x, y), cfg['gk_shorts'])
            # Shirt stripes
            elif y < 58 and c == (200, 16, 46, 255):
                dst.putpixel((x, y), cfg['shirt1'])
            elif y < 58 and c == (242, 242, 242, 255):
                dst.putpixel((x, y), cfg['shirt2'])
            # Outfield Shorts
            elif c == (18, 54, 102, 255):
                dst.putpixel((x, y), cfg['shorts'])
            # Socks
            elif y >= 58 and c == (242, 242, 242, 255):
                dst.putpixel((x, y), cfg['socks'])
    
    out_path = os.path.join(OUT_DIR, f'{name}.bmp')
    dst.save(out_path, format='BMP')
    print(f'Generated {out_path}')