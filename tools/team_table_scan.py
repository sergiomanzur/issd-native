"""List every table the cartridge indexes by team.

    python tools/team_table_scan.py "International Superstar Soccer Deluxe (USA).sfc"

Anything that wants more than 42 teams has to move all of these, so the first
question is how many there are. Chasing them one crash at a time answers that
very slowly - each round of "add a page, see what breaks" turns up one or two.
Reading the code answers it at once.

The cartridge fetches the team a small number of ways - `$0DA0` and `$0EA0`
hold the two sides doubled, `$1522` and `$1526` hold the one being looked at -
and then indexes something with it. So: find each of those loads, follow the
index into a register, and report the indexed load that uses it.

Three rules keep the answer honest, and each of them came from a false
positive that wasted a round:

  - **the index has to reach the register.** A team loaded into A only counts
    once a TAX or TAY moves it; a team loaded straight into X or Y counts at
    once. Without this, any indexed load a dozen bytes later looks like a hit.
  - **stop at control flow.** `LDA $0EA0 / ... / RTS` followed by an unrelated
    routine is not a team-indexed read, and two of the four WRAM "tables" this
    first reported were exactly that.
  - **addressing mode.** `LDA $82F9B3,X` carries its bank; `LDA $F9B3,X` takes
    it from DB. Searching for the long form alone misses readers written the
    short way, and some are.

Bank $7E is WRAM rather than the cartridge. Those are runtime arrays sized for
the teams that exist, so no cartridge patch reaches them; they are listed
separately because they are the part that decides whether more teams is a
patch or a re-layout.
"""
import io
import sys

# How the cartridge gets hold of the team, and where it lands.
LOADS = {
    b'\xAD\xA0\x0D': ("LDA $0DA0", "A"), b'\xAD\xA0\x0E': ("LDA $0EA0", "A"),
    b'\xAE\xA0\x0D': ("LDX $0DA0", "X"), b'\xAE\xA0\x0E': ("LDX $0EA0", "X"),
    b'\xAC\xA0\x0D': ("LDY $0DA0", "Y"), b'\xAC\xA0\x0E': ("LDY $0EA0", "Y"),
    b'\xB9\xA0\x00': ("LDA $00A0,Y", "A"), b'\xBD\xA0\x00': ("LDA $00A0,X", "A"),
    b'\xB5\xA0':     ("LDA $A0,X", "A"),
    b'\xAD\x26\x15': ("LDA $1526", "A"),
}
# $7E1522 looks like a team and is not one. Eight readers index tables with
# it, three of them four bytes apart - which no forty-two entry table can be
# - and it reads 0 for every team on the select screen. Whatever it counts,
# it is not the side.

# The indexed loads that would then read a table, and which register they use.
INDEXED = {
    0xBF: ("long,X", 4, "X"), 0xBD: ("abs,X", 3, "X"), 0xB9: ("abs,Y", 3, "Y"),
    0xBE: ("LDX abs,Y", 3, "Y"), 0xBC: ("LDY abs,X", 3, "X"),
}

TRANSFER = {0xAA: "X", 0xA8: "Y"}          # TAX, TAY

# Anything that ends the run of straight-line code.
CONTROL = {0x60, 0x6B, 0x40, 0x4C, 0x5C, 0x6C, 0x7C, 0xDC, 0x80, 0x82,
           0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0, 0x20, 0x22, 0xFC}

# What the eighth-group work has already relocated.
KNOWN = {0xDA3F, 0x8138, 0xEF48, 0x827A, 0x82D0, 0xF59A, 0xF7BF, 0xF89D,
         0xF95F, 0xF9B3, 0xFA5F, 0xF61C, 0x842E}

REACH = 14          # how far past the load to follow straight-line code


def scan(rom):
    found = {}
    for pat, (how, lands_in) in LOADS.items():
        i = 0
        while True:
            i = rom.find(pat, i)
            if i < 0:
                break
            holds = {lands_in}          # registers now holding the team
            j = i + len(pat)
            end = min(j + REACH, len(rom) - 4)
            while j < end:
                op = rom[j]
                if op in TRANSFER and "A" in holds:
                    holds.add(TRANSFER[op])
                    j += 1
                    continue
                if op in INDEXED:
                    kind, size, reg = INDEXED[op]
                    if reg in holds:
                        addr = rom[j + 1] | (rom[j + 2] << 8)
                        bank = rom[j + 3] if size == 4 else -1
                        if addr >= 0x8000:
                            found.setdefault((addr, bank), set()).add(
                                (j + 1, how, kind))
                    break
                if op in CONTROL:
                    break
                j += 1
            i += 1
    return found


def main(path):
    rom = io.open(path, "rb").read()
    found = scan(rom)

    wram, cart = [], []
    for (addr, bank), uses in found.items():
        (wram if bank == 0x7E else cart).append((addr, bank, sorted(uses)))

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
        print("\nIn WRAM - runtime arrays, which no cartridge patch reaches:")
        for addr, bank, uses in sorted(wram):
            print("  $7E:%04X  %d site(s): %s" % (
                addr, len(uses), ", ".join("%06X" % s for s, _, _ in uses)))

    done = sum(1 for a, _, _ in cart if a in KNOWN)
    print("\n%d of %d cartridge tables relocated so far; %d in WRAM."
          % (done, len(cart), len(wram)))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: team_table_scan.py <cartridge.sfc>")
    main(sys.argv[1])
