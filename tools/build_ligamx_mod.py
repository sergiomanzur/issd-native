#!/usr/bin/env python3
"""Build the complete Liga MX and Liga de Expansión MX mod pack for ISSD Native.
Replaces all 36 teams in the cartridge with Mexican clubs.
"""
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODS_DIR = os.path.join(REPO, "mods")
OUTPUT_PATH = os.path.join(MODS_DIR, "liga_mx_expansion.json")

def sanitize_name(name):
    name = name.replace("-", " ").replace("'", "")
    name = "".join(c for c in name if c.isascii() and (c.isalpha() or c in " ."))
    return name[:8]

def create_player(num, name, pos, acc, spd, sht, tec, bal, intl, drb, jmp, sta, gk, skin=0, hair=1):
    return {
        "shirt_number": num,
        "name": sanitize_name(name),
        "position": pos,
        "skin_tone": skin,
        "hair_style": 0 if pos == "GK" else hair,
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
    }

def make_squad(players_raw, tier_boost=0):
    players = []
    for i, p in enumerate(players_raw):
        num, name, pos, role_type, quality = p
        q = quality + tier_boost
        skin = 0 if i % 3 != 0 else 1
        hair = (i % 3) + 1

        if pos == "GK":
            acc = max(45, min(65, 52 + q))
            spd = max(45, min(65, 54 + q))
            sht = 20
            tec = max(40, min(60, 48 + q))
            bal = max(68, min(75, 72))
            intl = max(55, min(65, 60))
            drb = 25
            jmp = max(75, min(95, 82 + q))
            sta = max(60, min(75, 68 + q))
            gk = max(70, min(98, 82 + q * 2))
            hair = 0
        elif pos == "DF":
            if role_type == "CB":
                acc = max(60, min(80, 68 + q))
                spd = max(62, min(82, 70 + q))
                sht = max(30, min(50, 40 + q))
                tec = max(55, min(72, 62 + q))
                bal = max(78, min(92, 85 + q))
                intl = max(68, min(86, 76 + q))
                drb = max(45, min(65, 55 + q))
                jmp = max(76, min(92, 84 + q))
                sta = max(72, min(88, 80 + q))
                gk = 8
            else: # FB
                acc = max(72, min(90, 80 + q))
                spd = max(74, min(92, 82 + q))
                sht = max(45, min(65, 55 + q))
                tec = max(65, min(80, 72 + q))
                bal = max(65, min(78, 70 + q))
                intl = max(65, min(80, 72 + q))
                drb = max(68, min(82, 74 + q))
                jmp = max(60, min(78, 68 + q))
                sta = max(80, min(95, 86 + q))
                gk = 8
        elif pos == "MF":
            if role_type == "DM":
                acc = max(65, min(82, 72 + q))
                spd = max(66, min(82, 74 + q))
                sht = max(55, min(75, 65 + q))
                tec = max(70, min(84, 76 + q))
                bal = max(78, min(90, 84 + q))
                intl = max(75, min(90, 82 + q))
                drb = max(68, min(82, 74 + q))
                jmp = max(68, min(85, 74 + q))
                sta = max(82, min(96, 88 + q))
                gk = 8
            else: # AM / Wing
                acc = max(76, min(92, 84 + q))
                spd = max(78, min(94, 86 + q))
                sht = max(70, min(88, 78 + q))
                tec = max(80, min(95, 86 + q))
                bal = max(65, min(80, 72 + q))
                intl = max(75, min(90, 82 + q))
                drb = max(80, min(95, 86 + q))
                jmp = max(54, min(72, 62 + q))
                sta = max(75, min(90, 82 + q))
                gk = 8
        else: # FW
            acc = max(75, min(92, 82 + q))
            spd = max(78, min(94, 84 + q))
            sht = max(78, min(96, 86 + q * 2))
            tec = max(74, min(88, 80 + q))
            bal = max(74, min(90, 82 + q))
            intl = max(72, min(90, 80 + q))
            drb = max(72, min(88, 80 + q))
            jmp = max(72, min(92, 80 + q))
            sta = max(74, min(88, 80 + q))
            gk = 8

        # Ensure dynamic range across ratings
        if i in (1, 3, 12, 14, 16):
            intl = max(58, min(72, intl - 14))
        if i in (2, 4, 13, 15, 17):
            bal = max(60, min(74, bal - 14))
        if i in (4, 7, 14, 18):
            jmp = max(52, min(65, jmp - 15))
        elif i in (2, 8, 10, 15):
            jmp = max(82, min(96, jmp + 10))

        players.append(create_player(num, name, pos, acc, spd, sht, tec, bal, intl, drb, jmp, sta, gk, skin, hair))
    return players
