"""Work out which tables a routine indexes by team, and with what stride.

Following the register beats pattern-matching but it is still a guess.
$82:FAB3 reads exactly like a per-team byte table here and is not one -
relocating it stops the select screen drawing, which none of the real ones
do. Check anything this reports by building it and looking.

A static search finds the loads but not what is in X, and X is the whole
question: the same `LDA $82F9B3,X` is a per-team table if X holds the team and
a shared pool if X holds a byte fetched from one. So follow the register.

Kinds tracked: "team2" (the team doubled, as the cartridge usually holds it),
"team" (halved by an LSR), "pool" (a byte fetched from a per-team table), and
None for anything else.
"""
import io
import sys

if len(sys.argv) < 2:
    sys.exit("usage: team_index_classify.py <cartridge.sfc> [lo hi]")
ROM = io.open(sys.argv[1], "rb").read()

# Instruction lengths for the opcodes that appear here; enough to walk
# straight-line code without a full disassembler.
LEN = {0xA3: 2, 0xAD: 3, 0xAE: 3, 0xAC: 3, 0xA9: 3, 0xA5: 2, 0xA6: 2,
       0x85: 2, 0x86: 2, 0x84: 2, 0x8D: 3, 0x8E: 3, 0x8C: 3, 0x64: 2,
       0x29: 3, 0x09: 3, 0x69: 3, 0xC9: 3, 0x89: 3, 0xE9: 3,
       0x4A: 1, 0x0A: 1, 0xAA: 1, 0xA8: 1, 0x8A: 1, 0x98: 1, 0x9B: 1,
       0xBB: 1, 0xDA: 1, 0xFA: 1, 0x5A: 1, 0x7A: 1, 0x48: 1, 0x68: 1,
       0x18: 1, 0x38: 1, 0xEB: 1, 0x1A: 1, 0x3A: 1, 0xE8: 1, 0xC8: 1,
       0xBF: 4, 0x9F: 4, 0xAF: 4, 0x8F: 4,
       0xBD: 3, 0x9D: 3, 0xB9: 3, 0x99: 3, 0xBE: 3, 0xBC: 3,
       0xB5: 2, 0x95: 2, 0x20: 3, 0x22: 4, 0x5B: 1, 0x2B: 1, 0x0B: 1,
       0xC2: 2, 0xE2: 2, 0x54: 3, 0x60: 1, 0x6B: 1, 0x4C: 3, 0x5C: 4,
       0x80: 2, 0x90: 2, 0xB0: 2, 0xD0: 2, 0xF0: 2, 0x10: 2, 0x30: 2}

INDEXED_X = {0xBF, 0xBD, 0xBC}
INDEXED_Y = {0xB9, 0xBE}


def walk(lo, hi):
    """Run over one stretch of straight-line code, reporting each table read."""
    out = []
    a = x = y = None
    stack = []
    off = lo
    while off < hi:
        op = ROM[off]
        size = LEN.get(op)
        if size is None:                      # something unmodelled: give up
            a = x = y = None
            off += 1
            continue

        if op == 0xA3:                        # LDA sr,S - the team, doubled
            a = "team2"
        elif op in (0xAD, 0xAE, 0xAC):
            addr = ROM[off + 1] | (ROM[off + 2] << 8)
            kind = "team2" if addr in (0x0DA0, 0x0EA0, 0x1526) else None
            if op == 0xAD:
                a = kind
            elif op == 0xAE:
                x = kind
            else:
                y = kind
        elif op == 0x4A:
            a = "team" if a == "team2" else None
        elif op == 0x0A:
            a = "team2" if a == "team" else None
        elif op == 0xAA:
            x = a
        elif op == 0xA8:
            y = a
        elif op == 0x9B:
            y = x
        elif op == 0xBB:
            x = y
        elif op == 0x8A:
            a = x
        elif op == 0x98:
            a = y
        elif op == 0xDA:
            stack.append(x)
        elif op == 0xFA:
            x = stack.pop() if stack else None
        elif op == 0x29:
            a = "pool" if a == "loaded" else a
        elif op in (0xBF, 0xBD, 0xB9, 0xBE, 0xBC):
            reg = x if op in INDEXED_X else y
            addr = ROM[off + 1] | (ROM[off + 2] << 8)
            bank = ROM[off + 3] if size == 4 else None
            out.append((off + 1, addr, bank, reg))
            a = "loaded"
        elif op in (0x60, 0x6B, 0x4C, 0x5C):
            a = x = y = None
            stack = []
        elif op in (0x20, 0x22):
            a = None                           # a call clobbers A, keeps X/Y
        elif op in (0x80, 0x90, 0xB0, 0xD0, 0xF0, 0x10, 0x30):
            pass                               # a branch: keep going
        else:
            if op in (0xA9, 0xA5, 0xB5):
                a = None
        off += size
    return out


def main():
    rows = {}
    lo = int(sys.argv[2], 0) if len(sys.argv) > 3 else 0x2A800
    hi = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x2B300
    for site, addr, bank, reg in walk(lo, hi):
        if addr < 0x8000:
            continue
        rows.setdefault((bank, addr), set()).add(reg)
    moved = {0x827A, 0x82D0, 0x8326, 0x837C, 0x83D2, 0x842E, 0xF59A,
             0xF7BF, 0xF89D, 0xF95F, 0xF960, 0xF9B3, 0xFA5F, 0xF61C,
             0xF600, 0xF608, 0xF8F1, 0xF91B, 0xFA07, 0xFA35,
             0x8138, 0xEF48, 0xE6C1, 0xE730, 0xE7D8, 0xA93E}
    per_team, pools, unknown = [], [], []
    for (bank, addr), kinds in sorted(rows.items(), key=lambda kv: (kv[0][0] or 0, kv[0][1])):
        b = bank if bank is not None else 0x82
        off = (b & 0x7F) * 0x8000 + (addr - 0x8000)
        row = (b, addr, off, addr in moved)
        if kinds & {"team", "team2"}:
            per_team.append((row, "byte" if "team" in kinds else "word"))
        elif kinds & {"pool", "loaded"}:
            pools.append(row)
        else:
            unknown.append(row)

    print("per-team tables (these have to grow):")
    for (b, addr, off, done), stride in per_team:
        print("  $%02X:%04X file %06X  %-4s%s"
              % (b, addr, off, stride, "  (moved)" if done else ""))
    print("\npools - indexed by a value, not the team; leave them alone:")
    for b, addr, off, done in pools:
        print("  $%02X:%04X file %06X%s" % (b, addr, off, "  (MOVED - wrong!)" if done else ""))
    if unknown:
        print("\nindex not determined:")
        for b, addr, off, done in unknown:
            print("  $%02X:%04X file %06X%s" % (b, addr, off, "  (moved)" if done else ""))


if __name__ == "__main__":
    main()
