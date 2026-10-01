# Retail campaign passwords

The native Password page uses the supported cartridge's original password
instructions and tables. It does not define a new native password format.
Export runs the original encoder on private WRAM. Import first validates on
private WRAM, then queues the original Password task; the cartridge performs
the actual campaign restore on subsequent guest frames.

## Supported context

- The unmodified 2 MiB USA cartridge, SHA-256
  `d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9`.
- Effective cartridge bytes must equal the base cartridge and gameplay flags
  must be zero. Modified gameplay data disables retail password exchange.
- Export requires a verified, healthy, settled Cup or World Series campaign
  screen. Import requires the original game's settled Password input screen.
- Cup lengths are 12, 15 or 39 symbols; World Series lengths are 50, 11 or 8
  symbols, depending on the original campaign variant. The native field has
  capacity 60. Passwords are not universally 16 characters.
- Scenario and other mode bits `$1000`, `$0400` and `$0002` are outside this
  bridge, even when Cup/World bits are also present.

There are 64 symbols. Letter case matters, and several symbols are icons.
The verified palette order is:

```text
B C D F G H J K L M N P Q R S T
V W X Y Z b d f g h j n q r t 0
1 2 3 4 5 6 7 8 - + DIV PI = % < >
~ $ : " ? ! DOWN UP * # NOTE STAR SPADE DIAMOND CLUB HEART
```

`DIV` is the division sign, `PI` is pi, `DOWN`/`UP` are arrows, and the last
four symbols are card suits. Keyboard `/` is an alias for the division sign.
Other icons are selected from the palette. The cartridge glyph identifiers
come from `$87:DD45`; the selected-symbol label identifies each palette entry.

## Native page controls

Open the overlay with Escape/F1, the controller Guide button or Start+Back,
then choose `Cartridge Passwords...`. Arrow keys or the controller
D-pad move across the 64-symbol palette and Delete/Import/Export/Back actions.
Enter or controller A selects the current key/action; controller B returns.
Touch or click a palette key/action to select it directly. Keyboard typing
adds supported symbols with their original letter case, and Backspace removes
the last symbol. Escape returns from the Password page. Select Export at a
settled campaign checkpoint, or open the original game's Password screen
before using Import. A failed import retains the entered symbols for editing.

## Verified original flow

The original mode selector is `$A4:A9A3`, with symbol lengths at `$81:E44D`.
The original encoder is `$83:F46D`, decoder `$83:F674`, and 6-bit packing
helpers `$86:CB64` / `$86:CB27`. The field tables start at `$81:B2E4`.
The payload checksum and seed handling execute directly from the owned ROM.
Private execution has bounded WRAM, a read-only ROM bus and an instruction
budget. A failure never publishes partial decoded WRAM or exported symbols.

The original input screen is task `$8A:EA20`, mirrored in `$1446` and saved
at `$1538`, with screen identifiers `$32=6`, `$70=12`, no input-task busy flag,
and no fades. Successful submission writes the validated symbol array at
`$E2D0`, count/cursor at `$1542/$1546`, original OK/mode fields `$1544/$1548`,
and queues original callback `$8A:EAAD` in both task pointers. Merely setting
the submit-mode field does not submit under neutral controls.

The original task then runs the decoder and `$86:CAB7` dispatches to Cup
`$85:B76A` or World Series `$8B:A2C6`. No decoded campaign block is copied
into live WRAM by the native bridge. Invalid input leaves all live WRAM
unchanged; a successful queue leaves campaign fields unchanged until the
original guest executes.

## Reproducible verification

`tests/test_password_codec.py` compares native exports and decodes against an
independent harness executing original instructions from the user's ROM.
It checks the full glyph domain, exact goldens, all six lengths, several
teams/progress values/seeds, invalid passwords and context restrictions.
Two compact original-encoder fixtures (seed zero) are:

| Campaign state | Numeric symbol bytes (hex) |
| --- | --- |
| Cup `$1648=$0205`, teams `$DC00=60,62,64`, `$1F9C=2` | `003c10010200002437070800` |
| World `$1648=$0021`, `$1642=1`, `$1656=4`, team `$DA0=60`, `$1F9C=2` | `00341e08000400310310` followed by 40 zero bytes |

`tests/test_password_flow.py` generates actual campaign setup and original
Password-screen snapshots through the production guest. It exports the
captured campaign through the production wrapper, loads the unedited original
Password-screen snapshot and imports through the same wrapper as the native
UI. Real guest frames must restore the mode/team/progression and settled
callbacks, create exactly one campaign autosave, and Continue must restore
those campaign bytes without another save. The compact semantic fixtures in
`tests/fixtures/password/campaign_states.json` additionally cover original Cup
semifinal/final and World result/completion exports. Their original full-WRAM
exports were compared with the selected semantic regions before committing
the fixtures. Test-owned save directories isolate these checks from user
storage. No ROM bytes or copied ROM fixtures are stored
in the repository. These checks demonstrate the original ROM algorithm and
guest restore flow; they do not substitute for controller/touch testing on
each target device or comparison with physical cartridge hardware.

## Deterministic bridge tooling

The development CLI accepts raw numeric symbols, one byte per palette index
`0..63`; it is not an ASCII password file. Input is bounded to 1..60 bytes.
`--import-password-symbols file --import-password-at frame` invokes the same
validated import wrapper as the native page. When starting from a saved
original Password screen, use `--load-state 1 --import-password-at 2` so one
healthy guest frame completes after loading before submission. A failed
import exits with an error. `--export-password-symbols file` exports at
shutdown through the same healthy, settled checkpoint guard as the page.
