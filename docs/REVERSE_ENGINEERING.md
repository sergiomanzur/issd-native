# Reverse Engineering Overview & Methodology

This document tracks reverse-engineering findings, tooling, memory structures, and game routines for **ISSD Native** (International Superstar Soccer Deluxe - SNES USA).

## Primary References
1. **Disassembly:** [Yoshifanatic1/International-Superstar-Soccer-Deluxe-SNES-Disassembly](https://github.com/Yoshifanatic1/International-Superstar-Soccer-Deluxe-SNES-Disassembly)
2. **ROM Framework:** [Yoshifanatic1/SNES-ROM-Framework](https://github.com/Yoshifanatic1/SNES-ROM-Framework)
3. **Web Editor / Roster Specs:** [EstebanFuentealba/ISSD-SNES-ROM-Web-Editor](https://github.com/EstebanFuentealba/ISSD-SNES-ROM-Web-Editor)
4. **SNESRecomp Framework:** [mstan/snesrecomp](https://github.com/mstan/snesrecomp)

## ROM Memory Map & Layout
- **ROM Title:** ISS DELUXE USA
- **ROM Type:** LoROM / FastROM (,  banks)
- **ROM Size:** 2,097,152 bytes (16 Mbits / 2MB)
- **Clean USA MD5:** 345ddedcd63412b9373dabb67c11fc05
- **SNES Native Vectors:**
  - Reset: $80:FFFC
  - NMI: $80:FFEA
  - IRQ/BRK: $80:FFEE
  - COP: $80:FFE4
- **WRAM Layout:**
  - Direct Page / Stack: $0000 -  (Mirrored at $7E:0000 - )
  - Extended WRAM: $7E:2000 - 

## Symbol Classification System
Symbols in ROUTINE_MAP.md and RAM_MAP.md are tagged with confidence levels:
- [CONFIRMED]: Verified via disassembly, ROM trace, or reference emulator.
- [STRONGLY INFERRED]: Derived from context, tables, or web editor structures.
- [TENTATIVE]: Guessed or partially understood.
- [UNKNOWN]: Unidentified address.

## Gameplay freeze control flow (confirmed, September 2026)

Two independent AOT/LLE boundary errors could leave an NMI unfinished with
`S=$0195`, `PB=$87`, and the game's NMI-busy word set. The visible result was a
black frame followed by a stopped game loop.

First, an interpreter-owned NMI subroutine could reach a generated tail whose
exact variant was unavailable. `interp_tier_dispatch_tail` treated every active
interrupt context as permission to consume the complete NMI in a nested
interpreter. The generated caller then resumed after its guest stack had already
unwound. Interpreter-bounce ownership now takes priority; only an actual
interrupt tail with no generated return frame enters the interrupt dispatcher.

Second, generated "tail call past end" wrappers inherited their caller's guest
return context but forwarded `RECOMP_RETURN_SKIP_N` unchanged. The runtime
ancestor resolver counts real host frames, and each generated tail wrapper is
one such frame. The wrapper now consumes one nonzero skip level, just like an
ordinary generated call site. This was reproduced at `$84:C85D`, whose tail to
`$84:E60C` formerly caused `$84:8000` to return early, leave `D=$1700`, and
misdirect the later `JMP ($0000)` in `$83:8737`.

`tests/runtime/test_interrupt_tail.py` covers the interpreter-owned NMI case.
`tests/runtime/test_tail_ancestor_unwind.py` compiles the real emitted tail
statement and checks normal and non-local returns through one to three tail
layers. `tests/test_gameplay_stability.py` exercises the ROM-backed match path
and requires every final frame to return to `S=$01AF` with NMI-busy clear.
The host loop also bounds simulation-only catch-up to four 60 Hz ticks between
presentations. Sustained slow emulation therefore continues to show the newest
completed frame instead of starving `SDL_RenderPresent` indefinitely.

## Widescreen pitch and object rendering (confirmed, September 2026)

`ISSDNative/issd_widescreen.c` prepares additional view columns for the original
PPU. It does not scale a 256-pixel scene or change the camera/game simulation.
The preparation is a transaction: save the pitch VRAM and OAM, prepare the
additional geometry, scan out, then restore the game-visible memories before
the next simulation frame. Classic mode takes none of these branches.

### Pitch streaming

The streamers `$8B85E3` (rows) and `$8B86E9` (columns) maintain a 512x512 tilemap
ring with roughly 32 pixels of lookahead. Increasing PPU width alone exposes
stale ring entries. Both layers already have complete decompressed world data:

| Data | BG1 | BG2 |
| --- | --- | --- |
| World metatile map, byte IDs | `$7FD000` | `$7FE000` |
| 32x32 metatile definitions, sixteen 16-bit tiles | `$7F8000` | `$7FA000` |
| PPU tilemap word base | `$0000` | `$1000` |

`$8B87E7` converts world pixels to a metatile map byte using
`(y >> 8) * stride + ((y & 0xe0) >> 2) + (x >> 8) * 64 + ((x & 255) >> 5)`.
The stride is `$7FFFCC`, selected by the stadium table `$81EE71`.
The definition offset is `id * 32 + ((y & 31) >> 3) * 8 + ((x & 31) >> 3) * 2`.
`$8B8838` selects the corresponding wrapped PPU nametable quadrant.

The PPU retains only ten scroll bits. Recovering high world-coordinate bits
from `$13A0/$13C0` and `$13B0/$13D0` is essential; otherwise the result can look
like plausible grass while showing the wrong part of the stadium. In a real
frame-1200 snapshot, this mapping matched all 896 tested native tiles on each
of BG1 and BG2 (1,792/1,792). Only tiles wholly outside the native viewport are
written. World edges clamp the visible margin to initialized map data.

`$80846C` defines mode 3 as demo and mode 6 as menus/game. Mode 1 is title, not
in-game. Pitch detection combines these modes with `$50`, the BG1/BG2 tilemap
layout, 8-pixel BG tile size, and the initialized stadium stride. Mode 9 at the
PPU means mode 1 with the BG3 priority bit set. BG3 carries scores, clock,
mini-map, player names and pause UI and stays centered across the entire frame.
The modern PPU renderer must be selected for its layer policies to apply.

### Objects

There are two clipping stages. `$83CFE5` marks player records outside
`[-32,288)` via their `$1E` field; `$809B04` omits them from the sorted draw list.
The per-part OAM builders in `$809663..$809A98` then clip individual pieces at
`x >= 256` or `x < -16` (some large decoded pieces use `-32`). Changing the PPU
width cannot restore these missing pieces.

The presentation supplement reads the current object poses and reconstructs
only pieces rejected by horizontal clipping, filling parked `F0F0` OAM slots.
It includes player records `$0400..$1A00` and the field auxiliary records used
by `$809B28/$809B4A`, ordered by the same depth field. ROM poses in bank `$88`
store count and four-byte `(dy,dx,tile,attributes)` entries. Decoded poses use
parallel WRAM displacement/tile arrays at `pointer+$2000/$4000/$6000`;
pointers above `$4000` are valid and common. The ball is object `$0400`.
Explicit signed OAM hints distinguish left-margin sprites from parked sprites
and permit deliberately emitted right-margin coordinates above 255.

The offscreen flags remain unchanged because game logic also reads them.
A real frame-1200 snapshot contained a valid six-part player at `$0D00`, x=327,
which the original draw list omitted. This provides a concrete regression case
for whole-player visibility in the additional view.

### Validation and limits

`tests/test_widescreen_native.c` tests ring wrap, high world-coordinate bits,
ROM and decoded whole-object expansion, native/menu gating, world edges, and
VRAM/OAM restoration. `tests/test_widescreen.py` runs real-ROM Original,
16:10, 16:9 and 21:9 captures from isolated working directories and checks
expanded scene content plus byte-identical simulation WRAM.

This is presentation-only geometry expansion. It does not modify camera
movement or offscreen AI. Supplemental objects still share the 128-entry OAM
capacity; scenes exhausting all entries have not been validated. The existing
95-pixel per-side cap remains, so the "21:9" selection renders 446x224 internal
pixels. Stadium/weather combinations and all set-piece transitions still need
broader visual coverage before claiming exhaustive compatibility.

---

## Locating per-team data by diffing cartridge read maps

The per-team formation table resisted every pattern search. Capturing the
formation index the game leaves in `$7E15F6` for thirty of the thirty-six
teams gave a thirty-constraint fingerprint, and sweeping every stride from
1 to 1024 in both directions - matching exact values, and again matching
only the partition of teams into equal groups, so any re-encoding would
still be caught - found nothing anywhere in the 2 MB image. The same search
over a WRAM snapshot found nothing either.

What found it was recording reads rather than searching bytes. A temporary
hook in `RomPtr` - the single function every cartridge access resolves
through - marked each offset a run touched, and six runs were captured that
differed only in which team the team select screen had highlighted.

Two facts fell out immediately:

- Only about 67,000 of 2,097,152 offsets are read at all up to that screen.
- Because the navigation scripts walk through the grid, a run that ends on a
  later cell also performs every earlier cell's reads. Only the two teams at
  the end of each row had reads nobody else made.

Those two teams are three apart in the roster, so any 16-bit table indexed
by team shows up as a pair of reads exactly six bytes apart. Three such
pairs appeared at once: the attribute table (140-byte stride, already
known), and two pointer tables. Following the pointers from `$8B:EF48` gave
36 records of 31 bytes whose first byte reproduced the fingerprint exactly,
including the six teams that had never been captured.

The technique generalises to any per-team or per-entity table: capture read
maps for two runs that differ only in the selection, subtract, and look for
offsets whose spacing matches the index distance.

### Confirming what the record does

Three mutations, each changing one thing:

- Forcing all twenty of a team's position nibbles to the same value left the
  printed formation unchanged - so the label is not derived from the roster,
  which a promising-looking five-out-of-six correlation had suggested.
- Writing each of the sixteen values into the label byte and screenshotting
  the result enumerated the whole printable vocabulary.
- Rewriting only the ten coordinate pairs, leaving label and roles alone,
  changed 620 bytes of WRAM at kickoff and visibly moved the markers on the
  in-match radar - so the pairs drive real positioning, not just the
  formation screen. Shifting every pair's depth by the same amount and
  watching which way the side moved established that negative is upfield.

---

## Raising a fixed count: more stadiums than the cartridge has

"Eight stadiums" turned out to be one 16-bit literal and four packed
tables, none of it load-bearing.

The first thing worth knowing is that **cartridge code is patchable in this
build**. Changing `CMP #$0008` to `#$0009` at 0x121F6D immediately produced
a ninth entry on the stadium select screen, which means the routine runs
interpreted rather than being baked into the recompiled C. That single
experiment is what makes everything below possible.

It also surfaced something already in the data: the ninth name plate reads
**ALL STAR**, and beyond it are country plates (NORWAY and others). The
plate is picked by slot number from a list far longer than eight, which is
why renaming a stadium never changed it.

### Finding the tables

Four instructions index per-stadium data, all found by searching for the
long-addressing byte pattern `lo FA 82` - a reference into $82:FAxx:

| Address | Instruction | Holds |
|---|---|---|
| 0x121F9F | `LDA $82FADD,X` | turf pattern |
| 0x122009 | `LDA $82FAED,X` | pitch length |
| 0x12201C | `LDA $82FAEE,X` | pitch width |
| 0x12203E | `LDA $82FAFD,X` | unidentified, but per stadium |

The pitch table was located first, by searching for the lengths the select
screen prints - 114, 118, 126, 130, 122, 122, 114, 138 - which occur in
that order exactly once in the cartridge.

### Finding a reader that is not in the generated C

The name table had no long reference and did not appear in
`recomp/generated` at all. Logging the interpreter's PC - it keeps one in
`g_interp816_cur_pc` - whenever anything read the table named it at once:
$86:C494, file 0x34494. The code there is

```
LDA $4C ; ASL ASL ASL ; SEC ; SBC $4C   ; index * 7
CLC ; ADC #$CA9B                        ; + the table base
TAY ; LDX #$0128 ; LDA #$0007 ; JSL ... ; print seven bytes
```

so the base is a 16-bit immediate at 0x3448B. That technique - hook the
read, print the interpreter PC - is the general answer to "what reads
this?" when grepping the generated C comes up empty.

### Extending

Bank $82 has 1187 free bytes at $82:FB5D and bank $87 has 1336 at
$87:FAC8, both immediately after the tables that need to grow. Each table
is copied there, extended by repeating the cartridge's own entries, and
its one instruction re-pointed. Every site is verified against the bytes
it should hold first, so a different revision is refused rather than
corrupted.

### What this does not reach

The name plate, as above. And the same approach applied to teams would be
much larger: a team is spread across the roster name and attribute tables,
a formation record behind a pointer table, flags, kit palettes and name
plates, and the roster base is computed through a dispatcher in bank $83
rather than sitting in an instruction as an immediate. The select screen's
six-by-six grid would have to grow as well.
