# Continuous campaign acceptance (2026-10-09)

This run separates one-process original campaign execution from the earlier
accelerated nine-match Cup and 35-match World Series checks. The accelerated
campaign/game suite passed 19 tests in 1,837.03 seconds, including new-process
Continue after every committed result; those tests shorten clocks and set scores.

## Continuous method

`tools/ghidra/probe_uninterrupted_campaign.py` builds an isolated diagnostic
executable from the configured production objects. Only the input-script object
and main-loop diagnostic capture/stop object are replaced. Original guest CPU,
PPU, APU and interrupt execution remain active. Inputs start fresh tournaments
through the original menus; there are no passwords, snapshot loads, clock/score
edits or other guest-memory writes. The frame counter advances continuously in
one process. SDL video/audio use dummy devices, so this is game-flow evidence,
not physical presentation or listening acceptance.

Each run retains the actual compiled source copies, input/main hashes,
diagnostic executable hash, linked-object hashes, sampled progression and native
checkpoint checksum/generation. The input policy is diagnostic automation and
is not evidence of human competitive play. The original tournament constructor
uses five-minute periods despite the menu's selected three-minute option.

`monitor_campaign_checkpoints.py` passively archives published valid envelopes.
It never loads or changes the running campaign. Afterward,
`check_continuous_checkpoints.py` copies each archived envelope into its own
save root and uses the production executable's Continue in a separate process.
It checks original tournament tables, progress fields, settings and both team IDs
against the WRAM embedded in the saved snapshot, plus byte-identical stored envelopes.
These clone checks do not interrupt the continuous run.

## Cup: natural qualifying elimination verified

`build/campaign-uninterrupted/cup-03` completed two naturally timed qualifying
matches and the original elimination/return path in **108,455 frames**, without
reloads or guest state edits. Brazil lost 0–11 and 0–9. The original elimination
callback `0x8BC1F6` accepts START and returns to boot mode 0. Native generation
1 (setup) advanced to 2 and 3 for the two committed results.

All three archived envelopes passed separate Continue checks under
`build/campaign-uninterrupted/cup-03-continue-full`: original stage/round 0/0, 1/1 and
2/2 restored, with no resave. The qualification-table screenshot shows both
losses. This certifies early elimination; it does **not** certify an untouched
winning nine-match Cup. Earlier manually stopped diagnostic iterations are
retained as investigation artifacts and are not complete acceptance runs.

## World Series

The fresh run under `build/campaign-uninterrupted/world-03` completed all
**35 original matches in 1,921,751 guest frames**, in one process without
snapshot loads, passwords or clock/score/state edits. Every opponent in the
original 36-team roster except Brazil appeared exactly once. The trace records
all 35 full-time scores and committed rounds 1–35. Brazil lost all 35 matches;
this verifies progression and persistence, not competitive play or winning.

The original final table uses callback `0x8B94EE`, mode 12 and round 35. Native
generation 36 contains the final result; generation 37 is labeled
`World Series complete: Brazil`. The current strict terminal validator and
independent complete-opponent check passed. Their evidence is retained in
`validated-terminal.json`; the process exited cleanly without interpreter caps.

All **36 archived envelopes**, generations 2–37, passed production Continue
under `build/campaign-uninterrupted/world-03-continue-final`: all 35 distinct
result rounds and completion. Original tables, progress, settings and both team
IDs matched the WRAM in each native snapshot. Continue kept every campaign
envelope byte-identical. The final restored screenshot displays the original
35th-game standings, and the completion callback remains correct.

The monitor began after setup, so generation 1 is not part of this archive.
Fresh setup itself ran through the original menus; setup/Continue has separate
automated coverage. These checks do not certify every manual-device step in the
larger release checklist.

Frozen input driver SHA-256:
`6acf868bb383d66d35cf603ab874acdca849ea2ac58d7ab9ac542ad9b86f9cad`.
Diagnostic executable SHA-256:
`d1aa901493018b7934d39e9b75148d9e781e96fd1dd00f425411ea894266c620`.
The diagnostic input policy differs from the later Cup probe; each manifest
records its actual compiled source, rather than the subsequently edited helper.

## Build and scope

Production Windows executable SHA-256:
`8d9a0fbe558d49a47e4c3c0e183c2209aac01efbadfb54dd94452454014a51c4`.
Private headerless USA ROM SHA-256:
`d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9`.
ROM images and save envelopes remain in ignored owned build directories.

Full untouched winning Cup, alternate teams/settings, human-controlled competitive
play, physical controllers, audible timing and Android lifecycle remain separate
release checks. Completed ordinary/sudden-death shootout and seven expanded
substitution checks are recorded in [shootout acceptance](SHOOTOUT_ACCEPTANCE.md)
and [substitution acceptance](SUBSTITUTION_FLOW_ACCEPTANCE.md).
