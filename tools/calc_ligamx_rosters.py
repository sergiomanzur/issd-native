import json
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODS_DIR = os.path.join(REPO, "mods")
OUTPUT_PATH = os.path.join(MODS_DIR, "liga_mx_expansion.json")

def sanitize_name(name):
    name = name.replace("-", " ").replace("'", "")
    name = "".join(c for c in name if c.isascii() and (c.isalpha() or c in " ."))
    return name[:8]

def calc_attributes(pos, role, quality, speed_stat, skill_stat, power_stat, slot_idx):
    """
    Calculates realistic attributes based on player's position, role, quality tier (0..10),
    and relative attributes (speed, skill, power, 0..10).
    """
    q = quality
    sp = speed_stat
    sk = skill_stat
    pw = power_stat

    if pos == "GK":
        acc = max(45, min(65, 50 + sp))
        spd = max(45, min(65, 52 + sp))
        sht = 20
        tec = max(40, min(60, 45 + sk))
        bal = max(68, min(76, 70 + pw // 2))
        intl = max(56, min(66, 58 + sk // 2))
        drb = 25
        jmp = max(76, min(96, 78 + pw * 2))
        sta = max(60, min(76, 65 + q))
        gk = max(70, min(99, 72 + q * 3))
    elif pos == "DF":
        if role == "CB":
            acc = max(58, min(82, 65 + sp * 2))
            spd = max(60, min(84, 66 + sp * 2))
            sht = max(28, min(55, 36 + pw))
            tec = max(52, min(75, 58 + sk * 2))
            bal = max(76, min(95, 80 + pw * 2))
            intl = max(66, min(88, 72 + q * 2))
            drb = max(42, min(68, 50 + sk * 2))
            jmp = max(74, min(95, 78 + pw * 2))
            sta = max(70, min(92, 76 + q * 2))
            gk = 8
        else: # FB
            acc = max(72, min(94, 76 + sp * 2))
            spd = max(74, min(95, 78 + sp * 2))
            sht = max(42, min(68, 50 + sk))
            tec = max(62, min(84, 68 + sk * 2))
            bal = max(64, min(82, 68 + pw))
            intl = max(64, min(84, 68 + q * 2))
            drb = max(66, min(86, 70 + sk * 2))
            jmp = max(58, min(78, 64 + pw))
            sta = max(78, min(96, 82 + q * 2))
            gk = 8
    elif pos == "MF":
        if role == "DM":
            acc = max(64, min(84, 70 + sp))
            spd = max(65, min(85, 72 + sp))
            sht = max(52, min(78, 62 + pw))
            tec = max(68, min(88, 74 + sk * 2))
            bal = max(76, min(92, 80 + pw * 2))
            intl = max(72, min(92, 78 + q * 2))
            drb = max(66, min(85, 72 + sk * 2))
            jmp = max(66, min(86, 72 + pw))
            sta = max(80, min(98, 85 + q * 2))
            gk = 8
        else: # AM / Wing
            acc = max(75, min(96, 80 + sp * 2))
            spd = max(76, min(96, 82 + sp * 2))
            sht = max(68, min(92, 74 + pw * 2))
            tec = max(78, min(98, 82 + sk * 2))
            bal = max(64, min(84, 70 + pw))
            intl = max(72, min(94, 78 + q * 2))
            drb = max(78, min(98, 82 + sk * 2))
            jmp = max(54, min(75, 62 + pw))
            sta = max(74, min(92, 80 + q))
            gk = 8
    else: # FW
        acc = max(74, min(96, 80 + sp * 2))
        spd = max(76, min(96, 82 + sp * 2))
        sht = max(76, min(99, 82 + pw * 2))
        tec = max(72, min(92, 78 + sk * 2))
        bal = max(72, min(92, 78 + pw * 2))
        intl = max(70, min(94, 76 + q * 2))
        drb = max(70, min(92, 76 + sk * 2))
        jmp = max(70, min(95, 78 + pw * 2))
        sta = max(72, min(90, 78 + q))
        gk = 8

    # Dynamic range variation across slot indices to prevent uniform nibbles
    if slot_idx in (1, 3, 12, 14, 16):
        intl = max(56, min(72, intl - 14))
    if slot_idx in (2, 4, 13, 15, 17):
        bal = max(58, min(74, bal - 14))
    if slot_idx in (4, 7, 14, 18):
        jmp = max(50, min(65, jmp - 16))
    elif slot_idx in (2, 8, 10, 15):
        jmp = max(80, min(96, jmp + 8))

    return acc, spd, sht, tec, bal, intl, drb, jmp, sta, gk

def make_squad(players_raw, tier_boost=0):
    players = []
    for i, p in enumerate(players_raw):
        if len(p) == 5:
            num, name, pos, role, q = p
            sp, sk, pw = 6, 6, 6
        else:
            num, name, pos, role, q, sp, sk, pw = p[:8]
        acc, spd, sht, tec, bal, intl, drb, jmp, sta, gk = calc_attributes(
            pos, role, q + tier_boost, sp, sk, pw, i
        )
        skin = 0 if i % 3 != 0 else 1
        hair = 0 if pos == "GK" else ((i % 3) + 1)
        players.append({
            "shirt_number": num,
            "name": sanitize_name(name),
            "position": pos,
            "skin_tone": skin,
            "hair_style": hair,
            "acceleration": acc,
            "speed": spd,
            "shooting": sht,
            "technique": tec,
            "balance": bal,
            "intelligence": intl,
            "dribbling": drb,
            "jumping": jmp,
            "stamina": sta,
            "goalkeeping": gk
        })
    return players
