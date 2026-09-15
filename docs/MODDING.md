# Modding ISSD Native

A mod is a file you drop into `mods/`. Nothing is compiled, nothing is
patched on disk, and the cartridge you supply is never modified: packs are
applied to a copy of it in memory each time the game starts.

There are two kinds:

| Kind | What it is | What it changes |
|---|---|---|
| **Roster pack** | one `.json` file (and any images beside it) | player names, stats, appearance, formations, tactics, kit colours, the select screen's name plate and squad photograph, stadiums |
| **Tile pack** | a folder of `.bmp` files | the background graphics, at higher resolution |

```text
ISSDNative/
  ISSDNative.exe
  mods/
    world_cup_2026_mexico.json      <- a roster pack
    chivas_guadalajara.json         <- another one
    chivas_guadalajara/             <- images it names
      team_photo.bmp
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

## 2a. The editor

There is a visual editor for all of this:

```
tools/mod_studio/dist/ISSDModStudio.exe      built
python tools/mod_studio_launch.py            from source
python tools/build_mod_studio.py             rebuild the .exe
```

It reads and writes the same `.json` files this guide describes, so the
two are interchangeable - edit a pack by hand, open it in the editor, and
back again. **Check pack** runs the same rules `tools/validate_mod.py`
does.

### Starting from what is already there

**Import from cartridge** reads your own dump and brings teams and grounds
in as they are: twenty names each, their positions, appearance and
ratings, the shape the team plays and the colours it wears.
Open Mexico, change two players, save. A team pane in *replace* mode also
has a **Load this team from the cartridge** button for one at a time.

An import that is saved and applied unchanged leaves the game exactly as
it was - all 720 players written back produce byte-identical squads in
memory, which is the test that keeps it honest.

It is worth a look even if you prefer a text editor, for three things it
draws that a file cannot show you: the strip in the shades the cartridge
will really store, the formation as a shape, and - the one that catches
everybody - the band each rating quantises into. See
`tools/mod_studio/README.md`.

The rest of this guide is the file format, which is what the editor
writes.

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

A pack can **replace** one of the 36, or **add** up to six more.

### Replacing

Name the `team_id` and the team is taken over completely - squad, shape,
strip, name plate and squad photograph. That is how a World Cup pack is
built: the countries are already there and only need rewriting.

### Adding

```json
  { "new_team": true, "name": "Chivas de Guadalajara", "players": [ ... ] }
```

No `team_id`: the pack is given one of six slots and told which in the log.
Nothing is replaced - all 36 countries stay exactly as they are.

`mods/chivas_guadalajara.json` is the worked example. It adds a 37th team
with its own squad, a 4-3-3, the red-and-white strip, a name plate and a
squad photograph, and touches no existing team.

The six slots are the seventh group the cartridge hides - ALL STAR,
EUROSTAR A and B, ASIAN STAR, AFRICAN STAR, ALL AMERICAN STAR. Adding a
team switches that group on and turns its first cell into a real team;
add two and the first two cells become real, and so on. Cells you do not
take stay as they were, and the loader names the side whose cell each
added team took.

**They do come out of that group.** An eighth page would avoid it, and it
draws correctly with all six of its new teams selectable. Confirming one
still does not work: twenty tables the game indexes by team have been
relocated and there are more, some of which are shared pools that must not
be. `docs/REVERSE_ENGINEERING.md` has where it stands and what would
close it.

**They are not spare squads to begin with.** Each is assembled when the
match loads out of the group it belongs to - team 36+g takes twenty
players from the six rosters of group g, which is why ALL AMERICAN STAR
fields Brazil's keeper. `new_team` is what gives one a roster of its own,
written into free cartridge space.

So, if you would rather leave them as all-star sides and just adjust them,
name the `team_id` (36-41) and give **no** `players`:

| | |
|---|---|
| `players` | use `new_team` instead - listing them here is an error |
| attributes | applied; they are real and stored per team |
| `formation` / `tactics` | applied |
| `shirt` / `shorts` / `socks` | applied |
| `plate_name` / `photo` | applied |

A pack that only wants the group visible can still say
`"unlock_bonus_teams": true` without adding anything.

Team ids run 0 to 35 in the order the team select screen shows them,
six per group. Read off the screen one team at a time:

| id | team | id | team | id | team |
|---|---|---|---|---|---|
| 0 | Italy | 1 | Holland | 2 | England |
| 3 | Norway | 4 | Spain | 5 | Ireland |
| 6 | Portugal | 7 | Denmark | 8 | Germany |
| 9 | France | 10 | Belgium | 11 | Sweden |
| 12 | Romania | 13 | Bulgaria | 14 | Russia |
| 15 | Swiss | 16 | Greece | 17 | Croatia |
| 18 | Austria | 19 | Wales | 20 | Scotland |
| 21 | N. Ireland | 22 | The Czech Rep. | 23 | Poland |
| 24 | Japan | 25 | S. Korea | 26 | Turkey |
| 27 | Nigeria | 28 | Cameroon | 29 | Morocco |
| 30 | Brazil | 31 | Argentina | 32 | Columbia |
| 33 | Mexico | 34 | U.S.A | 35 | Uruguay |

The groups are, in the order the screen pages through them: 0-5 Europe 1,
6-11 Europe 2, 12-17 **Europe 4**, 18-23 **Europe 3**, 24-29 Asia-Africa,
30-35 N.S. America. Three and four really are that way round.

---

## 5. Roster pack reference

A pack is ordinary JSON. Key order does not matter, whitespace does not
matter, and any key the game does not recognise is skipped - so a pack can
carry whatever an editor wants to keep alongside it. Line (`//`) and block
(`/* */`) comments are accepted too, which JSON does not strictly allow.

A file that will not parse is refused with a line number rather than
half-read.

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
| `new_team` | true | **Adds** a team instead of replacing one. Leave `team_id` out; a slot is assigned and named in the log. Six are available. |
| `team_id` | 0-35, or 36-41 for an all-star side | Required unless `new_team` is set. A typo here is why a pack "loads but does nothing". |
| `name` | text | For your own reference and the log. |
| `plate_name` | text, 12 max | What the **select screen's name plate** should read. The cartridge's plates are graphics, one per team, so the host draws this one instead. Leave it out and the cartridge's own stands. |
| `photo` | filename | A 32-bit `.bmp` beside the pack, drawn over the **squad photograph**. Any size; it is sampled into the 96x72 the frame leaves. |
| `shirt` | `"#RRGGBB"` | Shirt colour. The three shades the cartridge uses are derived from it. |
| `shorts` | `"#RRGGBB"` | Shorts colour. |
| `socks` | `"#RRGGBB"` | Sock colour. |
| `kit_record` | 0-83 | Which of the 84 kit palettes to paint. Only needed for a team the measured table does not cover - see section 6a. |
| `formation` | text | See section 6. Optional. |
| `tactics` | text | `attacking`, `balanced`, `defensive`. Optional. |
| `players` | array | Optional: leave it out to change only the shape. |

A pack may also carry a `stadiums` array alongside `teams`; see section 8.

### Player

| Key | Type | Notes |
|---|---|---|
| `shirt_number` | 1-99 | **Required.** Starts a player entry. |
| `name` | text | **8 characters**, A-Z a-z, space and a full stop. Longer names are cut; anything else becomes a space. Leading spaces are how the cartridge sits a name on screen - `" Pabi"` - and are kept. |
| `position` | `GK` `DF` `MF` `FW`, or 0-15 | The letters shown beside the name. The cartridge has six codes, not four: 3 and 5 sit between defence and midfield and between midfield and attack, and 51 of its 720 players have one. Write the number to keep one exactly. |
| `skin_tone` | 0-1 | Misnamed, and the name is kept so packs already written still load. It is the palette the player is drawn with: 0 dark hair, 1 fair. Those are the only two the cartridge uses; 2 and 3 select palettes the strip does not fit and turn the whole player orange or green. |
| `hair_style` | 0-15 | The other half of the same byte. Nothing in a match reads it, though the cartridge's own squads vary it from 0 to 13. Kept so an imported squad goes back byte for byte. |
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

## 6a. Strips, plates and squad photographs

Three things carry a team's identity on screen, and each is reached a
different way.

### The strip

```json
  { "team_id": 35, "shirt": "#C8102E", "shorts": "#123A6B", "socks": "#FFFFFF" }
```

The cartridge keeps a seventeen-colour palette per kit: three shirt shades,
three shorts shades, two sock shades, plus skin and hair it shares with
everyone. Give the colour you want and the shades are derived from it the
way the cartridge shades its own - the lit shade is your colour, the others
roughly three quarters and three fifths of it. Leave a part out and it is
not touched. `#RRGGBB`, with or without the hash, upper or lower case.

It is one flat colour per part, so a striped shirt cannot be drawn this way
- the stripes are in the sprite graphics, not the palette. Chivas' red is
the colour the strip reads as on the pitch.

**Some teams share a strip.** Which of the 84 palettes a team wears is not
written down anywhere in the cartridge; it was measured a match at a time
(see `docs/REVERSE_ENGINEERING.md`). Six teams - Swiss, Wales, Scotland,
N. Ireland, Czech Rep. and Poland - turned out to share one palette, and
Austria and Turkey another. Recolour one of those and you recolour them
all; the loader says so in the results. England's own palette is one of
the two it was seen wearing and could not be told apart, so it is the one
team the table leaves blank - name `kit_record` to paint it.

### The name plate

```json
  { "team_id": 35, "plate_name": "CHIVAS" }
```

The plate beside the flag on the select screen is a graphic, one per team,
so there is no way to add a word. The host paints over it instead - the
same trick the stadium plate uses. Twelve characters fit. The flag next to
it is left as the cartridge drew it.

The plate in the **match** HUD is a different graphic and still shows the
cartridge's name.

### The squad photograph

```json
  { "team_id": 35, "photo": "chivas_guadalajara/team_photo.bmp" }
```

A 32-bit BMP, anywhere beside the pack file, drawn into the frame on the
select screen. The frame's inside is **96 x 72**; any size is accepted and
sampled to fit, so author at 96x72 for pixel-exact work or larger if you
would rather draw big. The cartridge's grey mount is left showing around
it.

Most editors save 32-bit BMPs bottom-up; both orders are read.

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

A folder of files called `6bc41b4898647d62.bmp` is not something you can
find a penalty spot in, though, so the editor's **Pitch tiles** pane shows
the dump as pictures instead, sorted so the grass comes first, and writes
replacements into a pack for you. See `tools/mod_studio/README.md`.

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

## 8. Stadiums

The cartridge ships eight stadiums. Any of them can be replaced, and
`stadium_count` adds more - up to thirty-two.

```json
{
  "name": "My Pack",
  "stadiums": [
    {
      "stadium_id": 7,
      "name": "AKRON",
      "pitch_length": 115,
      "pitch_width": 74
    }
  ]
}
```

A pack may hold only `stadiums` and no `teams` at all.

### Adding stadiums

```json
  "stadium_count": 9,
  "stadiums": [
    { "stadium_id": 8, "name": "AKRON", "pitch_length": 115, "pitch_width": 74 }
  ]
```

`stadium_count` is how many the game offers at all. Raising it moves four
tables into free space, extends them, and re-points the code that reads
them - so slot 8 is a real ninth stadium with its own name, size and turf,
not a slot reading past the end of a table. New slots start as copies of
the cartridge's own, so one you have not customised still works.

Across a stack of packs the **largest** `stadium_count` wins, so a pack
that only replaces a stadium does not need to care how many there are.

Name a stadium you have added and the game draws its plate for you, so
there is no ceiling from the cartridge's supply of plate graphics. A slot
you add but never name keeps whatever unused graphic the cartridge has at
that index, which the validator warns about.

If this cartridge is not the USA revision the tables were measured on,
every patch site is checked before anything is written and the expansion
is refused rather than applied to the wrong bytes.

### Two names

```json
  { "stadium_id": 8, "name": "AKRON", "display_name": "EST. AKRON" }
```

`name` goes into the cartridge and is what the **pre-match** screen prints:
seven characters, because that is the size of the field it lives in.

`display_name` is what the **select screen's plate** shows, and the host
draws that, so it holds twelve. Leave it out and the plate shows `name`.
Anything over nine characters is drawn at a tighter pitch so it still fits.


| id | stadium | pitch |
|---|---|---|
| 0 | JAPAN | 114 x 74 |
| 1 | U.S.A | 118 x 82 |
| 2 | SPAIN | 126 x 90 |
| 3 | ITALY | 130 x 82 |
| 4 | ENGLAND | 122 x 82 |
| 5 | GERMANY | 122 x 74 |
| 6 | BRAZIL | 114 x 90 |
| 7 | NIGERIA | 138 x 90 |

`name` is at most **7 characters**, letters, spaces and full stops - which
is why the cartridge's own list is countries rather than grounds. It is
stored right-aligned, as the cartridge stores its own, because the plate
centres the field.

`pitch_length` and `pitch_width` are in yards and are clamped to 100-140
by 64-96, the range the cartridge's own stadiums span.

### What a renamed stadium actually changes

Be clear about this before building a pack around it.

| | Changes? |
|---|---|
| Pre-match screen (`AKRON STADIUM 115y`) | **yes** |
| Pitch dimensions printed on the select screen | **yes** |
| Pitch preview drawn on the select screen | **yes** |
| The name plate on the select screen | **yes** - the host draws it |
| The turf pattern | **yes** - via the table a new slot inherits |
| How the pitch actually plays | **no** |

That last row is measured, not assumed: the same match played on the
138 x 90 pitch and on a 115 x 74 one, with identical inputs, leaves WRAM
byte-identical 1400 frames in. The numbers are what the screens print. A
field called `pitch_length` invites the opposite reading, so it is worth
saying twice.

The stands, crowd and advertising boards are background tiles, so a tile
pack (section 7) is how a stadium gets its own look.

### Shipped example

`mods/world_cup_2026_mexico.json` puts Chivas' Estadio Akron in slot 7.
Nothing else in the shipped packs uses that slot, and the 2026 tournament
has no Nigerian venue.

---

## 9. Generating packs with a tool

If a script or an AI agent is writing the pack, three things matter more
than anything in the syntax.

### Check it before you ship it

```sh
python tools/validate_mod.py mods/my_pack.json
```

It exits non-zero on anything that would break, and warns about the things
that load fine and then disappoint. `--strict` fails on warnings too, which
is what you want in a generate-and-check loop. The formation names it
checks against are read out of the game's own source, so it cannot drift.

### Slots are positional

The order of the `players` array **is** the team sheet. `shirt_number` is a
label. Slot 0 is the goalkeeper, 1-10 are the rest of the eleven in
formation order, 11 is the reserve keeper, 12-19 are substitutes.

The commonest generated-pack mistake is a squad sorted by shirt number.
That gives you a goalkeeper at right back.

### Ratings quantise to eight steps

0-99 is the scale you write; the cartridge stores four bits and uses only
eight of the sixteen values. A squad rated 78-98 - which is what "these are
all world class players" naturally produces - lands on **two** steps, and
every one of them plays the same.

Spread a squad across the whole range. If your best player is 95, your
worst at that attribute should be near 20, not near 80. The validator
warns when an attribute uses two steps or fewer.

This is the mapping:

| You write | Stored as | Roughly |
|---|---|---|
| 0-7 | 2 | hopeless |
| 8-21 | 3 | poor |
| 22-35 | 4 | below average |
| 36-49 | 5 | average |
| 50-63 | 6 | good |
| 64-77 | 7 | very good |
| 78-91 | 8 | excellent |
| 92-99 | 9 | the best the game has |

Goalkeeping is the exception: it only means anything in slots 0 and 11, so
two clusters there is correct.

### A worked prompt

If you are asking a model for a pack, this is the shape of instruction that
produces a working one:

> Write a mod pack replacing team 30 with the 1970 Brazil squad. Output
> JSON only. `team_id` must be 30. The `players` array is positional:
> slot 0 is the goalkeeper, slots 1-4 defenders, 5-8 midfielders, 9-10
> forwards, slot 11 the reserve goalkeeper, 12-19 substitutes. Names are
> at most 8 characters, letters and spaces only - use surnames. Ratings
> are 0-99 and must span that range: the weakest player at an attribute
> should be near 20 where the strongest is near 95, because the game
> stores only eight steps. Set `formation` to one of: 4-4-2, 4-4-2
> diamond, 4-3-3, 4-2-3-1, 4-1-4-1, 4-5-1, 3-5-2, 3-4-3, 3-4-2-1, 5-3-2,
> 5-4-1, 4-2-4, 3-3-4, 2-3-5.

Then run the validator and feed any errors back.

---

## 10. When something does not work

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

## 11. What cannot be modded

Being straight about the ceiling saves everyone time.

- **More than six added teams.** The seventh group has six cells and that
  is what there is. Past 42 the grid itself would have to grow.
- **Keeping an all-star side *and* adding a team in its cell.** A cell is
  one or the other.
- **Striped or hooped shirts.** A kit is one colour per part.
- **The team name in the match HUD**, and the flag beside it. The select
  screen's plate can be renamed; those cannot.
- **More than 20 players per squad.**
- **Player names longer than 8 characters**, and no accents or digits. A
  full stop is fine: the cartridge is full of them.
- **Team names on the team select screen**, which are drawn graphics rather
  than text.
- **New formation labels.** The screen can only print three numbers and the
  sixteen it knows are fixed. The shape itself is free - see section 6.
- **Sprites.** Tile packs replace backgrounds; players, the ball and most UI
  text are objects, and are not covered.
- **More than 32 stadiums.** The free space the tables move into would
  take about 190, but nobody needs that and 32 is tested.
- **More than 42 teams.** Forty-two is what the select screen's seven
  groups of six hold, and the roster pointer table has exactly 43 entries.
  Going past that would mean growing the grid as well as the tables.
- **The name on the stadium select screen**, which is drawn from shared
  font tiles through a tilemap rather than from the name table. The
  pre-match screen does show a renamed stadium.
- **Audio.** Not wired up.

---

## 12. Sharing a pack

A pack is just files. Zip the `.json`, or the tile folder, and say which
version of ISSD Native you built it against.

Please do not distribute a cartridge image with it. Packs are applied to a
copy of whatever ROM the player supplies, which is the whole reason they
are kept separate.

Two conventions that make packs stack well:

1. **One job per pack.** Squads, shapes and tiles in separate files.
2. **A stable `name`.** It is the identity in the saved list, so renaming a
   pack silently unticks it for everyone who had it on.
