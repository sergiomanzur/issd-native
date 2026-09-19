#!/usr/bin/env python3
"""Generate authentic 96x72 32-bit BMP team squad photos for all 36 Liga MX & Expansion clubs.
Stored under mods/liga_mx/photos/.
"""
import os
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_PHOTO = os.path.join(REPO, 'mods', 'chivas_guadalajara', 'team_photo.bmp')
OUT_DIR = os.path.join(REPO, 'mods', 'liga_mx', 'photos')
os.makedirs(OUT_DIR, exist_ok=True)

src = Image.open(BASE_PHOTO).convert('RGBA')

teams = {
    # Liga MX
    'america': {
        'shirt1': (255, 230, 0, 255), 'shirt2': (13, 71, 161, 255),
        'shorts': (13, 71, 161, 255), 'socks': (255, 230, 0, 255),
        'gk': (40, 40, 45, 255), 'gk_shorts': (30, 30, 35, 255)
    },
    'chivas': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (18, 58, 107, 255), 'socks': (245, 245, 245, 255),
        'gk': (34, 130, 70, 255), 'gk_shorts': (20, 80, 40, 255)
    },
    'cruz_azul': {
        'shirt1': (0, 61, 165, 255), 'shirt2': (0, 45, 130, 255),
        'shorts': (245, 245, 245, 255), 'socks': (0, 61, 165, 255),
        'gk': (200, 16, 46, 255), 'gk_shorts': (160, 12, 36, 255)
    },
    'pumas': {
        'shirt1': (0, 43, 73, 255), 'shirt2': (196, 154, 69, 255),
        'shorts': (196, 154, 69, 255), 'socks': (0, 43, 73, 255),
        'gk': (210, 50, 40, 255), 'gk_shorts': (170, 40, 30, 255)
    },
    'toluca': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (175, 12, 38, 255),
        'shorts': (245, 245, 245, 255), 'socks': (200, 16, 46, 255),
        'gk': (25, 25, 30, 255), 'gk_shorts': (20, 20, 25, 255)
    },
    'atlas': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (26, 26, 26, 255),
        'shorts': (26, 26, 26, 255), 'socks': (26, 26, 26, 255),
        'gk': (0, 153, 204, 255), 'gk_shorts': (0, 120, 160, 255)
    },
    'tigres': {
        'shirt1': (255, 204, 0, 255), 'shirt2': (0, 61, 165, 255),
        'shorts': (0, 61, 165, 255), 'socks': (255, 204, 0, 255),
        'gk': (0, 160, 80, 255), 'gk_shorts': (0, 120, 60, 255)
    },
    'monterrey': {
        'shirt1': (11, 34, 101, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (11, 34, 101, 255), 'socks': (245, 245, 245, 255),
        'gk': (220, 80, 30, 255), 'gk_shorts': (180, 60, 20, 255)
    },
    'santos': {
        'shirt1': (0, 104, 71, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (0, 104, 71, 255), 'socks': (245, 245, 245, 255),
        'gk': (255, 204, 0, 255), 'gk_shorts': (210, 170, 0, 255)
    },
    'leon': {
        'shirt1': (0, 122, 61, 255), 'shirt2': (0, 95, 48, 255),
        'shorts': (245, 245, 245, 255), 'socks': (0, 122, 61, 255),
        'gk': (25, 25, 30, 255), 'gk_shorts': (20, 20, 25, 255)
    },
    'pachuca': {
        'shirt1': (0, 51, 102, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (0, 51, 102, 255), 'socks': (245, 245, 245, 255),
        'gk': (255, 120, 0, 255), 'gk_shorts': (210, 95, 0, 255)
    },
    'tijuana': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (26, 26, 26, 255),
        'shorts': (26, 26, 26, 255), 'socks': (200, 16, 46, 255),
        'gk': (50, 180, 220, 255), 'gk_shorts': (40, 140, 180, 255)
    },
    'puebla': {
        'shirt1': (245, 245, 245, 255), 'shirt2': (0, 61, 165, 255),
        'shorts': (0, 61, 165, 255), 'socks': (245, 245, 245, 255),
        'gk': (230, 50, 50, 255), 'gk_shorts': (190, 40, 40, 255)
    },
    'necaxa': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (18, 58, 107, 255), 'socks': (200, 16, 46, 255),
        'gk': (30, 140, 60, 255), 'gk_shorts': (20, 100, 45, 255)
    },
    'san_luis': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (18, 58, 107, 255), 'socks': (200, 16, 46, 255),
        'gk': (25, 25, 30, 255), 'gk_shorts': (20, 20, 25, 255)
    },
    'mazatlan': {
        'shirt1': (75, 0, 130, 255), 'shirt2': (55, 0, 100, 255),
        'shorts': (26, 26, 26, 255), 'socks': (75, 0, 130, 255),
        'gk': (0, 206, 209, 255), 'gk_shorts': (0, 160, 165, 255)
    },
    'juarez': {
        'shirt1': (0, 153, 51, 255), 'shirt2': (200, 16, 46, 255),
        'shorts': (26, 26, 26, 255), 'socks': (0, 153, 51, 255),
        'gk': (255, 204, 0, 255), 'gk_shorts': (210, 170, 0, 255)
    },
    'queretaro': {
        'shirt1': (0, 51, 102, 255), 'shirt2': (26, 26, 26, 255),
        'shorts': (26, 26, 26, 255), 'socks': (0, 51, 102, 255),
        'gk': (255, 80, 80, 255), 'gk_shorts': (210, 60, 60, 255)
    },

    # Liga de Expansion & Historicos
    'atlante': {
        'shirt1': (0, 34, 68, 255), 'shirt2': (128, 0, 32, 255),
        'shorts': (0, 34, 68, 255), 'socks': (128, 0, 32, 255),
        'gk': (0, 160, 90, 255), 'gk_shorts': (0, 120, 65, 255)
    },
    'celaya': {
        'shirt1': (245, 245, 245, 255), 'shirt2': (0, 61, 165, 255),
        'shorts': (0, 61, 165, 255), 'socks': (245, 245, 245, 255),
        'gk': (255, 140, 0, 255), 'gk_shorts': (210, 110, 0, 255)
    },
    'morelia': {
        'shirt1': (255, 209, 0, 255), 'shirt2': (200, 16, 46, 255),
        'shorts': (0, 43, 73, 255), 'socks': (255, 209, 0, 255),
        'gk': (25, 25, 30, 255), 'gk_shorts': (20, 20, 25, 255)
    },
    'leones_negros': {
        'shirt1': (255, 204, 0, 255), 'shirt2': (200, 16, 46, 255),
        'shorts': (26, 26, 26, 255), 'socks': (255, 204, 0, 255),
        'gk': (0, 120, 220, 255), 'gk_shorts': (0, 90, 180, 255)
    },
    'mineros': {
        'shirt1': (140, 16, 38, 255), 'shirt2': (115, 12, 30, 255),
        'shorts': (245, 245, 245, 255), 'socks': (140, 16, 38, 255),
        'gk': (255, 200, 0, 255), 'gk_shorts': (210, 160, 0, 255)
    },
    'venados': {
        'shirt1': (255, 209, 0, 255), 'shirt2': (225, 185, 0, 255),
        'shorts': (26, 26, 26, 255), 'socks': (255, 209, 0, 255),
        'gk': (0, 130, 60, 255), 'gk_shorts': (0, 95, 45, 255)
    },
    'cancun': {
        'shirt1': (0, 163, 224, 255), 'shirt2': (0, 135, 190, 255),
        'shorts': (26, 26, 26, 255), 'socks': (0, 163, 224, 255),
        'gk': (255, 100, 180, 255), 'gk_shorts': (210, 80, 150, 255)
    },
    'correcaminos': {
        'shirt1': (255, 102, 0, 255), 'shirt2': (220, 85, 0, 255),
        'shorts': (245, 245, 245, 255), 'socks': (255, 102, 0, 255),
        'gk': (0, 51, 153, 255), 'gk_shorts': (0, 40, 120, 255)
    },
    'dorados': {
        'shirt1': (212, 175, 55, 255), 'shirt2': (185, 150, 45, 255),
        'shorts': (26, 26, 26, 255), 'socks': (212, 175, 55, 255),
        'gk': (200, 16, 46, 255), 'gk_shorts': (160, 12, 36, 255)
    },
    'alebrijes': {
        'shirt1': (255, 102, 0, 255), 'shirt2': (0, 153, 51, 255),
        'shorts': (26, 26, 26, 255), 'socks': (255, 102, 0, 255),
        'gk': (50, 180, 240, 255), 'gk_shorts': (40, 140, 200, 255)
    },
    'tlaxcala': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (175, 12, 38, 255),
        'shorts': (26, 26, 26, 255), 'socks': (200, 16, 46, 255),
        'gk': (25, 25, 30, 255), 'gk_shorts': (20, 20, 25, 255)
    },
    'la_paz': {
        'shirt1': (0, 31, 63, 255), 'shirt2': (212, 175, 55, 255),
        'shorts': (245, 245, 245, 255), 'socks': (0, 31, 63, 255),
        'gk': (255, 60, 60, 255), 'gk_shorts': (210, 45, 45, 255)
    },
    'jaiba_brava': {
        'shirt1': (0, 153, 255, 255), 'shirt2': (0, 125, 215, 255),
        'shorts': (245, 245, 245, 255), 'socks': (0, 153, 255, 255),
        'gk': (200, 16, 46, 255), 'gk_shorts': (160, 12, 36, 255)
    },
    'tapatio': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (18, 58, 107, 255), 'socks': (245, 245, 245, 255),
        'gk': (255, 204, 0, 255), 'gk_shorts': (210, 170, 0, 255)
    },
    'tepatitlan': {
        'shirt1': (0, 51, 153, 255), 'shirt2': (200, 16, 46, 255),
        'shorts': (0, 51, 153, 255), 'socks': (245, 245, 245, 255),
        'gk': (25, 25, 30, 255), 'gk_shorts': (20, 20, 25, 255)
    },
    'cimarrones': {
        'shirt1': (140, 16, 38, 255), 'shirt2': (0, 31, 63, 255),
        'shorts': (245, 245, 245, 255), 'socks': (140, 16, 38, 255),
        'gk': (255, 140, 0, 255), 'gk_shorts': (210, 110, 0, 255)
    },
    'veracruz': {
        'shirt1': (200, 16, 46, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (0, 34, 68, 255), 'socks': (200, 16, 46, 255),
        'gk': (255, 220, 0, 255), 'gk_shorts': (210, 180, 0, 255)
    },
    'toros_neza': {
        'shirt1': (204, 0, 0, 255), 'shirt2': (245, 245, 245, 255),
        'shorts': (26, 26, 26, 255), 'socks': (204, 0, 0, 255),
        'gk': (255, 215, 0, 255), 'gk_shorts': (26, 26, 26, 255)
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
