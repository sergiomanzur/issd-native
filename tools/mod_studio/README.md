# ISSD Mod Studio

A visual editor for the mod packs in `mods/`. It never touches a cartridge:
it reads and writes the same `.json` files the game loads.

```
tools/mod_studio/dist/ISSDModStudio.exe            the built editor
python tools/mod_studio_launch.py                  the same thing from source
python tools/build_mod_studio.py                   rebuild the .exe
```

Needs Python 3.10 or newer and Pillow to run from source, and
PyInstaller as well to build the `.exe`:

```
python -m pip install pillow pyinstaller
```

The `.exe` is not committed - it is 29 MB of bundled Python, and the build is
one command. Everything it needs is in the repository.

## What it edits

| | |
|---|---|
| **Pack** | name, author, version, description, `stadium_count`, whether to show the seventh group |
| **Teams** | add (replacing nobody) or replace one of the 36; name, plate name, squad photograph, formation, tactics, kit colours |
| **Players** | name, number, position, skin tone, hair style, the ten ratings |
| **Stadiums** | slot, name, plate name, pitch size |

Add, duplicate and delete from the toolbar, the Edit menu or the Del key.
**Check pack** runs the same rules `validate_mod.py` does - literally the same
module, so the editor cannot disagree with the command line.

## Starting from the cartridge

**Import from cartridge** points at your own dump and brings teams and
grounds in exactly as they are - squads, positions, skin tones, hair
styles, ratings, shapes and kit colours - so a pack starts from the real
thing rather than a blank sheet. The path is remembered.

Reading and writing have to be each other's inverse or a round trip
quietly corrupts a squad, so the decoder is built from the same character
set, offsets and quantiser the encoder uses, parsed out of
`ISSDNative/issd_mod_rom.c`. Two details came out of testing that:

- a full stop is a real character (`R.Banks`), stored as `0x54`, and the
  encoder had been turning every one into a space;
- a name's **leading** blanks are how the cartridge places it on screen and
  are kept, while trailing ones are just the rest of the field. Stadium
  names are the other way round - the writer right-aligns them itself.

## What it draws, and why

Three numbers in a pack are not what a player sees, so the editor draws them:

- **The strip.** The cartridge keeps five bits per channel and derives the
  other two shades itself, so `#C8102E` is not the red that reaches the pitch.
  The preview shows the three tones it will actually store.
- **The shape.** Ten pairs of signed numbers are not a formation. Attacking
  and defensive shift every line, and the drawing shifts with them.
- **The pitch.** 138 x 90 and 114 x 74 are the same two numbers until they are
  side by side.

And the one genuinely surprising thing about this cartridge's ratings: a
rating is 0-99 in the pack but four bits in the game, of which only 2-9 are
used. Every slider has the band it quantises into drawn behind it, so it is
obvious that 55 and 60 are the same player.

## Where its facts come from

Nothing about the cartridge is typed into the editor by hand. The formations,
the 42 team names and the rating quantiser are parsed out of
`ISSDNative/issd_formation.c` and `ISSDNative/issd_mod_rom.c` every time the
editor starts from source.

A packaged `.exe` has no repository to read, so the build bakes a snapshot
into `baked.json`. `tests/test_mod_studio.py` re-parses the repository and
fails if the snapshot has drifted, which is what stops the editor describing a
game that no longer exists.

## Checking a build

A windowed `.exe` has no console, so it writes its self-test to a file:

```
ISSDModStudio.exe --selftest report.txt [pack.json]
```

It opens a window, builds every pane, runs the checker and exits - which is
how a missing `baked.json` or a Pillow that cannot talk to tk gets caught
before a user clicks the thing that needs it.
