# Audio transfer timing fix

The Ghidra investigation reproduced signal-bearing PCM loss during original
sound-transfer traffic. An isolated 1,200-frame baseline discarded 6,360 native
stereo pairs above the runtime signal threshold (about 0.198 seconds distributed
across the run), with one callback underflow.

## Causes and changes

1. CPU-derived within-frame timestamps could exceed the next frame boundary
   during uploads. Starting the following frame then moved guest time backwards
   and caused port timing to rebase. Credit is now bounded to the current frame;
   blocking SPC acknowledgement execution remains independent.
2. Startup waits expired before the original driver's 75-tick readiness delay.
   The wait budget now permits that real acknowledgement, avoiding the observed
   fabricated boot echo. A real-SPC regression catches false success.
3. Uploads produced PCM faster than the device could consume it. Realtime port
   writes now wait for FIFO room with the consumer mutex released. PCM is not
   trimmed, replaced or hidden from the counters. The device callback continues
   to consume only; it does not become an emulation clock.

Waiting is disabled for absent/paused consumers and fast-forward. A stalled
device gets one bounded 250 ms wait; subsequent writes bypass waiting until
consumption advances. Review identified that repeated timeouts per word could
freeze an upload, and a failing regression verified the correction.

No snapshot schema changed. Existing user edits were preserved. Diagnostic
executables, Ghidra data and captures remain under ignored
`build/ghidra-investigation/`.

## Measurements

Two sequential device captures after producer pacing was integrated, with no
campaign simulations running:

| Capture | Produced stereo pairs | Signal-bearing drops | Underflows |
| --- | ---: | ---: | ---: |
| Default profile, 1,200 frames | 916,903 | 0 | 0 |
| Minimal profile, 3,600 frames | 2,226,724 | 0 | 0 |

Full exit counters include 164,197 and 164,821 below-threshold drops respectively,
mostly startup silence before the output device is available. These are retained
in the diagnostics and are not represented as zero total drops.

The production device regression failed before pacing with 7,138 signal-bearing
drops and passed after pacing. Regression coverage also includes clock monotonicity,
real startup acknowledgements, cleared input latches, word echoes, PCM output,
mutex release while waiting, tick wraparound, device stalls and turbo bypass.
Focused checks passed **7 tests and 8 subtests**. The final-build production
device regression passed in 33.67 seconds. Production executable SHA-256:
`7ed1da9f07dd73062c38a172c389e85129255644c4faaac928cc98f0132b85fe`.
The full project run (`python -m pytest tests -q`) completed in 2,316.99 seconds:
**285 passed, 8 skipped, 8 subtests passed, and one failed**. The failure was
`test_bugfix_context.py::test_bugfix_setting_refreshes_save_context`: its C
harness still defined `issd_match_reset`, whereas the earlier match-control
changes call `issd_match_reset_context`. Only that harness stub was renamed;
gameplay behavior was not changed to satisfy the test. The corrected targeted
test passed, and `python -m pytest tests --lf --last-failed-no-failures none -q`
then passed the sole failed case in 2.98 seconds. The complete suite was not
repeated after this isolated test-harness correction.

Independent review found no remaining actionable correctness issue in the
audio changes. The final executable hash above was verified after validation.

Reproduce the muted hardware-device regression on an otherwise idle machine:

```powershell
$env:ISSD_RUN_AUDIO_TIMING = '1'
python -m pytest tests/test_audio_transfer_timing.py -q
Remove-Item Env:ISSD_RUN_AUDIO_TIMING
```

Normal `python -m pytest tests -q` skips this device-dependent check.

These results resolve the reproduced transfer-loss case. They do not establish
complete perceptual fidelity against an independent original-game audio capture.
A single unusually long/unresponsive acknowledgement still has a bounded locked
SPC execution window; the tested retail transfers do not exhibit signal loss in
that window.
