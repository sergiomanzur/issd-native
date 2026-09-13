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

### The plate, drawn host-side instead

The plate is the one thing a ROM patch cannot reach: it is a pre-rendered
graphic chosen by slot number, and the cartridge only has so many. Since
this is a recompilation rather than an emulator, the host can simply draw
it - the same way the pause menu and the mod notification are drawn.

That needed three measurements:

- **The selector.** Diffing WRAM between two stadium selections gave 19
  differing bytes, of which `$7E154C` held 0 for JAPAN and 2 for SPAIN.
- **The rectangle.** Printing an is-it-grey map of the screen put the plate
  at x 56-127, y 48-63 in native coordinates, with the grey gradient down
  its rows and its lettering in `$1039B5`.
- **Which screen we are on.** This took two attempts. `$7E0076` looked like
  a screen id across one pair of captures and turned out to be a frame
  counter - it read $024F, $02B3, $032B and $03AD on the same screen at
  four different frames. The four background scroll positions at `$7E0018`
  onwards are the real signature: 52, 44, 48, 40 on the stadium screen at
  every frame and for every stadium, and different on team select, which
  shares the same game mode.

The check is worth the care. Team select is the same game mode, so a weaker
test would paint a grey slab over it.

### What this does not reach

The same approach applied to teams runs into the select screen's six-by-six
grid, which would have to grow. The tables themselves turned out to be
reachable - see the next section.

---

## The seventh group of teams, and what is actually behind it

### Unlocking it

The team select screen offers six groups of six. Watching writes to the
group count in WRAM and following the interpreter's PC back reached
$85:A566:

```
  LDX #$0006          ; the default
  LDA $7ED856
  CMP #$0001
  BNE +3
  LDX #$0007          ; ... unless the flag is set
```

Raising the default at file offset **0x2A567** from 6 to 7 is the whole
unlock. A seventh group appears - ALL STAR, EUROSTAR A and B, ASIAN STAR,
AFRICAN STAR, ALL AMERICAN STAR - and its teams are selectable and play.

### They are not six spare squads

The obvious next step - write twenty names at name_base + team * 160 -
changed nothing, and a match as ALL AMERICAN STAR showed a keeper called
da Silva, which is Brazil's. Three measurements settled it.

A read map of that match, restricted to the name table, shows **twenty**
eight-byte reads scattered across teams 30 to 35 and none from a roster of
its own. Counted by slot they are exactly twenty players - six from Brazil,
four from Argentina, three from Columbia and so on: team 36+g is assembled
at kick-off out of the six rosters of group g.

The same map shows six single-word reads at 0x38174 onwards. That is a
**roster pointer table at 0x38138** - 43 sixteen-bit pointers into bank $87,
stride $A0, one per team - and those reads are entries 30 to 35, the group
being drawn from. Its last seven entries all hold the same dead address,
$87:980E, one past the end of the 36 rosters. There is a second, identical
copy at 0x398AE.

Twenty names, no roster of its own, seven dummy pointers: three independent
facts saying the same thing.

### Making them real teams

That is a description of the *default*, not a limit. The squad loader is
`$80:CF2A`, and it is nine instructions:

```
  LDX $0DA0          ; the team, doubled
  CPX #$0048         ; 72 - that is 36 teams
  BCC normal
  TXA / LDY #$D478 / JSL $A49C89     ; assemble one from the group
normal:
  LDA $878138,X      ; the roster pointer table
  TAX / LDA #$009F / LDY #$D478
  MVN $87,$7E        ; 160 bytes of names into WRAM
```

Raise that compare and slot 36 reads a roster like any other team. There
are two copies, one per side of the match, at file offsets 0x4F2E and
0x4F50. Point the table's entry 36 at 160 bytes of free space - bank $87
ends with 1336 spare bytes, exactly six squads' worth - and the slot is a
real team, added rather than replacing anyone.

The gate only ever moves as far as the teams actually added. Raising it
past a slot with no roster behind it would leave that slot reading the
table's dead entry, so the all-star sides above the added ones keep
working.

### Why the cartridge patch did nothing at first

Writing the new compare into the cartridge changed nothing, and the trace
said why: the reads came from `CODE_80CF2A_M0X0`, a **generated C**
function, where the immediate is a baked-in constant:

```c
  uint16 _v2 = 0x48;
```

Cartridge patching only reaches code that runs interpreted. This is the
first time in this project that mattered, and it is worth remembering: an
immediate that lives in a recompiled body is not in the ROM any more.

The fix is the runner's own deny set. Listing the routine in
`recomp/aot_boot_deny.txt` tiers it down to the interpreter, and the
cartridge bytes are authoritative again. Two details cost a run each:
the file is matched on all 24 bits, so the bank-$80 mirror has to be
listed as well as `00CF2A`; and the file is read once, at startup.

It runs twice per match load, so interpreting it costs nothing.

A pack can therefore give these six ratings, a shape, a strip, a plate, a
photograph **and** a squad of their own.

Searching for the pick list as a table found nothing under any encoding
tried: (team, player) pairs either way round, packed indices, sixteen-bit
globals, and every permutation of those. The picker is at $98:FA2D, reached
by a JSL from $85:ADA1; it looks computed rather than tabulated, and was not
chased further because nothing depends on it.

### What the seventh group does have

| | where | teams |
|---|---|---|
| ratings | 0x50000 + team * 140 | 0-41 |
| formation | pointer table at 0x5EF48 | 0-42 |
| kit palette | see below | 0-41 |
| names | pointer table at 0x38138 | **0-35 only** |

ALL STAR's formation record really does read 4-2-4, which is what the select
screen prints for it - a cheap check that the pointer table runs past 36
rather than into rubbish.

---

## Kit palettes: when there is no index table to find

### The table

Diffing the read maps of two matches that differed only in the opponent left
a handful of runs, among them two thirty-two byte reads thirty-four bytes
apart. Thirty-four bytes is seventeen colours, and dumping them showed a
strip: three shirt shades, three shorts shades, two sock shades, skin and
hair, and two constants at the end that every record shares ($0120, $001F).
England's red runs $F6, $BD, $94 - the lit shade, and roughly three quarters
and three fifths of it.

Scanning for those two trailing constants at a 34-byte stride bounds the
table: **84 records from 0x483C0**, two per team for the 42 the screen can
offer.

### The index that does not exist

Which record a team wears is stored nowhere. Searched for and not found: a
byte table, a word table, byte tables at every stride from 1 to 32, a table
of bank-$89 addresses, and the record index scaled by 2 or by 34. The mapping
is not the team order, the group order or the screen order - Brazil,
Argentina, Columbia, Mexico, U.S.A and Uruguay wear records 28, 25, 26, 24,
23 and 27.

### Measuring it instead

Forty-two scripted matches, one per cell of the select grid, each recording
every cartridge offset it touched. Each run yields two things:

- **which team it was** - the 160-byte roster read names the team outright,
  so the grid cell never has to be trusted. Worth doing: several of the first
  attempt's scripts confirmed before the cursor had finished moving, and the
  roster read is what caught it.
- **which record it wore** - the opponent is always England, so the
  thirty-two byte read that is not England's is the team's own.

Forty-one of the forty-two resolve. Six teams share one record and two share
another, which is real rather than an artefact - a match between two of them
reads one record, not two. England is the one team this cannot resolve,
because it is the opponent in every run. kKitRecord in
ISSDNative/issd_mod_rom.c is that measurement and nothing else.

The other 42 records are the change strips the game switches to when two
teams would clash. Pairing those to their teams would need a match per team
against something that clashes with it, and has not been done.

### The near miss worth recording

There is a second palette table at 0x4C920 - 126 records of sixteen colours,
and 126 is 42 times 3, which looks exactly like a per-team kit table. It is
not the match palette: painting all 126 magenta changed nothing on the pitch.
It belongs to the select screen. The lesson is the usual one - a table whose
shape fits the theory is not evidence for the theory, and one mutation
settles it in two minutes.

---

## The team plate and photograph, drawn host-side

Both are pre-rendered graphics chosen by team, so a club the cartridge never
heard of has neither. As with the stadium plate, the host draws over them.
Three measurements:

- **The screen.** Team select and stadium select share game mode $32 = $06,
  $70 = $0C, so the four background scroll positions at $7E0018 separate
  them: 20, 36, 16, 32 on team select against 52, 44, 48, 40 on the stadium
  screen.
- **The selection.** Capturing the six cells of one group and looking for the
  byte that counted 60, 62, 64, 66, 68, 70 gives **$7E1526**: the team index
  doubled, which is how the screen indexes its own tables.
- **The rectangles.** The name plate is x 160-231, y 32-46, blue with a
  vertical gradient, lettered yellow with a magenta outline. The
  photograph's frame holds 96 x 72 at x 24, y 40.

The match HUD's own plate is left alone.

### The grid, and which team is in which cell

Renaming the plate is only half of it: the grid below names all six cells
of the group, and those are graphics too. Labelling them needs to know
which team is in which cell, and the grid is **not** in team order - its
first cell is England, which is team 2.

That order was measured first (42 matches, each naming its own team
through the 160-byte roster read) and then searched for: the sequence,
doubled, is 42 bytes at **0xDA3F**. So the label lands on the right team
from the cartridge's own table rather than from anything assumed.

### An eighth group

Raising the group count to 8 works - the screen draws a seventh and eighth
page. Its six cells read Austria, Japan, Nigeria, Brazil, Mexico and All
Star, which is the cell table running off its end into the table that
follows it, and that table cannot grow in place.

So a genuinely new page is: relocate the cell table to 48 entries, extend
the roster and formation pointer tables the same way, find room in the
attribute table, and work out how a team picks its kit palette - which is
the one thing here that was never found, only measured team by team. It is
the same shape of job as the stadium expansion, four times over, and it
has not been done.
