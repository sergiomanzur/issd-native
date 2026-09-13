"""Build a cartridge with an eighth group of teams, for experimenting.

    python tools/eighth_group_probe.py            all six new cells
    python tools/eighth_group_probe.py 42         every new cell set to team 42

This is not part of the mod loader and nothing ships it. It is the state the
eighth-group investigation reached, kept runnable so the next attempt starts
here instead of at the beginning. See "An eighth group" in
docs/REVERSE_ENGINEERING.md for what works and what does not.

What it does:

  - raises the group count so the select screen pages through eight;
  - relocates every table the cartridge indexes by team into free space,
    extends it to 48 entries, and re-points every instruction that names it;
  - replaces the squad loader's single compare with a range check, so 36-41
    stay all-star sides and 42-47 read rosters;
  - gives the six new slots rosters copied from the cartridge's first squad.

Where it gets to: the eighth page renders, and all six of the new indices are
stable on it - the grid, the formation panel and the statistics all draw.
Confirming a team from that page still jumps into WRAM, so the match-load path
has further per-team tables that have not been found yet. Find them the way
these were: pick a team that works and one that does not, capture a cartridge
read map for each, and look for offsets that differ by (teamA - teamB) * 2.
The instruction that reads a table is then a search for its address bytes -
in both the long form (BF lo hi 82) and the absolute one (BD lo hi with DB
set), which is the trap that cost a round here.
"""
import io
import sys

SRC = r"International Superstar Soccer Deluxe (USA).sfc"
TEAMS, SLOTS = 42, 48

# name: (where the cartridge keeps it, entries it has, where it goes, bank,
#        the long loads that name it)
WORD_TABLES = {
    "roster":  (0x038138, 43, 0x03E4CC, 0x87, [0x004F3D, 0x004F60]),
    "form":    (0x05EF48, 43, 0x05FDD0, 0x8B, [0x02AA89, 0x02B161]),
    "kit_h":   (0x01027A, 43, 0x017BA0, 0x82, [0x123C67]),
    "kit_a":   (0x0102D0, 43, 0x017C00, 0x82, [0x123C6F]),
    "F59A":    (0x01759A, 42, 0x017C60, 0x82,
                [0x01BA5C, 0x01BA87, 0x02A996, 0x02A9CC, 0x02C755, 0x02D157,
                 0x02D6B0, 0x02E9A0, 0x0364A1, 0x05668F, 0x0566B2, 0x0566D5,
                 0x0566F8, 0x05671B, 0x05673E, 0x059909, 0x059E93, 0x122047,
                 0x1244E1]),
    "F7BF":    (0x0177BF, 42, 0x017CC0, 0x82, [0x02A9A7, 0x056618]),
    "F89D":    (0x01789D, 42, 0x017D20, 0x82, [0x02AFC4]),
    "F95F":    (0x01795F, 42, 0x017D80, 0x82, [0x02AFD2]),
    "F9B3":    (0x0179B3, 42, 0x017E40, 0x82, [0x02AFCB]),
    "FA5F":    (0x017A5F, 42, 0x017EA0, 0x82, [0x02AFBD]),
    "F61C":    (0x01761C, 42, 0x017F00, 0x82, [0x02B115]),
}
# The kit table is also reached in absolute form, where the bank comes from DB
# rather than from the instruction - two more sites, sixteen bits each.
ABS_REFS = {"kit_h": [0x123B53, 0x123C05]}

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


def w24(rom, off, file_off, bank):
    addr = 0x8000 + (file_off - (bank & 0x7F) * 0x8000)
    rom[off] = addr & 0xFF
    rom[off + 1] = addr >> 8
    rom[off + 2] = bank


def build(src=SRC, cells_at=None):
    rom = bytearray(io.open(src, "rb").read())
    rom[GROUPS] = 8

    rom[CELL_NEW:CELL_NEW + CELLS] = rom[CELL:CELL + CELLS]
    for c in range(CELLS, SLOTS):
        rom[CELL_NEW + c] = (cells_at if cells_at is not None else c) * 2
    for r in CELL_REFS:
        assert rom[r - 1] == 0xBD, "cell ref %06X" % r
        rom[r] = CELL_NEW & 0xFF
        rom[r + 1] = CELL_NEW >> 8

    for name, (base, have, new, bank, refs) in WORD_TABLES.items():
        rom[new:new + have * 2] = rom[base:base + have * 2]
        for t in range(have, SLOTS):
            src_off = base + (t - TEAMS) * 2 if t >= TEAMS else base
            rom[new + t * 2:new + t * 2 + 2] = rom[src_off:src_off + 2]
        for r in refs:
            assert rom[r - 1] == 0xBF, "%s ref %06X" % (name, r)
            w24(rom, r, new, bank)
        for r in ABS_REFS.get(name, []):
            addr = 0x8000 + (new - (bank & 0x7F) * 0x8000)
            rom[r] = addr & 0xFF
            rom[r + 1] = addr >> 8

    rom[RANGE:RANGE + len(RANGE_CODE)] = RANGE_CODE
    call = 0x8000 + RANGE
    for g in GATES:
        assert rom[g] == 0xE0, "gate %06X" % g
        rom[g] = 0x20
        rom[g + 1] = call & 0xFF
        rom[g + 2] = call >> 8

    rnew = WORD_TABLES["roster"][2]
    for i in range(6):
        block = ROSTERS + i * 160
        rom[block:block + 160] = rom[NAME:NAME + 160]
        addr = 0x8000 + (block - 7 * 0x8000)
        rom[rnew + (TEAMS + i) * 2] = addr & 0xFF
        rom[rnew + (TEAMS + i) * 2 + 1] = addr >> 8
    return bytes(rom)


if __name__ == "__main__":
    at = int(sys.argv[1]) if len(sys.argv) > 1 else None
    out = "eighth_group_%s.sfc" % (at if at is not None else "all")
    io.open(out, "wb").write(build(cells_at=at))
    print(out)
