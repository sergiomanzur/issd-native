# Modding ISSD Native

A mod is a file you drop into `mods/`. Nothing is compiled, nothing is
patched on disk, and the cartridge you supply is never modified: packs are
applied to a copy of it in memory each time the game starts.

There are two kinds:

| Kind | What it is | What it changes |
|---|---|---|
| **Roster pack** | one `.json` file | team names, player names, stats, appearance, formations, tactics |
| **Tile pack** | a folder of `.bmp` files | the background graphics, at higher resolution |

```text
ISSDNative/
  ISSDNative.exe
  mods/
    world_cup_2026_mexico.json      <- a roster pack
    formation_showcase.json         <- another one
    title_screen_hd/                <- a tile pack
      0a3f19c4b7e25d80.bmp
      ...
```

---

## 1. Turning mods on

Open the pause menu, choose **Mods...**, and tick what you want.

```text
                  MODS
  -- TEAMS, STATS, FORMATIONS --
 >[ ] All-Star Legends
  [x] Formation Showcase
  [ ] FIFA World Cup 2026
  [x] World Cup 2026 - Mexico
  -- HD TILES --
  [x] title_screen_hd
  SAVE & RESTART (apply)
  Back

  Applied cleanly
  2p 20plr 7frm 397tile
```

Then choose **SAVE & RESTART**. Roster packs are applied to the cartridge
image before the engine boots and the game caches squads as a match loads,
so they cannot take effect part way through a session. The restart is what
applies them.

Tile packs are only images and take effect the moment you tick them.

The two lines at the bottom are the result of the last apply, and the same
message appears over the game for a few seconds after a restart:

| Message | Meaning |
|---|---|
| `Applied cleanly` | every enabled pack was used in full |
| `Applied, 2 warnings` | it worked, but something was ignored - see below |
| `1 PACK FAILED` | a pack could not be used at all; the next line names it |
| `Vanilla - nothing enabled` | no mods are on |

The counts are packs, players, formations and tiles.

---

## 2. Mods stack

Any number of packs can be on at once, and they are applied **top to
bottom in the order the list shows**. Where two packs change the same
thing, the later one wins.

Ticking a pack puts it at the bottom of the stack, so if you want one to
win, untick it and tick it again.

Overlaps are reported rather than silently resolved:

```text
[ModLoader] Formation Showcase overrides World Cup 2026 - Mexico on team 33.
```

That is a warning, not an error - stacking a roster pack under a formation
pack is a perfectly good way to work. It is there so you know which one you
are actually seeing.

Because packs stack, the most useful ones are **small and single-purpose**:
a squad pack, a formation pack, a tile pack. A pack that changes everything
cannot be combined with anything.

The enabled set is saved as `active_mod_packs` and `hd_texture_packs` in
the config file, as `|` separated lists in apply order. You can edit those
by hand if you prefer.

---

## 3. Your first pack

Save this as `mods/my_first_mod.json` and restart with it ticked:

```json
{
  "name": "My First Mod",
  "author": "you",
  "version": "1.0.0",
  "description": "Renames one Brazilian.",
  "teams": [
    {
      "team_id": 30,
      "name": "Brazil",
      "players": [
        {
          "shirt_number": 10,
          "name": "PELE",
          "position": "FW",
          "acceleration": 95,
          "speed": 92,
          "shooting": 99,
          "technique": 99,
          "balance": 88,
          "intelligence": 99,
          "dribbling": 99,
          "jumping": 85,
          "stamina": 90,
          "goalkeeping": 10
        }
      ]
    }
  ]
}
```

That replaces **squad slot 0** of team 30 - the goalkeeper - with a forward
called PELE. Which is the first thing worth understanding.

---

## 4. Slots, not players

Every team has exactly **20 squad slots**, and a pack fills them in the
order it lists players. The first player in your `players` array becomes
slot 0, the second slot 1, and so on. `shirt_number` is a label; it does
not choose the slot.

The slots mean something:

| Slot | Who it is |
|---|---|
| 0 | goalkeeper |
| 1-10 | the rest of the starting eleven, in formation order |
| 11 | reserve goalkeeper |
| 12-19 | outfield substitutes |

So list your keeper first, then your back line, then midfield, then
forwards. If you list fewer than 20, the rest of the squad is left as the
cartridge has it. More than 20 and the extras are ignored, with a warning.

**Teams cannot be added.** The cartridge indexes a fixed table of 36, and
nothing in the code reads a 37th. A pack that names `team_id: 40` gets a
warning and that team is skipped. What you can do is replace any of the 36
completely - name, squad, shape - which is how a World Cup pack is built.

Team ids run 0 to 35 in the order the team select screen shows them, six
per group. The N.S. America group is 30-35: Brazil, Argentina, Columbia,
Mexico, U.S.A, Uruguay.

---

## 5. Roster pack reference

The parser reads one `"key": value` per line, so keep the file formatted
the way the examples are - one key per line, no arrays or objects packed
onto a single line. Any key it does not recognise is skipped silently.

### Pack

| Key | Type | Notes |
|---|---|---|
| `name` | text | What the menu shows. **Also the pack's identity** in the saved list, so changing it loses the tick. |
| `author` | text | |
| `version` | text | |
| `description` | text | |
| `teams` | array | one entry per team you are changing |

### Team

| Key | Type | Notes |
|---|---|---|
| `team_id` | 0-35 | **Required.** Starts a team entry. A typo here is why a pack "loads but does nothing". |
| `name` | text | For your own reference and the log. The name on the team select screen is a graphic and is not changed. |
| `formation` | text | See section 6. Optional. |
| `tactics` | text | `attacking`, `balanced`, `defensive`. Optional. |
| `players` | array | Optional: leave it out to change only the shape. |

### Player

| Key | Type | Notes |
|---|---|---|
| `shirt_number` | 1-99 | **Required.** Starts a player entry. |
| `name` | text | **8 characters**, A-Z a-z and space. Longer names are cut; anything else becomes a space. |
| `position` | `GK` `DF` `MF` `FW` | The letters shown beside the name. |
| `skin_tone` | 0-2 | 0 light, 1 medium, 2 dark |
| `hair_style` | 0-15 | |
| the ten attributes | 0-99 | see below |

### Attributes

- `acceleration` - how fast he reaches top speed
- `speed` - top speed
- `shooting` - shot power and accuracy
- `technique` - first touch, curve, volleys
- `balance` - resistance to being knocked off the ball
- `intelligence` - positioning, both attacking and defending
- `dribbling` - close control when turning
- `jumping` - heading reach
- `stamina` - how long he lasts
- `goalkeeping` - only meaningful in slot 0 and slot 11

Write these on the familiar 0-99 scale. **They are not stored that way.**
The cartridge holds each attribute in four bits, and across its own 720
players it only ever uses values 2 to 9 - so 0-99 is mapped onto that band,
eight steps wide.

Two consequences worth knowing before you author a squad:

- **Use the whole range.** Ratings all bunched in the 70s and 80s land on
  the same two or three steps and your whole squad plays identically. A
  world-class forward should be near 99 and his stamina when he is a poor
  defender should be near 20.
- **Differences smaller than about 12 points disappear.** 80 and 85 are the
  same player.

---

## 6. Formations and Tactics

A team's shape is not derived from its players' listed positions. The
cartridge keeps it in a record of its own, and a mod pack sets it with two
keys on the team:

```json
{
  "team_id": 33,
  "name": "Mexico",
  "formation": "4-2-3-1",
  "tactics": "attacking"
}
```

Both are optional. Leaving `formation` out keeps the team's original shape.
Names are matched with case and separators ignored, so `4-2-3-1`, `4231` and
`4 2 3 1` are the same formation.

### Available formations

| Name | Shape | Printed as |
|---|---|---|
| `4-4-2` | Flat back four, flat midfield four, two strikers | 4-4-2 |
| `4-4-2 diamond` | Back four, a holder, two wide, one behind the strikers | 4-4-2 |
| `4-3-3` | Back four, a midfield three, a front three | 4-3-3 |
| `4-2-3-1` | Two holding midfielders, three behind a lone striker | 4-5-1 |
| `4-1-4-1` | An anchor behind a midfield four, one striker | 4-5-1 |
| `4-5-1` | Back four, a flat five across midfield, one striker | 4-5-1 |
| `3-5-2` | Three at the back, wing backs, two strikers | 3-5-2 |
| `3-4-3` | Three at the back, four across, a front three | 3-4-3 |
| `3-4-2-1` | Three at the back, two shadow strikers behind a lone man | 3-4-3 |
| `5-3-2` | Five at the back, three in the middle, two up | 5-3-2 |
| `5-4-1` | Five at the back, four across, one striker | 5-4-1 |
| `4-2-4` | Back four, two in midfield, four forwards | 4-2-4 |
| `3-3-4` | Three at the back, three in the middle, four up | 3-3-4 |
| `2-3-5` | The pyramid. Two at the back and five forwards | 2-3-5 |

The **Printed as** column is what the team select screen shows. The screen
can only draw three numbers, so a four-band shape is labelled the way a
newspaper would have labelled it in 1995: a 4-2-3-1 reads as 4-5-1. What the
players actually do on the pitch follows the shape, not the label - the two
holding midfielders in a 4-2-3-1 really do sit behind the other three.

### Tactics

| Value | Effect |
|---|---|
| `attacking` | The whole side starts six units further up the pitch; full backs overlap and midfielders push on. |
| `balanced` (default) | The formation as authored. |
| `defensive` | The whole side sits six units deeper and nobody is given an attacking role. |

`strategy` is accepted as an alias for `tactics`.

Tactics never change how many players are in each line, so the printed label
stays true whichever preset is used.

### Seeing a formation in game

Pick the team, then from the pre-match menu choose **FORMATION CHANGE**.
That screen draws all eleven at their home positions across DF / MF / FW
bands, and highlights the label in its **TYPE OF FORMATION** list. A small
arrow beside a player marks an attacking role, so a side set to
`defensive` shows none at all.

`mods/formation_showcase.json` puts six different shapes on the six N.S.
America teams and changes no squads, so they can be stepped through
side by side.

### A team entry may change only the shape

A team needs no `players` list at all. Naming just `team_id` and
`formation` leaves the squad exactly as the cartridge has it and changes
only how it lines up.

### Squad order matters

A formation assigns positions by squad order, not by the `position` string.
Slot 0 in the `players` list is the goalkeeper, and players 1 to 10 fill the
formation's ten outfield slots in the order listed. For `4-2-3-1` that is:
four defenders, the two holding midfielders, the three behind the striker,
and the striker last.

If the `position` strings of the first eleven do not add up to the same
number of defenders, midfielders and forwards as the formation, the loader
says so on startup and carries on: the pitch follows the formation and the
squad list follows the positions, and which of the two is wrong is the
author's call.

### How this is stored

For anyone extending the tooling: `$8B:EF48` holds thirty-six 16-bit
pointers, one per team, into bank `$8B`. Each names a 31-byte record:

| Offset | Bytes | Meaning |
|---|---|---|
| `+0` | 1 | Label index, 0-15. The three-number string the team select screen prints. |
| `+1` | 20 | Ten `(depth, width)` signed byte pairs, one per outfield player in squad order. Negative depth is further up the pitch. |
| `+21` | 10 | Ten role bytes: 1 defender, 5 overlapping defender, 2 midfielder, 6 attacking midfielder, 3 forward. |

The label indices, all sixteen read off the screen one at a time, are:
`0` 4-5-1, `1` 4-4-2, `2` 4-3-3, `3` 4-2-4, `4` 3-5-2, `5` 3-4-3, `6` 3-3-4,
`7` 3-2-5, `8` 2-5-3, `9` 2-4-4, `10` 2-3-5, `11` 5-4-1, `12` 5-3-2,
`13` 5-2-3, `14` 1-5-4, `15` 1-4-5.

The label is independent of the record's own contents, so it has to be kept
consistent by hand. `tests/test_formation.c` asserts that every entry in the
library counts its roles the way its label claims.

---

## 7. High Resolution Tiles

A tile pack replaces the cartridge's 8x8 background graphics with larger
images. It is a directory of BMPs under `mods/`, and it changes nothing
about the cartridge: the game still runs its own graphics, and the
replacements are drawn over them as the frame is enlarged for the window.

```text
mods/
  title_screen_hd/
    0a3f19c4b7e25d80.bmp      32x32, a 4x replacement for one 8x8 tile
    1b77e0c9f2a41d63.bmp
    ...
```

Pick one from the pause menu under **HD Tiles**, or set
`hd_texture_pack=title_screen_hd` in the config. Packs are only images, so
switching takes effect immediately - no restart, unlike a roster mod.

Replacements are drawn into the enlarged frame, so **Internal** has to be
2X or higher. At 1X there is nothing to put the extra detail into and the
menu row says `NEEDS 2X+`.

### Making one

Play to the screen you want, dumping what the game draws:

```sh
ISSDNative --headless 3700 --dump-tiles tiles --dump-tiles-from 3650
```

Every distinct background tile is written as an 8x8 32-bit BMP named after
its identity, alongside a `tiles.csv` listing each one's colour depth and
the frame it first appeared on. A run walks through every screen before the
one you want, so `--dump-tiles-from` skips the ones you do not.

Then enlarge them:

```sh
python tools/upscale_tiles.py tiles mods/my_pack --scale 4
```

The filenames are the identities, so anything in that directory under the
right name is used. Replace any of them by hand - with an AI upscaler, or
redrawn from scratch - and the game picks that up instead. Any square
32-bit BMP whose edge is a multiple of 8 works, so a pack can mix 2x and 8x
tiles; each is sampled to whatever the current scale is.

### What a tile's name means

The name is a hash of the tile's own graphics **and** the colours it is
drawn in. The cartridge reuses one piece of artwork across several
palettes - the same crowd tile in a dozen kit colours - and a pack author
almost always wants to treat those as separate images, so they are.

The flip side is that a tile drawn in a palette that fades in or out is a
different tile at every step of the fade, and will not be replaced unless
every step is in the pack.

### What does not get replaced

- **Sprites.** Players, the ball and most UI text are objects, not
  background tiles. Only backgrounds are covered.
- **Anything drawn on top.** A background pixel is replaced only when the
  frame still holds exactly the colour that tile would have put there, so a
  pixel covered by a sprite, a higher layer, or colour maths is left as the
  game drew it. This is what keeps players in front of the pitch instead of
  behind it.
- **Transparent pixels.** Colour index 0 shows whatever is behind it, so
  there is nothing to replace.
- **The widescreen margins**, which are reconstructed rather than drawn from
  a tilemap.
- **Mode 7**, which is not a tilemap at all.

Expect roughly two thirds of a screen to be background. On the title screen
it measures 66%.

### Shipped example

`mods/title_screen_hd` is the title screen's 397 tiles enlarged 4x. It is
there to show the pipeline working end to end, not because a smooth filter
is the best a pack can do - it is the floor, and hand-made or AI-upscaled
art goes in the same folder under the same names.

---

## 8. When something does not work

The console window carries the detail; the menu carries the summary.

| What you see | What it means |
|---|---|
| `X has no teams - check the team_id keys` | The file parsed but no `team_id` was found. Usually a typo, or `team_id` written as a string (`"30"` instead of `30`). |
| `cannot open mods/x.json` | Unreadable or locked. |
| `team_id 40 is out of range` | Only 0-35 exist. |
| `A overrides B on team 33` | Both packs change that team; A is later in the stack and wins. |
| `unknown formation "X"` | Not in the library; section 6 lists the names. The team keeps its own shape. |
| `formation "4-2-3-1" lines up 4-5-1 but the first eleven are listed 4-3-3` | Your `position` letters and your formation disagree. The pitch follows the formation. |
| `squad longer than 20` | Only the first 20 are used. |
| Pack ticked, nothing changed | Did you **SAVE & RESTART**? Roster packs only apply at boot. |
| Tile pack ticked, nothing changed | Tiles need **Internal** at 2X or higher. The row reads `NEEDS 2X+` when it is not. |

---

## 9. What cannot be modded

Being straight about the ceiling saves everyone time.

- **New teams.** The table is 36 and fixed. Replace, do not add.
- **More than 20 players per squad.**
- **Player names longer than 8 characters**, and no accents or digits.
- **Team names on the team select screen**, which are drawn graphics rather
  than text.
- **New formation labels.** The screen can only print three numbers and the
  sixteen it knows are fixed. The shape itself is free - see section 6.
- **Sprites.** Tile packs replace backgrounds; players, the ball and most UI
  text are objects, and are not covered.
- **New stadiums.** The ones that exist can be re-coloured through a tile
  pack; adding one would need code that does not exist.
- **Audio.** Not wired up.

---

## 10. Sharing a pack

A pack is just files. Zip the `.json`, or the tile folder, and say which
version of ISSD Native you built it against.

Please do not distribute a cartridge image with it. Packs are applied to a
copy of whatever ROM the player supplies, which is the whole reason they
are kept separate.

Two conventions that make packs stack well:

1. **One job per pack.** Squads, shapes and tiles in separate files.
2. **A stable `name`.** It is the identity in the saved list, so renaming a
   pack silently unticks it for everyone who had it on.
