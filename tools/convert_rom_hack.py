#!/usr/bin/env python3
"""Turn a ROM hack into a cartridge pack, with a compatibility report.

    python tools/convert_rom_hack.py --rom "World Cup 2026.sfc" \
        --name "World Cup 2026" --author "..." --version "1.0.0"

    python tools/convert_rom_hack.py --ips "world_cup_2026.ips" --name "..."

A cartridge pack is a flat `mods/<slug>.json` whose `cartridge` block points at
an IPS patch beside it (`mods/<slug>/<slug>.ips`). The patch is applied to the
user's own verified retail ROM at load time, so no ROM or hack bytes are ever
part of a pack we commit. A full hack image passed with `--rom` is diffed
against the retail base to produce that IPS; a 2,097,664-byte image is treated
as a 512-byte-copier-headered dump and the header is dropped (ruling 2).

`REPORT.md` is the honesty contract (ruling 7). The engine is a hybrid:

  * **interpreted** - boot (`$80:8000`), NMI (`$80:80E0`) and the PCs listed in
    `recomp/aot_boot_deny.txt` run from cartridge bytes, so a change here takes
    effect;
  * **known data** - tables the runtime reads from the image (rosters, names,
    kits, palettes, plates, stadium tables), so a hack's bytes here are honored;
  * **not guaranteed** - everything else. The precompiled C carries its own copy
    of the code, so changed code is ignored; relocated data is still read from
    its original addresses.

The classification is best-effort by file offset, and the report says so.

Refusals mirror the loader's (Chain A's `issd_cartridge.c`): the result must be
exactly 2,097,152 bytes, LoROM (mapping byte 0x7FD5 bit 0 clear) and ROM-only
(type byte 0x7FD6 == 0x00); an IPS may not write past 2 MB nor end before its
EOF marker. The tool refuses the same inputs, so it never writes a pack the
loader would reject, and it re-applies the IPS it just wrote before writing
anything: applying it to the base reproduces the hack byte for byte.

Offsets and table extents come from `ISSDNative/issd_mod_rom.c` and
`docs/REVERSE_ENGINEERING.md`; the live-PC list is read from
`recomp/aot_boot_deny.txt` at run time rather than copied here.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

# The retail cartridge. Everything here is about *this* revision: the loader
# verifies the same hash before it applies a patch on any path (ruling 3).
RETAIL_NAME = "International Superstar Soccer Deluxe (USA).sfc"
RETAIL_SIZE = 2_097_152
RETAIL_SHA256 = "d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9"
HEADER_SIZE = 512
IMAGE_SIZE = RETAIL_SIZE

MAPPING_BYTE = 0x7FD5
CART_TYPE_BYTE = 0x7FD6

# Boot/NMI live in the first page of bank $80. Ruling 7 names the entries; the
# page is the honest extent the report can promise.
BOOT_AREA = (0x0000, 0x0200)

DENY_LIST = REPO / "recomp" / "aot_boot_deny.txt"

# Changed runs closer than this are reported and encoded as one region.
MERGE_GAP = 16
# A run at least this long is cheaper as IPS RLE (8 bytes) than literal (5 + n).
RLE_MIN = 16

MAX_RECORD = 0xFFFF


class ConversionError(Exception):
    """A hack the loader would refuse, or a base that is not the retail ROM."""


# A table the runtime reads out of the image. `free` marks the runs regular
# packs write into; a hack occupying those makes those writes refuse (ruling 5).
class Region:
    __slots__ = ("name", "start", "end", "citation", "free")

    def __init__(self, name, start, end, citation, free=False):
        self.name = name
        self.start = start
        self.end = end
        self.citation = citation
        self.free = free


# Seeded from ISSDNative/issd_mod_rom.c and docs/REVERSE_ENGINEERING.md. The
# extents use the counts in that file: 42 selectable teams of 20 players, 84
# kit records of 34 bytes, twelve 24-byte head combos, 43 pointer entries.
KNOWN_REGIONS = (
    Region("cell table", 0x00DA3F, 0x00DA3F + 42,
           "ISSDNative/issd_mod_rom.c ROM_CELL_TABLE 0xDA3F, 42 doubled "
           "team indices"),
    Region("kit pointer tables", 0x01027A, 0x010326,
           "ISSDNative/issd_mod_rom.c ROM_KIT_PTRS_HOME 0x1027A / "
           "ROM_KIT_PTRS_AWAY 0x102D0, 43 x 2 bytes each"),
    Region("tone pointer tables", 0x01037C, 0x010428,
           "ISSDNative/issd_mod_rom.c ROM_TONE1_SKIN_PTRS 0x1037C / "
           "ROM_TONE1_HAIR_PTRS 0x103D2, 43 x 2 bytes each"),
    Region("head combo index", 0x01795F, 0x01795F + 42 * 2,
           "ISSDNative/issd_mod_rom.c ROM_HEAD_INDEX 0x1795F, two head bytes "
           "per team (DATA_82F95F)"),
    Region("bank $82 free space ($82:FB5D)", 0x017B5D, 0x018000,
           "ISSDNative/issd_mod_rom.c ROM_FREE_BANK82 0x17B5D, 1187 bytes "
           "extended stadiums write into - pack free space", free=True),
    Region("stadium pitches", 0x017AED, 0x017AED + 8 * 2,
           "ISSDNative/issd_mod_rom.c ROM_STADIUM_PITCH_BASE 0x17AED, "
           "8 (length, width) pairs"),
    Region("roster names", 0x03818E, 0x03818E + 36 * 20 * 8,
           "ISSDNative/issd_mod_rom.c ROM_NAME_BASE 229774 (0x3818E); names "
           "run out at 36 squads - 36 x 20 x 8 bytes (see the has_roster "
           "comment)"),
    Region("roster pointer tables", 0x038138, 0x038138 + 43 * 2,
           "ISSDNative/issd_mod_rom.c ROM_ROSTER_PTRS 0x38138, 43 x 2 bytes"),
    Region("roster pointer table copy", 0x0398AE, 0x0398AE + 43 * 2,
           "ISSDNative/issd_mod_rom.c ROM_ROSTER_PTRS_COPY 0x398AE, "
           "43 x 2 bytes"),
    Region("stadium names", 0x03CA9B, 0x03CA9B + 8 * 7,
           "ISSDNative/issd_mod_rom.c ROM_STADIUM_NAME_BASE 0x3CA9B, "
           "8 x 7 bytes (through where STADIUM begins)"),
    Region("bank $87 free space ($87:FAC8)", 0x03FAC8, 0x040000,
           "ISSDNative/issd_mod_rom.c ROM_FREE_BANK87 0x3FAC8, 1336 bytes "
           "extended stadium names write into - pack free space", free=True),
    Region("added-roster space ($87:FC40)", 0x03FC40, 0x040000,
           "ISSDNative/issd_mod_rom.c ROM_ADDED_ROSTERS 0x3FC40, six "
           "20-player squads at the tail of bank $87 - pack free space",
           free=True),
    Region("kit records", 0x0483C0, 0x0483C0 + 84 * 34,
           "ISSDNative/issd_mod_rom.c ROM_KIT_BASE 0x483C0, 84 records x "
           "34 bytes (ROM_KIT_STRIDE)"),
    Region("head combo palettes", 0x04DD6A, 0x04DE8A,
           "ISSDNative/issd_mod_rom.c ROM_HEAD_COMBO_SKIN 0x4DD6C, twelve "
           "24-byte records from 0x4DD6A (DATA_89DD6A)"),
    Region("tone-1 free space ($89:FA42)", 0x04FA42, 0x050000,
           "ISSDNative/issd_mod_rom.c ROM_TONE1_FREE_BASE 0x4FA42..0x50000, "
           "alternate skin/hair palettes write here - pack free space",
           free=True),
    Region("roster attributes", 0x050000, 0x050000 + 42 * 20 * 7,
           "ISSDNative/issd_mod_rom.c ROM_ATTR_BASE 327680 (0x50000), "
           "42 teams x 20 players x 7 bytes"),
    Region("formation pointer table", 0x05EF48, 0x05EF48 + 36 * 2,
           "ISSDNative/issd_mod_rom.c ROM_FORMATION_PTRS 0x5EF48, one 16-bit "
           "pointer per team into bank $8B"),
    Region("formation records", 0x05EF9E, 0x05EF9E + 36 * 31,
           "ISSDNative/issd_mod_rom.c note: records packed from $8B:EF9E, "
           "36 x 31 bytes (label, ten positions, ten roles)"),
)

# Reported separately because they are structural: the loader reads these to
# decide whether to accept the image at all.
HEADER_FIELDS = (
    ("title", 0x7FC0, 0x7FD5),
    ("mapping (LoROM bit)", 0x7FD5, 0x7FD6),
    ("cartridge type", 0x7FD6, 0x7FD7),
    ("ROM size", 0x7FD7, 0x7FD8),
    ("SRAM size", 0x7FD8, 0x7FD9),
    ("country", 0x7FD9, 0x7FDA),
    ("license", 0x7FDA, 0x7FDB),
    ("version", 0x7FDB, 0x7FDC),
    ("checksum complement", 0x7FDC, 0x7FDE),
    ("checksum", 0x7FDE, 0x7FE0),
    ("vector table", 0x7FE0, 0x8000),
)


# --- IPS ------------------------------------------------------------------

def changed_spans(base, hack, gap=MERGE_GAP):
    """Ranges that differ, adjacent runs merged across at most `gap` bytes."""
    if len(base) != len(hack):
        raise ConversionError("the diff needs two images of the same size")
    n = len(base)
    spans = []
    i = _next_diff(base, hack, 0, n)
    while i < n:
        start = last = i
        i = _next_diff(base, hack, i + 1, n)
        while i < n and i - last - 1 <= gap:
            last = i
            i = _next_diff(base, hack, i + 1, n)
        spans.append((start, last + 1))
    return spans


def _next_diff(base, hack, i, n, block=4096):
    while i < n:
        if i + block <= n:
            if base[i:i + block] == hack[i:i + block]:
                i += block
                continue
        if base[i] == hack[i]:
            i += 1
            continue
        return i
    return n


def make_ips(base, hack, gap=MERGE_GAP):
    """A canonical IPS from `base` to `hack`: literals plus RLE for long runs."""
    if len(base) != len(hack):
        raise ConversionError("the diff needs two images of the same size")
    out = bytearray(b"PATCH")
    for start, end in changed_spans(base, hack, gap):
        for kind, a, b in _chunks(hack, start, end):
            if kind == "rle":
                out += a.to_bytes(3, "big") + b"\x00\x00"
                out += (b - a).to_bytes(2, "big") + bytes((hack[a],))
            else:
                at = a
                while at < b:
                    size = min(b - at, MAX_RECORD)
                    out += at.to_bytes(3, "big") + size.to_bytes(2, "big")
                    out += hack[at:at + size]
                    at += size
    out += b"EOF"
    return bytes(out)


def _chunks(hack, start, end):
    """Split one merged span into literal and RLE pieces."""
    pieces = []
    literal = start
    i = start
    while i < end:
        j = i + 1
        while j < end and hack[j] == hack[i]:
            j += 1
        if j - i >= RLE_MIN:
            if literal < i:
                pieces.append(("lit", literal, i))
            at = i
            while at < j:
                upto = min(j, at + MAX_RECORD)
                pieces.append(("rle", at, upto))
                at = upto
            literal = j
        i = j
    if literal < end:
        pieces.append(("lit", literal, end))
    return pieces


def apply_ips(base, patch):
    """Apply IPS to `base`, refusing everything the C loader refuses."""
    if not patch.startswith(b"PATCH"):
        raise ConversionError("not an IPS patch: it does not start with PATCH")
    out = bytearray(base)
    pos = 5
    saw_eof = False
    while pos < len(patch):
        if patch[pos:pos + 3] == b"EOF":
            pos += 3
            saw_eof = True
            break
        if pos + 5 > len(patch):
            raise ConversionError("IPS record is truncated at offset %d" % pos)
        offset = int.from_bytes(patch[pos:pos + 3], "big")
        size = int.from_bytes(patch[pos + 3:pos + 5], "big")
        pos += 5
        if size:
            if pos + size > len(patch):
                raise ConversionError(
                    "IPS payload at %d is truncated" % pos)
            end = offset + size
            if end > IMAGE_SIZE:
                raise ConversionError(
                    "IPS writes to 0x%06X, past the 2 MB image limit; the "
                    "image would expand, which is unsupported" % (end - 1))
            out[offset:end] = patch[pos:pos + size]
            pos += size
        else:
            if pos + 3 > len(patch):
                raise ConversionError(
                    "IPS RLE record at 0x%06X is truncated" % offset)
            run = int.from_bytes(patch[pos:pos + 2], "big")
            fill = patch[pos + 2]
            pos += 3
            if run == 0:
                raise ConversionError(
                    "IPS RLE record at 0x%06X has a zero run length" % offset)
            end = offset + run
            if end > IMAGE_SIZE:
                raise ConversionError(
                    "IPS fills to 0x%06X, past the 2 MB image limit; the "
                    "image would expand, which is unsupported" % (end - 1))
            out[offset:end] = bytes((fill,)) * run
    if not saw_eof:
        raise ConversionError(
            "the IPS ends before its EOF marker (truncated patch); the "
            "loader refuses this, so the converter does too")
    if pos != len(patch):
        raise ConversionError(
            "%d byte(s) of data after the IPS EOF marker" % (len(patch) - pos))
    if len(out) != IMAGE_SIZE:
        raise ConversionError(
            "the patched image is %d bytes; the loader requires exactly %d"
            % (len(out), IMAGE_SIZE))
    return bytes(out)


# --- images and the base ---------------------------------------------------

def normalize_image(data, label="image"):
    """Return (headerless 2 MB image, whether a copier header was dropped)."""
    if len(data) == IMAGE_SIZE:
        return data, False
    if len(data) == IMAGE_SIZE + HEADER_SIZE:
        return data[HEADER_SIZE:], True
    if len(data) > IMAGE_SIZE:
        raise ConversionError(
            "%s is %d bytes; it expands past the 2 MB image limit (%d). "
            "Expanded ROM hacks are unsupported."
            % (label, len(data), IMAGE_SIZE))
    raise ConversionError(
        "%s is %d bytes; expected exactly %d (headerless) or %d "
        "(512-byte copier header)"
        % (label, len(data), IMAGE_SIZE, IMAGE_SIZE + HEADER_SIZE))


def validate_image(image, label="image"):
    """The structural checks the loader applies to every substituted image."""
    if len(image) != IMAGE_SIZE:
        raise ConversionError(
            "%s is %d bytes; the loader requires exactly %d"
            % (label, len(image), IMAGE_SIZE))
    mapping = image[MAPPING_BYTE]
    if mapping & 0x01:
        raise ConversionError(
            "%s is not LoROM: mapping byte 0x7FD5 is 0x%02X (bit 0 set). "
            "The loader only accepts LoROM images." % (label, mapping))
    cart = image[CART_TYPE_BYTE]
    if cart != 0x00:
        raise ConversionError(
            "%s is not ROM-only: cartridge type byte 0x7FD6 is 0x%02X. "
            "The loader accepts only type 0x00." % (label, cart))


def load_base(path=None):
    """Read and verify the retail cartridge; return (path, 2 MB bytes)."""
    if path is None:
        candidate = Path.cwd() / RETAIL_NAME
        if not candidate.is_file():
            raise ConversionError(
                "no retail cartridge to diff against: expected %r in %s.\n"
                "Pass --base PATH pointing at the headerless 2 MB %s "
                "(sha256 %s)."
                % (RETAIL_NAME, Path.cwd(), RETAIL_NAME, RETAIL_SHA256))
        path = candidate
    else:
        path = Path(path)
        if not path.is_file():
            raise ConversionError(
                "base ROM not found: %s\nPass --base PATH pointing at %r "
                "(sha256 %s)." % (path, RETAIL_NAME, RETAIL_SHA256))
    data = path.read_bytes()
    if len(data) == IMAGE_SIZE + HEADER_SIZE:
        data = data[HEADER_SIZE:]
    if len(data) != IMAGE_SIZE:
        raise ConversionError(
            "base ROM %s is %d bytes; the retail cartridge is %d bytes "
            "(a 512-byte-headered dump is also accepted). Pass --base PATH "
            "pointing at %r."
            % (path, len(data), IMAGE_SIZE, RETAIL_NAME))
    digest = hashlib.sha256(data).hexdigest()
    if digest != RETAIL_SHA256:
        raise ConversionError(
            "base ROM %s is not the retail cartridge: sha256 %s, expected "
            "%s.\nUse --base PATH to point at %r; patches are only applied "
            "to that verified image."
            % (path, digest, RETAIL_SHA256, RETAIL_NAME))
    return path, data


# --- the hybrid-engine classifier ------------------------------------------

def pc_to_file(pc):
    """LoROM PC -> file offset, including the bank-$80 mirror."""
    return ((pc >> 16) & 0x7F) * 0x8000 + (pc & 0x7FFF)


def load_deny_pcs(path=None):
    """Read the live-routine PCs at run time; never copy the list here."""
    path = Path(path) if path is not None else DENY_LIST
    if not path.is_file():
        return []
    pcs = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        token = line.strip()
        if re.fullmatch(r"[0-9A-Fa-f]{6}", token):
            pcs.append(int(token, 16))
        else:
            match = re.fullmatch(r"\$([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4})", token)
            if match:
                pcs.append(int(match.group(1), 16) << 16 |
                           int(match.group(2), 16))
    return pcs


def deny_map(pcs):
    """{file offset: [PCs]} so the report can name what makes a range live."""
    mapped = {}
    for pc in pcs:
        mapped.setdefault(pc_to_file(pc), []).append(pc)
    for offsets in mapped.values():
        offsets.sort()
    return mapped


def classify_span(start, end, deny=None, deny_label=None):
    """(category, details) for a file-offset range: interpreted, known, else."""
    deny = deny or {}
    label = deny_label or DENY_LIST.name
    boot = start < BOOT_AREA[1] and end > BOOT_AREA[0]
    live = [pc for offset, pcs in deny.items() if start <= offset < end
            for pc in pcs]
    if boot or live:
        detail = []
        if boot:
            detail.append("boot/NMI area (file 0x000000..0x0001FF; $80:8000 "
                          "reset and $80:80E0 NMI run interpreted)")
        if live:
            detail.append("deny-listed PC%s in %s: %s"
                          % ("" if len(live) == 1 else "s", label,
                             ", ".join("$%02X:%04X (0x%06X)"
                                       % (pc >> 16, pc & 0xFFFF, pc)
                                       for pc in sorted(live))))
        return "interpreted", detail

    touched = [region for region in KNOWN_REGIONS
               if start < region.end and end > region.start]
    if touched:
        return "known", ["%s [%s]" % (region.name, region.citation)
                         for region in touched]
    return "unknown", ["no known table here"]


# --- the report ------------------------------------------------------------

def _display_path(path):
    try:
        return Path(path).resolve().relative_to(REPO).as_posix()
    except ValueError:
        return str(path)


def _hexdump(data):
    return " ".join("%02X" % byte for byte in data)


def _header_changes(base, image):
    lines = []
    for name, start, end in HEADER_FIELDS:
        if base[start:end] == image[start:end]:
            continue
        old, new = base[start:end], image[start:end]
        lines.append("- 0x%06X..0x%06X %s: %s -> %s"
                     % (start, end - 1, name, _hexdump(old),
                        _hexdump(new)))
    return lines


def _span_line(start, end, base, image, detail):
    changed = sum(1 for a, b in zip(base[start:end], image[start:end])
                  if a != b)
    return "- 0x%06X..0x%06X (%d byte%s changed): %s" % (
        start, end - 1, changed, "" if changed == 1 else "s", "; ".join(detail))


def build_report(*, name, input_label, ips_name, base_path, base, image,
                 patch, deny_path, deny_pcs, headered):
    spans = changed_spans(base, image)
    deny = deny_map(deny_pcs)
    buckets = {"interpreted": [], "known": [], "unknown": []}
    free_hits = []
    for start, end in spans:
        category, detail = classify_span(start, end, deny,
                                         _display_path(deny_path))
        if category == "known":
            free = [region.name for region in KNOWN_REGIONS
                    if region.free and start < region.end and end > region.start]
            if free:
                detail = list(detail)
                detail.append("occupied: pack free space at risk - regular "
                              "packs that write here now refuse (ruling 5)")
                free_hits.append((start, end, free))
        buckets[category].append((start, end, detail))

    records = _count_records(patch)
    changed_bytes = sum(1 for a, b in zip(base, image) if a != b)

    out = []
    out.append("# %s - cartridge compatibility report" % name)
    out.append("")
    out.append(
        "`tools/convert_rom_hack.py` diffed `%s` against the retail base "
        "`%s` (sha256 `%s`)."
        % (input_label, base_path.name, hashlib.sha256(base).hexdigest()))
    out.append("")
    out.append(
        "Result: %s bytes, sha256 `%s`; `%s` holds %d IPS record%s "
        "covering %d changed byte%s."
        % ("{:,}".format(len(image)), hashlib.sha256(image).hexdigest(),
           ips_name, records, "" if records == 1 else "s",
           changed_bytes, "" if changed_bytes == 1 else "s"))
    if headered:
        out.append("")
        out.append("The input carried a 512-byte copier header; it was "
                   "dropped, so the pack holds a headerless image (ruling 2).")
    out.append("")
    if deny_pcs:
        out.append("Deny list: `%s` (%d PC%s -> %d live file offset%s)."
                   % (_display_path(deny_path), len(deny_pcs),
                      "" if len(deny_pcs) == 1 else "s", len(deny),
                      "" if len(deny) == 1 else "s"))
    else:
        out.append("Deny list: `%s` is absent or empty, so only boot and NMI "
                   "are treated as live." % _display_path(deny_path))
    out.append("")
    if not changed_bytes:
        out.append("This image is byte-identical to the retail cartridge, so "
                   "the pack changes nothing. Check that the hack was saved.")
        out.append("")
    out.append("## Summary")
    out.append("")
    out.append("This is a best-effort map of file offsets, not a promise about "
               "any particular hack.")
    out.append("")
    out.append("| Category | Regions | Changed bytes |")
    out.append("|---|---|---|")
    labels = (("interpreted", "Interpreted - changes take effect (live)"),
              ("known", "Known data - honored"),
              ("unknown", "Not guaranteed - precompiled engine"))
    for key, label in labels:
        rows = buckets[key]
        out.append("| %s | %d | %d |"
                   % (label, len(rows),
                      sum(1 for start, end, _ in rows
                          for a, b in zip(base[start:end], image[start:end])
                          if a != b)))
    out.append("")

    out.append("## Interpreted - changes take effect (live)")
    out.append("")
    out.append("Boot (`$80:8000`), NMI (`$80:80E0`) and the routines in the "
               "deny list run from the cartridge bytes in this build, so a "
               "change here takes effect.")
    out.append("")
    if buckets["interpreted"]:
        out.extend(_span_line(start, end, base, image, detail)
                   for start, end, detail in buckets["interpreted"])
    else:
        out.append("_None._")
    out.append("")

    out.append("## Known data - honored")
    out.append("")
    out.append("These tables are read from the image at run time, so a hack's "
               "bytes here are used. Extents come from "
               "`ISSDNative/issd_mod_rom.c` and `docs/REVERSE_ENGINEERING.md`.")
    out.append("")
    if buckets["known"]:
        out.extend(_span_line(start, end, base, image, detail)
                   for start, end, detail in buckets["known"])
    else:
        out.append("_None._")
    out.append("")

    out.append("## Not guaranteed - precompiled engine")
    out.append("")
    out.append("Changed code here is ignored: the precompiled C carries its own "
               "copy, and an immediate that lives in a recompiled body is not "
               "in the ROM any more. Relocated data is still read from its "
               "original addresses. A routine at an unknown address may still "
               "run interpreted if a live routine calls into it, which a "
               "file-offset map cannot see.")
    out.append("")
    if buckets["unknown"]:
        out.extend(_span_line(start, end, base, image, detail)
                   for start, end, detail in buckets["unknown"])
    else:
        out.append("_None._")
    out.append("")

    out.append("## Cartridge header")
    out.append("")
    header = _header_changes(base, image)
    if header:
        out.append("The header is structural: the loader reads it before it "
                   "accepts the image. These fields changed and were checked "
                   "against the loader's rules.")
        out.append("")
        out.extend(header)
        out.append("")
        out.append("Mapping is LoROM (bit 0 clear) and the cartridge type is "
                   "0x00 (ROM-only), or the converter would have refused the "
                   "image.")
    else:
        out.append("Unchanged: 2 MB, LoROM, ROM-only; title, mapping, "
                   "cartridge type, size and checksum are the retail image's.")
    out.append("")

    out.append("## Pack free space")
    out.append("")
    if free_hits:
        out.append("Regular packs write added teams, tone-1 palettes and "
                   "extended stadiums into the runs below. This hack occupies "
                   "part of one, so those writes will refuse rather than "
                   "overwrite hack content (ruling 5).")
        out.append("")
        for start, end, names in free_hits:
            out.append("- 0x%06X..0x%06X: %s - occupied: pack free space at "
                       "risk" % (start, end - 1, "; ".join(names)))
    else:
        out.append("No changed bytes fall in the runs regular packs write "
                   "into.")
    out.append("")

    out.append("## Limits")
    out.append("")
    out.append("- Quick and manual saves are not image-scoped (only campaign "
               "saves are). After switching cartridge packs, load only saves "
               "made under the same cartridge.")
    out.append("- The retail password bridge sees a different image hash and "
               "disables itself; that is non-fatal.")
    out.append("- A pack carries the patch, not the cartridge: the user's own "
               "verified retail ROM is required and stays theirs.")
    out.append("- Changed runs closer than %d bytes are reported as one "
               "region, so a small edit next to a table shares its category."
               % MERGE_GAP)
    out.append("")
    return "\n".join(out)


def _count_records(patch):
    if not patch.startswith(b"PATCH") or not patch.endswith(b"EOF"):
        return 0
    count = 0
    pos = 5
    end = len(patch) - 3
    while pos < end:
        if pos + 5 > end:
            return count
        size = int.from_bytes(patch[pos + 3:pos + 5], "big")
        pos += 5
        if size:
            pos += size
        else:
            pos += 3
        count += 1
    return count


# --- writing the pack ------------------------------------------------------

def slugify(name):
    return re.sub(r"[^a-z0-9]+", "_", name.strip().lower()).strip("_")


def convert(*, rom=None, ips=None, name=None, author=None, version=None,
            out=None, base=None):
    """Write `<out>.json`, `<out>/<slug>.ips` and `<out>/REPORT.md`.

    Returns the pack JSON path. Nothing is written if any check fails.
    """
    if (rom is None) == (ips is None):
        raise ConversionError("give exactly one of --rom or --ips")
    if not name or not name.strip():
        raise ConversionError("--name is required")
    slug = slugify(name)
    if not slug:
        raise ConversionError(
            "--name %r has no letters or digits to build a file name from"
            % name)

    base_path, base_data = load_base(base)

    if rom is not None:
        try:
            raw = Path(rom).read_bytes()
        except OSError as exc:
            raise ConversionError("cannot read %s: %s" % (rom, exc))
        image, headered = normalize_image(raw, str(rom))
        input_label = Path(rom).name
        if headered:
            input_label += " (512-byte copier header dropped)"
    else:
        try:
            raw = Path(ips).read_bytes()
        except OSError as exc:
            raise ConversionError("cannot read %s: %s" % (ips, exc))
        image = apply_ips(base_data, raw)
        headered = False
        input_label = Path(ips).name

    validate_image(image, input_label)

    patch = make_ips(base_data, image)
    if apply_ips(base_data, patch) != image:
        raise ConversionError(
            "internal error: the IPS produced does not round-trip to the "
            "hack; nothing was written")

    out_dir = Path(out) if out is not None else Path("mods") / slug
    pack_json = out_dir.parent / (out_dir.name + ".json")
    ips_name = out_dir.name + ".ips"

    deny_pcs = load_deny_pcs()
    report = build_report(
        name=name.strip(), input_label=input_label, ips_name=ips_name,
        base_path=base_path, base=base_data, image=image, patch=patch,
        deny_path=DENY_LIST, deny_pcs=deny_pcs, headered=headered)

    payload = {
        "name": name.strip(),
        "cartridge": {
            "patch": "%s/%s" % (out_dir.name, ips_name),
            "sha256": hashlib.sha256(image).hexdigest(),
        },
    }
    if author:
        payload["author"] = author
    if version:
        payload["version"] = version

    try:
        out_dir.mkdir(parents=True, exist_ok=True)
        pack_json.parent.mkdir(parents=True, exist_ok=True)
        (out_dir / ips_name).write_bytes(patch)
        (out_dir / "REPORT.md").write_text(report, encoding="utf-8")
        pack_json.write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8")
    except OSError as exc:
        raise ConversionError("cannot write the pack: %s" % exc)
    return pack_json


def build_parser():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rom", metavar="HACK",
                        help="a complete hack image (.sfc; a 512-byte copier "
                             "header is dropped)")
    parser.add_argument("--ips", metavar="PATCH",
                        help="an IPS patch to apply to the retail base")
    parser.add_argument("--name", required=True,
                        help="pack name shown in the menu")
    parser.add_argument("--author", help="pack author")
    parser.add_argument("--version", help="pack version")
    parser.add_argument("--out", metavar="DIR",
                        help="pack stem (default: mods/<slug>); writes "
                             "DIR.json, DIR/<name>.ips and DIR/REPORT.md")
    parser.add_argument("--base", metavar="ROM",
                        help="retail cartridge to diff against (default: "
                             "%r in the current directory)" % RETAIL_NAME)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        pack_json = convert(
            rom=args.rom, ips=args.ips, name=args.name, author=args.author,
            version=args.version, out=args.out, base=args.base)
    except ConversionError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1
    print("wrote %s" % pack_json)
    print("      %s" % Path(pack_json).with_suffix(""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
