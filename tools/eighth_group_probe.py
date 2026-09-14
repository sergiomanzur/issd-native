"""Build a cartridge with an eighth group of teams.

    python tools/eighth_group_probe.py            all six new cells
    python tools/eighth_group_probe.py 42         every new cell set to team 42

tools/team_table_scan.py lists what this has to cover. Nothing ships this; see
"An eighth group" in docs/REVERSE_ENGINEERING.md for where it gets to.

What it does:

  - raises the group count so the select screen pages through eight;
  - relocates every table the cartridge indexes by team, extends it to 48
    entries, and re-points every instruction that names it;
  - replaces the squad loader's single compare with a range check, so 36-41
    stay all-star sides and 42-47 read rosters;
  - gives the six new slots rosters copied from the cartridge's first squad.

Two things here are less obvious than they look.

**Five of the tables are one array.** $82:827A, 82D0, 8326, 837C and 83D2 are
86 bytes apart - 43 entries each - and the code walks from one to the next by
adding 86 to the index:

    TXA / CLC / ADC #$0056 / TAX / LDA $82827A,X

Relocating them to scattered addresses breaks that even though every named
reference is right. They move as one block at the new 96-byte stride, and the
two `ADC #$0056` sites become `ADC #$0060`.

**Addressing mode decides where a table may live.** A long reference carries
its bank, so those can go anywhere there is room - which matters, because bank
$82 has nowhere near enough. An absolute reference takes its bank from DB, so
$82:A93E has to stay where DB points.
"""
import io
import sys

SRC = r"International Superstar Soccer Deluxe (USA).sfc"
TEAMS, SLOTS = 42, 48
STRIDE = SLOTS * 2                 # 96 bytes per relocated word table

# --- the five-table block, which has to stay contiguous ---------------------
# name, where the cartridge keeps it, long reference sites, absolute ones
KIT_BLOCK = [
    ("827A", 0x01027A, [0x123C67, 0x05CFD1, 0x05D029], [0x123B53, 0x123C05]),
    ("82D0", 0x0102D0, [0x123C6F], []),
    ("8326", 0x010326, [0x05D06F, 0x05D089], []),
    ("837C", 0x01037C, [0x05CFEB, 0x05D043], []),
    ("83D2", 0x0103D2, [0x05CFFC, 0x05D054], []),
]
KIT_BLOCK_NEW = 0x017BA0           # bank $82, after the stadium pitch table
KIT_STEP_SITES = [0x05CFCD, 0x05D025]      # the ADC #$0056 operands

# --- reached by a long load, so they can live in any bank with room ---------
WORD_TABLES = {
    "roster": (0x038138, 43, 0x03E4CC, 0x87, [0x004F3D, 0x004F60]),
    "form":   (0x05EF48, 43, 0x05FDD0, 0x8B,
               [0x02AA89, 0x02B161, 0x05DB35, 0x05DB47]),
    "E6C1":   (0x00E6C1, 42, 0x00F9E4, 0x81, [0x121154, 0x121170]),
    "E730":   (0x00E730, 42, 0x00FA44, 0x81,
               [0x02A92D, 0x02A942, 0x1211DF, 0x1211F4]),
    "E7D8":   (0x00E7D8, 42, 0x00FAA4, 0x81,
               [0x02A8FF, 0x02A90E, 0x059DE7, 0x059E1A, 0x12120F, 0x12121E]),
    "F59A":   (0x01759A, 42, 0x00FB04, 0x81,
               [0x01BA5C, 0x01BA87, 0x02A996, 0x02A9CC, 0x02C755, 0x02D157,
                0x02D6B0, 0x02E9A0, 0x0364A1, 0x05668F, 0x0566B2, 0x0566D5,
                0x0566F8, 0x05671B, 0x05673E, 0x059909, 0x059E93, 0x122047,
                0x1244E1]),
    "F7BF":   (0x0177BF, 42, 0x00FB64, 0x81, [0x02A9A7, 0x056618]),
    "F89D":   (0x01789D, 42, 0x00FBC4, 0x81, [0x02AFC4]),
    "F95F":   (0x01795F, 42, 0x00FC24, 0x81, [0x02AFD2]),
    "F9B3":   (0x0179B3, 42, 0x00FC84, 0x81, [0x02AFCB]),
    "FA5F":   (0x017A5F, 42, 0x00FCE4, 0x81, [0x02AFBD]),
    "F61C":   (0x01761C, 42, 0x00FD44, 0x81, [0x02B115]),
    # Sorted per-team from pool by following X through the routine rather
    # than by guessing - see the classifier described in the notes. Moving a
    # pool would be worse than leaving it: its indices run past 42.
    "F600":   (0x017600, 42, 0x00FDA4, 0x81, [0x02B0B4]),
    "F608":   (0x017608, 42, 0x00FE04, 0x81, [0x02B0A0]),
}
# $82:F960 is not a table: it is the second byte of each $82:F95F record,
# read with the same index. It follows F95F wherever F95F goes.
ALIASES = {"F960": ("F95F", 1, [0x02AFE2])}

# --- reached absolutely, so the bank comes from DB and cannot change --------
ABS_TABLES = {"A93E": (0x01293E, 42, 0x017DB0, 0x82, [0x01C9AC])}

# --- indexed by the team itself rather than doubled -------------------------
BYTE_TABLES = {
    "842E": (0x01042E, 42, 0x017D80, 0x82, [0x0C7A65, 0x0C7A73]),
    "F8F1": (0x0178F1, 42, 0x00FE64, 0x81, [0x02B134]),
    "F91B": (0x01791B, 42, 0x00FE94, 0x81, [0x02B001]),
    "FA07": (0x017A07, 42, 0x00FEC4, 0x81, [0x02B0BE, 0x02B0DF]),
    "FA35": (0x017A35, 42, 0x00FEF4, 0x81, [0x02B083]),
}
BYTE_ABS_REFS = {"842E": [0x123B61, 0x123B6E]}

# $82:FAB3 looks exactly like the others - LDA $01,S / LSR / TAX and then a
# byte load - and is not one of them. Relocating it stops the eighth page
# drawing at all, which none of the others do, so whatever that index counts
# it is not the team. Left alone. Following the register is a better guess
# than pattern-matching but it is still a guess; bisecting caught this one.

# Which team is in which cell of the grid. Not in team order: cell 0 is
# England, which is team 2.
CELL, CELLS, CELL_NEW = 0x00DA3F, 42, 0x00F9B3
CELL_REFS = (0x02AEA2, 0x02AEF3, 0x02AF4B, 0x02AF7E, 0x02AF88)

GROUPS = 0x02A567          # LDX #$0006, the number of groups the screen draws
ROSTERS = 0x03FC40         # free at the end of bank $87: six squads exactly
NAME = 229774              # the cartridge's own first squad

# One compare cannot say "below 36, or 42 and up", but a JSR is the same three
# bytes as a CPX immediate, so the gate becomes a call.
RANGE = 0x007828
RANGE_CODE = bytes((0xE0, 0x48, 0x00,        # CPX #$0048  - 36 doubled
                    0x90, 0x07,              # BCC clear   - a stock team
                    0xE0, 0x54, 0x00,        # CPX #$0054  - 42 doubled
                    0xB0, 0x02,              # BCS clear   - an added team
                    0x38, 0x60,              # SEC / RTS   - an all-star side
                    0x18, 0x60))             # CLC / RTS
GATES = (0x004F2D, 0x004F4F)


def w16(rom, off, addr):
    rom[off] = addr & 0xFF
    rom[off + 1] = addr >> 8


def w24(rom, off, file_off, bank):
    w16(rom, off, 0x8000 + (file_off - (bank & 0x7F) * 0x8000))
    rom[off + 2] = bank


def _extend(rom, base, have, new, entry_bytes):
    """Copy a table and fill the new slots from its first entry."""
    rom[new:new + have * entry_bytes] = rom[base:base + have * entry_bytes]
    for t in range(have, SLOTS):
        at = new + t * entry_bytes
        rom[at:at + entry_bytes] = rom[base:base + entry_bytes]


def build(src=SRC, cells_at=None):
    rom = bytearray(io.open(src, "rb").read())
    rom[GROUPS] = 8

    rom[CELL_NEW:CELL_NEW + CELLS] = rom[CELL:CELL + CELLS]
    for c in range(CELLS, SLOTS):
        rom[CELL_NEW + c] = (cells_at if cells_at is not None else c) * 2
    for r in CELL_REFS:
        assert rom[r - 1] == 0xBD, "cell ref %06X" % r
        w16(rom, r, CELL_NEW)

    for i, (name, base, longs, absolutes) in enumerate(KIT_BLOCK):
        new = KIT_BLOCK_NEW + i * STRIDE
        _extend(rom, base, 43, new, 2)
        for r in longs:
            assert rom[r - 1] == 0xBF, "%s long ref %06X" % (name, r)
            w24(rom, r, new, 0x82)
        for r in absolutes:
            w16(rom, r, 0x8000 + (new - 0x10000))
    for r in KIT_STEP_SITES:
        assert rom[r - 1] == 0x69, "step site %06X" % r
        w16(rom, r, STRIDE)

    for name, (base, have, new, bank, refs) in WORD_TABLES.items():
        _extend(rom, base, have, new, 2)
        for r in refs:
            assert rom[r - 1] == 0xBF, "%s ref %06X" % (name, r)
            w24(rom, r, new, bank)

    for name, (base, have, new, bank, refs) in ABS_TABLES.items():
        _extend(rom, base, have, new, 2)
        for r in refs:
            w16(rom, r, 0x8000 + (new - (bank & 0x7F) * 0x8000))

    for name, (base, have, new, bank, refs) in BYTE_TABLES.items():
        _extend(rom, base, have, new, 1)
        for r in refs:
            assert rom[r - 1] == 0xBF, "%s ref %06X" % (name, r)
            w24(rom, r, new, bank)
        for r in BYTE_ABS_REFS.get(name, []):
            w16(rom, r, 0x8000 + (new - (bank & 0x7F) * 0x8000))

    for name, (of, delta, refs) in ALIASES.items():
        new = WORD_TABLES[of][2] + delta
        for r in refs:
            assert rom[r - 1] == 0xBF, "%s ref %06X" % (name, r)
            w24(rom, r, new, WORD_TABLES[of][3])

    rom[RANGE:RANGE + len(RANGE_CODE)] = RANGE_CODE
    call = 0x8000 + RANGE
    for g in GATES:
        assert rom[g] == 0xE0, "gate %06X" % g
        rom[g] = 0x20
        w16(rom, g + 1, call)

    rnew = WORD_TABLES["roster"][2]
    for i in range(6):
        block = ROSTERS + i * 160
        rom[block:block + 160] = rom[NAME:NAME + 160]
        w16(rom, rnew + (TEAMS + i) * 2, 0x8000 + (block - 7 * 0x8000))
    return bytes(rom)


if __name__ == "__main__":
    at = int(sys.argv[1]) if len(sys.argv) > 1 else None
    out = "eighth_group_%s.sfc" % (at if at is not None else "all")
    io.open(out, "wb").write(build(cells_at=at))
    print(out)
