# Cartridge campaign checkpoint evidence

Target: the local USA cartridge, observed using the production native headless
runner. ROM bytes and captured WRAM/snapshots are not distributed. All addresses
below are original SNES addresses; WRAM offsets omit the $7E bank.

## Original task flow

$80:AF13 dispatches $0070; entry 12 reaches $80:B3FA. Menu tasks run with
D=$1400. $85:865A stores the active child input callback at $1446..$1448.
The actual observed menu scene is $0032=6, $0070=12, irrespective of older
bridge/document labels. $001A and $19A2 are scratch and cannot identify campaigns.

$85:B947 copies $1640..$169F to $7E:DDFF..$DE5E before play. $85:B931
restores these bytes after play. Live $1648.. is reused for player data; its
campaign-looking bits must not be trusted outside the verified menu scene.
$85:C117 explicitly reads the backed-up flags at $DE07 during play.

Campaign identity is ($1648 & $24)==$04 for Cup, ==$20 for World Series.
Both bits together are invalid. Cup initial flags are $0205; World initial
flags $0021. The low setup bit is cleared by the first outcome, but Cup sets
it again when constructing its next stage ($4205 after regional qualification).
It therefore cannot exclude a committed Cup result with a positive round count.

## Enabled checkpoints

All predicates additionally require the observed menu scene and $1460=$1462=0.

| Event | Witnessed preparation | Committed child callback | Progress requirement |
|---|---|---|---|
| Cup initial setup | $85:C37C | $A4:BEB6 | setup bit, $1652=$165E=$1660=0 |
| World initial setup | $8B:9431 | $86:C3EB | setup bit, $1652=$165E=$1660=0 |
| Cup result | original final statistics exit | $85:C37C | positive $1652 |
| World result | original final statistics exit | $8B:9496 | setup bit clear, positive $1652 |
| World completion | last result then table | $8B:94EE | setup bit clear, $1652=35 |
| Cup knockout result | original outcome commit | $85:D32E | knockout bit $08, $1640=5..8 |
| Cup completion | original final commit and ceremony, then bracket | $85:D32E | knockout bit $08, $1640=9, $1652=0, valid champion byte $DDCE |
| Trusted password import | successful original submission | Cup $8B:953C / World $8B:9414 | explicit import arm, valid campaign |

Cup $85:B404 commits the outcome/standings, then increments $1652 and updates
teams with $85:C249. The next regional outcome uses $85:B431 and produces
$4205/$1652=2 before Cup constructs its following stage. The Cup state machine
also uses $1640; round counters reset between stages. Result identity therefore
includes $1640, $1652, $165E and $1660.

World $8B:9984 commits results, and $8B:9A2B advances $1652 if below 36.
$8B:9F19/$9F83 calculate standings before returning to $8B:9496. The final
round table checks $1652=35 at $8B:9CF5 and $8B:9DB7 and settles at $8B:94EE.
The following A opens the cartridge password display; this is not a save event.
The completion fixture covers the final table with a seeded previous-round
counter, not a naturally played 35-match championship or every unlock outcome.

Cup knockout rounds count down 3..0. Original $85:B54E commits the semifinal,
reduces the round to zero and constructs the final pair. $85:B57D commits the
final through $85:DA5C, performs its post-match updates ($83:ECA0 and $85:C952),
and starts the victory sequence. This transient scene has callback zero and is
ineligible. After the sequence, the original Tournament Table choice uses
$8B:C8C0; A opens $85:D32E with state $1640=9 and the champion at $DDCE. The
verified fixture produced Brazil ($3C) in $DDCE. Completion is saved at this
final resumable bracket, after the original final updates, rather than during
the ceremony. The private fixture seeds late-stage entrants/counters using
original table pointers $8B:F583 (semifinals DDC8, final DDCC). It places the
selected team and three original draw entrants into private semifinal slots
and normalizes the backed-up $165E/$1660 selectors to that first slot; the cartridge
still calculates both finalists and writes the champion itself. This is not
a claim that every earlier match was naturally played or every unlock variant
was individually exercised.

## Health, stability and repeat suppression

Observer snapshots compare exactly $DC00..$DEFF, $1640..$1697, settings
$1E4A..$1E6F, and both raw team words. Two consecutive healthy completed frames
with identical progress are required. Unhealthy frames break the pending pair.
Health is captured before the host repairs the guest stack/NMI state.
Half-time and full-time statistics use $0070=$12 and are ineligible. Match,
intro and replay scenes are also ineligible. No callback/scratch heuristic
substitutes for the verified task callbacks.

The observer consumes an event even if the storage write later fails, avoiding
continuous retries. Reset/Continue consume an already eligible result as the
baseline. A new initial preparation rearms if committed team/settings/progress
changed; unchanged preparation does not repeatedly save. The actual Cup cancel
path before the first match was traced through team selection and back to the
original main menu ($A4:9D47), where $1648=0 rearms a fresh campaign.
Trusted successful password submission explicitly arms a new import;
that arm survives the original Password-to-campaign task transition, then is
consumed only after a verified checkpoint. Password editing itself is ineligible.

$0DA0/$0EA0 contain even team codes. The original World table enumerates
$0000,$0002,...,$0046: 36 teams. A valid raw code divided by two is the existing
mod team-name index; Brazil is raw $003C, index 30. Reject odd/out-of-range codes.

## Reproducible checks

`python -m pytest tests/test_campaign_transitions.py -q`

The unit observer test runs without cartridge assets. Setup fixtures replay the
original menu inputs and capture adjacent healthy frames. Production outcome
fixtures boot the actual game, create private test snapshots, shorten each
half's guest clock and seed a winning score, then run the original match-end,
statistics and campaign update routines. They assert that half-time/full-time
have not autosaved; committed results create generation 2; Continue restores
campaign bytes and does not resave. World completion additionally seeds the
verified backed-up prior-round count 34 before the original code commits 35,
then enters the final table and verifies its save and Continue. Cup additionally
executes the original regional and group constructors, seeds documented private
prior-stage counters and semifinal entrants, and lets original semifinal/final
code determine finalists and champion, then verifies completion save and Continue.
Test directories,
configuration and save roots are isolated from user saves.
