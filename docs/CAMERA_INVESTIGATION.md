# Camera options investigated (2026-10-10)

The current field renderer uses a fixed 2D shear projection, not a perspective
camera. Its coordinate mapping is `x = u + v - camera_x`,
`y = v - camera_y`. This is also the mapping used by the compiled stadium
preview in `tools/mod_studio/stadium_geometry.py`. Changing the output aspect
ratio with True widescreen widens the horizontal view; it does not rotate the
pitch or change its projection.

Original `$83CFBD` / `$83CFCB` / `$83CFE5` update screen coordinates with camera
deltas `$A0/$A2` and apply the native admission window. `$80DCA9` recovers the
pitch coordinates from those screen coordinates. `$8BAA5F` stores the recorded
BG2 camera and `$8BAE80` restores that recording during replay. Stadium profiles
retain verified template camera envelopes (`min_y`, `max_y`, shear anchors,
`max_x_cap`); the current validator deliberately rejects unsupported bounds.

## Feasibility

| Option | Assessment |
| --- | --- |
| Wider horizontal field of view | Already supplied by True widescreen. The reconstructed map ring currently caps at 504 logical pixels (21:9) so every scroll phase fits. |
| Zoom in | Feasible as a presentation crop and scale. It shows less field and reduces effective sprite detail; HUD should be composited independently. Not implemented. |
| Zoom out / tactical overview | Implemented for verified pitch layouts as Tactical 80% and Tactical Wide 67%. Direct metatile sampling reveals additional field in both axes; signed actor rendering and independent HUD composition accompany it. See the 1.0 validation report for remaining gates. |
| Ball/player tracking, smoother follow or fixed framing | Feasible within the same projection, using a separate presentation camera and translating all field layers and objects consistently. New areas need valid recorded actors in replay; scene cuts and tracking limits need dedicated tests. Not implemented. |
| Rotated, behind-goal or overhead camera | The existing sprites and preprojected tile artwork cannot supply those angles. This would require a new renderer plus compatible artwork, rather than a camera setting. |

The tactical cameras preserve the original tracking and projection. They are
presentation settings, with original scanout retained for unsupported scenes.
See [tactical validation](TACTICAL_CAMERA_1_0_VALIDATION.md).

Private Ghidra record/decoder export:
`build/ghidra-investigation/widescreen-replay-evidence.txt`. Its addresses and
bytes were checked against the owned retail cartridge. No cartridge artwork,
raw RAM or private saves are published with this document.
