#!/usr/bin/env python3
"""Generate crisp 32-bit BMP flag/crest assets for the 36 Liga MX and Expansion MX teams.
Stored under mods/liga_mx/flags/.
"""
import os
import math
from PIL import Image, ImageDraw

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                          "mods", "liga_mx", "flags")
os.makedirs(OUTPUT_DIR, exist_ok=True)

W, H = 72, 48

def save_bmp(im, name):
    rgba = im.convert("RGBA")
    out_im = rgba.resize((24, 16), Image.Resampling.LANCZOS)
    path = os.path.join(OUTPUT_DIR, f"{name}.bmp")
    out_im.save(path, format="BMP")
    print(f"Saved {path}")

def gen_club_flags():
    # 1. America (Amarillo canario, azul, rojo V)
    im = Image.new("RGBA", (W, H), (255, 230, 0, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (W // 2, H // 2)], fill=(13, 71, 161, 255))
    d.line([(0, 0), (W // 2, H // 2), (W, 0)], fill=(200, 16, 46, 255), width=3)
    d.ellipse([W // 2 - 8, H // 2 - 4, W // 2 + 8, H // 2 + 12], fill=(255, 230, 0, 255), outline=(13, 71, 161, 255))
    save_bmp(im, "america")

    # 2. Chivas (Rojiblanco rayas, azul marino)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, W, 12):
        d.rectangle([x + 6, 0, x + 12, H], fill=(255, 255, 255, 255))
    d.polygon([(W // 4, 4), (3 * W // 4, 4), (3 * W // 4, H - 12), (W // 2, H - 4), (W // 4, H - 12)],
              fill=(18, 58, 107, 255), outline=(212, 175, 55, 255))
    save_bmp(im, "chivas")

    # 3. Cruz Azul (Azul con cruz blanca y roja)
    im = Image.new("RGBA", (W, H), (0, 61, 165, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    d.rectangle([cx - 14, cy - 14, cx + 14, cy + 14], fill=(255, 255, 255, 255), outline=(200, 16, 46, 255), width=2)
    d.rectangle([cx - 3, cy - 10, cx + 3, cy + 10], fill=(200, 16, 46, 255))
    d.rectangle([cx - 10, cy - 3, cx + 10, cy + 3], fill=(200, 16, 46, 255))
    save_bmp(im, "cruz_azul")

    # 4. Pumas UNAM (Azul marino con Puma dorado)
    im = Image.new("RGBA", (W, H), (0, 43, 73, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    d.polygon([(cx - 16, cy + 14), (cx + 16, cy + 14), (cx + 16, cy - 6), (cx, cy - 16), (cx - 16, cy - 6)],
              fill=(196, 154, 69, 255))
    d.polygon([(cx - 10, cy + 8), (cx + 10, cy + 8), (cx + 8, cy - 2), (cx, cy - 10), (cx - 8, cy - 2)],
              fill=(0, 43, 73, 255))
    save_bmp(im, "pumas")

    # 5. Toluca (Rojo con monograma blanco)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 14, cy - 14, cx + 14, cy + 14], outline=(255, 255, 255, 255), width=3)
    d.arc([cx - 8, cy - 8, cx + 8, cy + 8], start=45, end=315, fill=(255, 255, 255, 255), width=3)
    d.line([cx - 6, cy - 4, cx + 6, cy - 4], fill=(255, 255, 255, 255), width=3)
    d.line([cx, cy - 4, cx, cy + 6], fill=(255, 255, 255, 255), width=3)
    save_bmp(im, "toluca")

    # 6. Atlas (Rojinegro mitad y mitad con 'A')
    im = Image.new("RGBA", (W, H), (26, 26, 26, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 2, H], fill=(200, 16, 46, 255))
    cx, cy = W // 2, H // 2
    d.polygon([(cx, cy - 12), (cx - 10, cy + 10), (cx + 10, cy + 10)], outline=(255, 255, 255, 255), width=3)
    d.line([cx - 6, cy + 2, cx + 6, cy + 2], fill=(255, 255, 255, 255), width=2)
    save_bmp(im, "atlas")

    # 7. Tigres UANL (Amarillo oro y franja azul con 'T')
    im = Image.new("RGBA", (W, H), (255, 204, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(0, 61, 165, 255))
    cx, cy = W // 2, H // 2
    d.line([cx - 10, cy - 6, cx + 10, cy - 6], fill=(255, 255, 255, 255), width=3)
    d.line([cx, cy - 6, cx, cy + 6], fill=(255, 255, 255, 255), width=3)
    save_bmp(im, "tigres")

    # 8. Monterrey (Azul marino y blanco rayas)
    im = Image.new("RGBA", (W, H), (11, 34, 101, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, W, 16):
        d.rectangle([x + 8, 0, x + 16, H], fill=(255, 255, 255, 255))
    cx, cy = W // 2, H // 2
    d.polygon([(cx - 10, cy - 10), (cx + 10, cy - 10), (cx, cy + 12)], fill=(200, 16, 46, 255), outline=(11, 34, 101, 255))
    save_bmp(im, "monterrey")

    # 9. Santos Laguna (Verde y blanco rayas horizontales)
    im = Image.new("RGBA", (W, H), (0, 104, 71, 255))
    d = ImageDraw.Draw(im)
    for y in range(0, H, 16):
        d.rectangle([0, y + 8, W, y + 16], fill=(255, 255, 255, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 10, cy - 10, cx + 10, cy + 10], fill=(0, 104, 71, 255), outline=(212, 175, 55, 255), width=2)
    d.polygon([(cx, cy - 7), (cx - 5, cy + 5), (cx + 5, cy + 5)], fill=(212, 175, 55, 255))
    save_bmp(im, "santos")

    # 10. Leon (Verde esmeralda con dorado)
    im = Image.new("RGBA", (W, H), (0, 122, 61, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (0, H)], fill=(255, 215, 0, 255))
    d.rectangle([2, 2, W - 3, H - 3], outline=(0, 122, 61, 255), width=2)
    save_bmp(im, "leon")

    # 11. Pachuca (Blanco y azul marino rayas verticales)
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, W, 14):
        d.rectangle([x, 0, x + 7, H], fill=(0, 51, 102, 255))
    cx, cy = W // 2, H // 2
    d.rectangle([cx - 8, cy - 12, cx + 8, cy + 12], fill=(255, 255, 255, 255), outline=(200, 16, 46, 255), width=2)
    save_bmp(im, "pachuca")

    # 12. Tijuana (Rojo y negro diagonal)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, H), (0, H)], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 10, cy - 10, cx + 10, cy + 10], fill=(26, 26, 26, 255), outline=(255, 255, 255, 255), width=2)
    save_bmp(im, "tijuana")

    # 13. Puebla (Blanco con Franja diagonal azul)
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (16, 0), (W, H - 10), (W, H), (W - 16, H), (0, 10)], fill=(0, 61, 165, 255))
    save_bmp(im, "puebla")

    # 14. Necaxa (Rojiblanco rayas verticales con rayo)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, W, 12):
        d.rectangle([x + 6, 0, x + 12, H], fill=(255, 255, 255, 255))
    cx, cy = W // 2, H // 2
    d.polygon([(cx + 2, cy - 12), (cx - 6, cy), (cx + 1, cy), (cx - 3, cy + 12), (cx + 6, cy - 1), (cx - 1, cy - 1)],
              fill=(255, 215, 0, 255), outline=(18, 58, 107, 255))
    save_bmp(im, "necaxa")

    # 15. San Luis (Rojo y azul auriazul/rojiblanco)
    im = Image.new("RGBA", (W, H), (18, 58, 107, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 2, H], fill=(200, 16, 46, 255))
    d.rectangle([W // 4, 0, W // 4 + 6, H], fill=(255, 255, 255, 255))
    save_bmp(im, "san_luis")

    # 16. Mazatlan (Morado y negro con ancla)
    im = Image.new("RGBA", (W, H), (75, 0, 130, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 12, H], fill=(26, 26, 26, 255))
    d.rectangle([W - 12, 0, W, H], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.arc([cx - 8, cy - 2, cx + 8, cy + 10], start=0, end=180, fill=(0, 206, 209, 255), width=3)
    d.line([cx, cy - 8, cx, cy + 10], fill=(0, 206, 209, 255), width=3)
    save_bmp(im, "mazatlan")

    # 17. FC Juarez (Verde bravo y franja roja/negra)
    im = Image.new("RGBA", (W, H), (0, 153, 51, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (W // 2, H // 2)], fill=(200, 16, 46, 255))
    d.polygon([(0, H), (W, H), (W // 2, H // 2)], fill=(26, 26, 26, 255))
    save_bmp(im, "juarez")

    # 18. Queretaro (Azul, negro y blanco franjas)
    im = Image.new("RGBA", (W, H), (0, 51, 102, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(26, 26, 26, 255))
    d.rectangle([0, H // 3 - 2, W, H // 3 + 2], fill=(255, 255, 255, 255))
    d.rectangle([0, 2 * H // 3 - 2, W, 2 * H // 3 + 2], fill=(255, 255, 255, 255))
    save_bmp(im, "queretaro")

    # 19. Atlante (Azulgrana franjas verticales con potro)
    im = Image.new("RGBA", (W, H), (0, 34, 68, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, W, 16):
        d.rectangle([x + 8, 0, x + 16, H], fill=(128, 0, 32, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(255, 255, 255, 255), outline=(0, 34, 68, 255))
    save_bmp(im, "atlante")

    # 20. Celaya (Blanco con V azul)
    im = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (W // 2, H - 8)], fill=(0, 61, 165, 255))
    d.polygon([(8, 0), (W - 8, 0), (W // 2, H - 20)], fill=(255, 255, 255, 255))
    save_bmp(im, "celaya")

    # 21. Atletico Morelia (Amarillo con franja diagonal roja)
    im = Image.new("RGBA", (W, H), (255, 209, 0, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (18, 0), (W, H - 12), (W, H), (W - 18, H), (0, 12)], fill=(200, 16, 46, 255))
    save_bmp(im, "morelia")

    # 22. Leones Negros UdeG (Amarillo, rojo y negro franjas horizontales)
    im = Image.new("RGBA", (W, H), (255, 204, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(200, 16, 46, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(26, 26, 26, 255), outline=(255, 204, 0, 255))
    save_bmp(im, "leones_negros")

    # 23. Mineros de Zacatecas (Rojo vino con cerro / plata)
    im = Image.new("RGBA", (W, H), (140, 16, 38, 255))
    d = ImageDraw.Draw(im)
    cx, cy = W // 2, H // 2
    d.polygon([(cx - 12, cy + 12), (cx, cy - 8), (cx + 12, cy + 12)], fill=(220, 220, 220, 255))
    save_bmp(im, "mineros")

    # 24. Venados FC (Amarillo y verde con astas ciervo)
    im = Image.new("RGBA", (W, H), (255, 209, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 16, H], fill=(0, 104, 71, 255))
    d.rectangle([W - 16, 0, W, H], fill=(0, 104, 71, 255))
    cx, cy = W // 2, H // 2
    d.polygon([(cx - 8, cy + 8), (cx, cy - 8), (cx + 8, cy + 8)], fill=(0, 104, 71, 255))
    save_bmp(im, "venados")

    # 25. Cancun FC (Turquesa caribeno y negro)
    im = Image.new("RGBA", (W, H), (0, 163, 224, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (W, H // 2), (0, H // 2)], fill=(0, 163, 224, 255))
    d.rectangle([0, H // 2, W, H], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], outline=(255, 215, 0, 255), width=2)
    save_bmp(im, "cancun")

    # 26. Correcaminos UAT (Naranja y blanco y azul)
    im = Image.new("RGBA", (W, H), (255, 102, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(255, 255, 255, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(0, 51, 153, 255))
    save_bmp(im, "correcaminos")

    # 27. Dorados de Sinaloa (Dorado y negro)
    im = Image.new("RGBA", (W, H), (212, 175, 55, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, H), (0, H)], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(212, 175, 55, 255), outline=(255, 255, 255, 255))
    save_bmp(im, "dorados")

    # 28. Alebrijes de Oaxaca (Naranja vibrante y verde)
    im = Image.new("RGBA", (W, H), (255, 102, 0, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (W // 2, H // 2)], fill=(0, 153, 51, 255))
    d.rectangle([0, H - 10, W, H], fill=(26, 26, 26, 255))
    save_bmp(im, "alebrijes")

    # 29. Tlaxcala FC (Rojo coyote y negro)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W // 3, H], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.polygon([(cx - 8, cy + 8), (cx + 8, cy + 8), (cx, cy - 8)], fill=(255, 255, 255, 255))
    save_bmp(im, "tlaxcala")

    # 30. Club Atletico La Paz (Azul marino y dorado y blanco)
    im = Image.new("RGBA", (W, H), (0, 31, 63, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, H), (W, 0), (W, H)], fill=(212, 175, 55, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=(255, 255, 255, 255))
    save_bmp(im, "la_paz")

    # 31. Jaiba Brava (Celeste y blanco con jaiba)
    im = Image.new("RGBA", (W, H), (0, 153, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(255, 255, 255, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 9, cy - 6, cx + 9, cy + 6], fill=(200, 16, 46, 255))
    save_bmp(im, "jaiba_brava")

    # 32. CD Tapatio (Rojiblanco con franja azul)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, W, 14):
        d.rectangle([x + 7, 0, x + 14, H], fill=(255, 255, 255, 255))
    d.rectangle([0, H - 10, W, H], fill=(18, 58, 107, 255))
    save_bmp(im, "tapatio")

    # 33. Tepatitlan FC (Azul y rojo mitad y mitad)
    im = Image.new("RGBA", (W, H), (0, 51, 153, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([W // 2, 0, W, H], fill=(200, 16, 46, 255))
    cx, cy = W // 2, H // 2
    d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(255, 215, 0, 255))
    save_bmp(im, "tepatitlan")

    # 34. Cimarrones de Sonora (Rojo vino, azul marino y blanco)
    im = Image.new("RGBA", (W, H), (140, 16, 38, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 2, W, H], fill=(0, 31, 63, 255))
    d.line([0, H // 2, W, H // 2], fill=(255, 255, 255, 255), width=2)
    save_bmp(im, "cimarrones")

    # 35. Tiburones Rojos de Veracruz (Rojo con Gran 'V' Blanca)
    im = Image.new("RGBA", (W, H), (200, 16, 46, 255))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (W, 0), (W // 2, H - 8)], fill=(255, 255, 255, 255))
    d.polygon([(8, 0), (W - 8, 0), (W // 2, H - 22)], fill=(200, 16, 46, 255))
    d.rectangle([0, H - 8, W, H], fill=(0, 34, 68, 255))
    save_bmp(im, "veracruz")

    # 36. Toros Neza (Rojo, blanco y negro con cabeza de toro)
    im = Image.new("RGBA", (W, H), (204, 0, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, H // 3, W, 2 * H // 3], fill=(255, 255, 255, 255))
    d.rectangle([0, 2 * H // 3, W, H], fill=(26, 26, 26, 255))
    cx, cy = W // 2, H // 2
    d.polygon([(cx - 8, cy - 4), (cx + 8, cy - 4), (cx, cy + 8)], fill=(204, 0, 0, 255), outline=(26, 26, 26, 255))
    save_bmp(im, "toros_neza")

if __name__ == "__main__":
    gen_club_flags()
