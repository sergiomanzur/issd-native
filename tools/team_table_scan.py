"""List every table the cartridge indexes by team.

    python tools/team_table_scan.py "International Superstar Soccer Deluxe (USA).sfc"

Anything that wants more than 42 teams has to move all of these, so the first
question is how many there are. Chasing them one crash at a time answers that
very slowly - each round of "add a page, see what breaks" turns up one or two.
Reading the code answers it at once.

The cartridge fetches the team a small number of ways - `$0DA0` and `$0EA0`
hold the two sides doubled, `$1522` and `$1526` hold the one being looked at -
and then indexes something with it. So: find each of those loads, walk forward
a few instructions, and report the indexed load that follows. That is the
table, and the instruction is where it is named.

Two things the output is careful about, because both cost a round when they
were missed:

  - **addressing mode.** `LDA $82F9B3,X` carries its bank; `LDA $F9B3,X` takes
    it from DB. Searching for the long form alone misses readers written the
    short way, and some are.
  - **bank $7E is WRAM, not the cartridge.** Those entries are runtime arrays
    sized for the teams that exist. They cannot be relocated by patching the
    cartridge, and they are why 48 teams is a re-layout rather than a patch.
"""
import io
import sys

# How the cartridge gets hold of the team index.
LOADS = {
    b'\xAD\xA0\x0D': "LDA $0DA0",     b'\xAD\xA0\x0E': "LDA $0EA0",
    b'\xAE\xA0\x0D': "LDX $0DA0",     b'\xAE\xA0\x0E': "LDX $0EA0",
    b'\xAC\xA0\x0D': "LDY $0DA0",     b'\xAC\xA0\x0E': "LDY $0EA0",
    b'\xB9\xA0\x00': "LDA $00A0,Y",   b'\xBD\xA0\x00': "LDA $00A0,X",
    b'\xB5\xA0':     "LDA $A0,X",
    b'\xAD\x26\x15': "LDA $1526",     b'\xAD\x22\x15': "LDA $1522",
}

# The indexed loads that would then read a table.
INDEXED = {
    0xBF: ("long,X", 4), 0xBD: ("abs,X", 3), 0xB9: ("abs,Y", 3),
    0xBE: ("LDX abs,Y", 3), 0xBC: ("LDY abs,X", 3),
}

# What the eighth-group work has already relocated.
KNOWN = {0xDA3F, 0x8138, 0xEF48, 0x827A, 0x82D0, 0xF59A, 0xF7BF, 0xF89D,
         0xF95F, 0xF9B3, 0xFA5F, 0xF61C, 0x842E}

REACH = 18          # how far past the load to look for the indexed read


def scan(rom):
    found = {}
    for pat, how in LOADS.items():
        i = 0
        while True:
            i = rom.find(pat, i)
            if i < 0:
                break
            j = i + len(pat)
            end = min(j + REACH, len(rom) - 4)
            while j < end:
                op = rom[j]
                if op in INDEXED:
                    kind, size = INDEXED[op]
                    addr = rom[j + 1] | (rom[j + 2] << 8)
                    bank = rom[j + 3] if size == 4 else -1
                    if addr >= 0x8000:
                        found.setdefault((addr, bank), []).append((j + 1, how, kind))
                    break
                j += 1
            i += 1
    return found


def main(path):
    rom = io.open(path, "rb").read()
    found = scan(rom)

    wram, cart = [], []
    for (addr, bank), uses in found.items():
        (wram if bank == 0x7E else cart).append((addr, bank, uses))

    print("%d tables are indexed by the team.\n" % len(found))
    print("In the cartridge - these can be relocated and extended:")
    for addr, bank, uses in sorted(cart, key=lambda e: (e[1], e[0])):
        b = bank if bank >= 0 else 0x82        # absolute: DB, and it is $82
        off = (b & 0x7F) * 0x8000 + (addr - 0x8000)
        note = "  (already done)" if addr in KNOWN else ""
        print("  $%02X:%04X  file %06X  %2d site(s)%s" % (b, addr, off, len(uses), note))
        for site, how, kind in uses:
            print("        %06X  %s then %s" % (site, how, kind))

    if wram:
        print("\nIn WRAM - these are runtime arrays, sized for the teams that")
        print("exist, and no cartridge patch reaches them:")
        for addr, bank, uses in sorted(wram):
            print("  $7E:%04X  %d site(s): %s" % (
                addr, len(uses), ", ".join("%06X" % s for s, _, _ in uses)))

    done = sum(1 for a, b, _ in cart if a in KNOWN)
    print("\n%d of %d cartridge tables relocated so far; %d in WRAM untouched."
          % (done, len(cart), len(wram)))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__.strip().splitlines()[2].strip())
    main(sys.argv[1])
