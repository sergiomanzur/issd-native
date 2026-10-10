# Focused ISSD Ghidra investigations

These tools produce local diagnostic evidence for the supported retail USA ROM.
They do not alter production sources, game data, configuration, or user saves.
ROM-derived listings, Ghidra databases, PCM, and snapshots belong under ignored
`build/ghidra-investigation/`, never in a release or Git commit.

## Ghidra setup and evidence export

Verified locally with Ghidra **12.1.3**, Android Studio's **JBR/JDK 25.0.2**, and
[ghidra-snes v1.3.0](https://github.com/joshleaves/ghidra-snes/releases/tag/v1.3.0).
The extension archive SHA-256 observed here is
`40265aeca0785f56437e40983e22ff209cb10ab31f75323fe696d54e21322cd3`.
This records the downloaded artifact, not an independently signed upstream checksum.

Extract `ghidra_snes-v1.3.0.zip` so that
`build/ghidra-investigation/extension/ghidra-snes/lib/ghidra-snes.jar` exists.
The release declares Ghidra 12.0.4; this workflow loads it as a build-local external
module. Its loader and focused 65816 decoding were exercised on 12.1.3, not every
extension/UI feature. The wrapper removes two unsupported manifest lines from
this local extension copy. No global installation or Ghidra preferences change
is needed for processor registration.

```powershell
& tools/ghidra/investigate.ps1 `
  -GhidraHome 'D:/Downloads/ghidra_12.1.3_PUBLIC_20260817/ghidra_12.1.3_PUBLIC' `
  -JavaHome 'C:/Program Files/Android/Android Studio/jbr'
```

This checks the ROM hash, imports with the SNES LoROM loader, and retains project
`build/ghidra-investigation/ISSD.gpr`. The simplified local ROM basename avoids
a quoting failure in Ghidra's Windows import launcher. It exports targeted
instruction bytes and tables to `rom-evidence.txt`. Pass `-Ranges` to select
other intervals: `start:end`, `start:end:data`, or `start:end:m1x1`.

Ranges enter native 16-bit accumulator/index mode unless `m1x1` is specified.
REP/SEP propagate within linear decoding; register modes restored by PLP/RTI
or across calls require explicit context review. Split embedded jump tables into
`:data` intervals. Labels come from the existing assembly, not newly inferred
symbols. The script does not claim a whole-ROM call graph or reliable recovered C.
It writes a completion marker; the wrapper rejects stale/incomplete exports.

## Audio evidence from production objects

```powershell
cmake --build build --parallel 4
python tools/ghidra/build_audio_probe.py
$env:ISSD_AUDIO_EXPORT = "$PWD/build/ghidra-investigation/audio-probe/capture"
python tests/test_audio_realtime.py `
  --exe build/ghidra-investigation/audio-probe/ISSDNative.exe `
  --device --config default --frames 1200
python tools/ghidra/summarize_audio.py build/ghidra-investigation/audio-probe/capture
Remove-Item Env:ISSD_AUDIO_EXPORT
```

The helper copies `audio_trace.c` locally, registers one exit callback during
the first stats initialization, and relinks with all other production objects.
Export runs after the main loop and audio producers stop; it reads the existing
private rings directly because the SDL mutex has already been destroyed.
The production executable is preserved. No active gameplay logging hooks or
debug server are added. This is a diagnostic executable, not a release candidate.

The exporter writes `*-stats.json` and `*-events.jsonl`; the summary records the
retained event interval and drop bursts near port traffic. The ring holds 524,288
events: an entire run may not fit. A preceding port frame is a temporal association,
not proof of an exact calling routine. `dropped_audible` means native pairs with
signal above the runtime threshold; it is not a listener assessment or comparison
against an independent original-game PCM capture.

`--device` opens the default sound device muted. Software video is invisible.
Run device timing baselines without other simulation/build workloads; retain
loaded-machine results only as diagnostic observations.

## Campaign checks

```powershell
$env:ISSD_RUN_FULL_WORLD = '1'
python -m pytest tests/test_campaign_full_acceptance.py `
  tests/test_game_acceptance.py tests/test_campaign_integration.py `
  tests/test_campaign_transitions.py -q -s `
  --basetemp=build/ghidra-investigation/campaign-runs
Remove-Item Env:ISSD_RUN_FULL_WORLD
```

The Cup/World checks use original entrants/progression and a new-process Continue
after each result, but shorten test-owned clocks and set scores. They certify
these accelerated flows, not complete untouched human campaigns.

`python tools/ghidra/probe_natural_match.py` additionally starts a fresh Cup match
and uses only real input, without clock/score edits. It saves/reloads between
bounded chunks, records sampled state, and reports whether a result checkpoint
was reached. It requires a fresh `build/ghidra-investigation/natural-match` directory.
If the frame bound expires, that is an incomplete probe, not evidence of a game hang.
The default bound is 120 chunks. `--resume --chunks 120` continues a previous
bounded probe from its last untouched snapshot and appends the progression record.

For a fresh one-process tournament with no snapshot reloads or clock/score edits:

```powershell
python tools/ghidra/probe_uninterrupted_campaign.py world --folder build/campaign-uninterrupted/world-new
# Run separately while the probe is active; archives published checkpoints only.
python tools/ghidra/monitor_campaign_checkpoints.py build/campaign-uninterrupted/world-new
# After terminal completion, Continue from private copies using production code.
python tools/ghidra/check_continuous_checkpoints.py world build/campaign-uninterrupted/world-new build/campaign-uninterrupted/world-new-continue --expect-world-complete
```

Use `cup` for Cup probing. A naturally eliminated Cup verifies its exit path,
not championship. Run folders must be new, and probe runtime is bounded to four
hours. The diagnostic retains frozen input/main sources and linked-object hashes;
only host input, periodic capture suppression and host stopping differ from the
production executable. PPU/APU execution is retained. A frame limit or partial
round history cannot produce a successful completion certificate. See
[continuous campaign acceptance](../../docs/CONTINUOUS_CAMPAIGN_ACCEPTANCE.md).
