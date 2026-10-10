# Untouched Cup winning acceptance

A successful certificate requires one fresh process, all nine original Cup
matches, the original championship ceremony, and retained native checkpoints
that independently reproduce each Continue boundary. Elimination, a partial
qualifying run, a two-human match against an idle opponent, or a terminal label
alone does not satisfy this requirement.

The diagnostic driver in `tools/ghidra/campaign_probe_input.c` reads WRAM and
returns normal SNES pad inputs. It never writes guest state. The probe builds a
separate executable, replacing only input scripting and diagnostic host capture
and stop handling. Its manifest hashes all linked original objects and its
frozen driver source. The final run must have no passwords, state loads, memory
writes, accelerated match clock, score changes, stage changes, or gameplay/AI
changes. Menu selection of original teams and options is allowed.

## Controller investigation

Original assembly references are in
`deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm`.

* `$838000` maps configurable logical A/B/X/Y masks into controller actor slots
  `+16/+18/+1A/+1C`. `$83F12B` dispatches these separate actions.
* The original USA manual identifies A as long pass, B as short pass, X as shot,
  and Y as dash. [Konami manual scan](https://www.videogamemanual.com/snes/International%20Superstar%20Soccer%20Deluxe%20(USA).pdf).
* `$849497/$8494A5` compare the actor with `$00A6/$00A4` for ball ownership.
  `$00D8` is not a reliable possession test.
* Player 1's selected actor is `$1ACC`; actor pitch coordinates are `+2A/+2C`.
  Ball coordinates are `$042A/$042C`; team membership is actor `+9A`.
* The selected goalkeeper cannot carry a caught ball toward the opposite goal
  like an outfield player. Normal pass/high-ball input releases the ball.
* Unconditional B presses away from the ball trigger needless slide animations
  and impede pursuit. The diagnostic policy restricts tackles to nearby balls.

`controller.jsonl` records read-only actor, owner, ball and goal target samples;
`progression.jsonl` retains its original campaign schema. Each exploratory
folder contains its frozen source and build manifest. Incomplete and aborted
exploratory runs must not be presented as winning acceptance.

## Current status

The fresh `build/cup_control_nomash10` run completed nine original matches and
Brazil's championship in one process with zero loads or guest state writes.
Its strict `winning-certificate.json` verifies every fulltime epoch and all
twelve contiguous native checkpoint generations. The final score was 17–4;
original completion callback `$85D32E` settled at frame 608509 and published
generation 12, `Cup complete: Brazil`. Production Continue passed for isolated
copies of all twelve archived checkpoints, preserving native save bytes and
campaign tables, settings and teams. Evidence is in
`build/cup_control_nomash10-continue-all/verification.json`; the winning
certificate binds its exact twelve-case hash coverage and records
`production_continue_verified: true`. Production executable SHA-256:
`d9710b024189d2aaed556f2d4b0a89d8247a1affc491a9a364e396dd618a3433`.

Played scores were 10–12, 7–5, 34–5, 36–3, 13–4, 18–5, 20–3, 17–6, and 17–4.
This is a nine-match championship campaign with one qualifying loss. It is
not evidence of nine individual match wins. Earlier incomplete controller
probes remain exploratory.

The original qualifying-to-group transition can reuse the displayed-score
addresses for ranking/menu values. For example, the endurance08 exploratory
run played two qualifying draws, both 10–10, then showed menu values 1–0 at
stage 2. Its group constructor published an additional original setup
checkpoint at that stage. Neither menu values nor a fixed generation count
may be treated as match results. The strict certificate records goals from
original fulltime, binds each committed callback to its native checkpoint,
requires nine result stages, and retains every contiguous generation including
additional phase setup checkpoints.
The group-to-knockout transition can also publish two result checkpoints at
stage 5, first through `$85C37C` and then through `$85D32E`. These belong to the
same played match and both must be retained and checked through Continue.

`tools/ghidra/cup_controller_certificate.py` rejects incomplete evidence,
opponent championships, missing clocks or fulltime transitions, missing native
generations, and indecisive or inconsistent shootout ceremonies. The certificate
requires separate production Continue verification, which the final nomash10
certificate includes for all twelve checkpoints.

Two fresh controller probes reached a player championship after nine played
matches but stopped at the ordinary `$8BC8C0` final menu before publishing the
completion save. A diagnostic clone of the last native checkpoint confirmed
that ordinary A input advances this menu to `$85D32E` and publishes generation
12, `Cup complete: Brazil`. That loaded clone is exploratory evidence only.
The fresh `cup_control_nomash10` probe retains the frozen nomash09 gameplay
policy and advances the ordinary ceremony before stopping. Its passive monitor
archives every native generation from setup. No runtime change was required.

The controller terminal regression executes the C driver directly: normal
ceremonies and extra time continue ordinary A input; only an actual decisive
shootout with matching winner, champion and completion flag may stop at
`$8BC8C0`.
