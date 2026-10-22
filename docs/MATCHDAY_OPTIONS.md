# Matchday launcher and gameplay options

Interactive startup opens the original retro football broadcast launcher.
Choose Kick Off, Choose ROM, Presentation, Controls, Gameplay, Mods or Quit.
Use arrows and Enter/Escape, gamepad D-pad/stick and A/B, mouse or touch.
The desktop browser supports folders and Windows drives (navigate above a drive
root); Android uses its document picker. A supported clean USA ROM is required,
with or without a 512-byte copier header. Existing mod packs apply after validation.
`--no-launcher` starts directly; bounded/headless automation bypasses the launcher.

Gameplay settings persist in the normal configuration:

- `gameplay_bug_fixes`: includes the penalty timing-input correction. The original
  instruction at $8C970B used menu input $140E instead of the routed taker's
  input $140C. The correction uses the taker's input for both sides and leaves
  goalkeeper input $141A unchanged. It is gated to the original penalty states.
- `gameplay_short_goal_kicks` (default false): hold B at a human keeper's goal-kick
  release for the original grounded-pass action. Ordinary keeper kicks retain
  their behavior.
- `gameplay_final_attack` (default false): while a team controls the ball in the
  opposing penalty area at expiry, defer the whistle up to the existing
  600-tick expiry limit. Losing the attacking condition releases protection.
- `match_minutes`: Original (0), 1, 2, 3, 5, 7, 10, 15 or 20 minutes per regulation
  half for Exhibition, Cup and World Series. Extra-time periods use approximately
  one third, rounded up. Scenario/training clocks retain their original rules.
  The original cartridge duration selector remains intact.
- `team_strength` (default true): team selection shows attack, midfield, defence,
  overall and stars from the applied roster's starting eleven and formation.
  Skill nibbles 2–9 map to 0–99. Each sector averages four relevant skills;
  the goalkeeper contributes to defence; overall averages the three sectors.
  Star thresholds are 40, 65 and 85. Invalid formations suppress the summary.

Gameplay options and duration are part of save/password compatibility context.
Keep the same gameplay settings when resuming saved progress. The launcher
keeps existing in-game menus and full control remapping available.

Windows, Linux/SteamOS and Android compile from shared launcher sources.
Compilation and scripted input tests do not replace physical touch/controller,
Android document-provider or Steam Deck device acceptance.

## Validation (2026-10-21)

Windows Release, Linux Release and Android release (arm64-v8a and x86_64)
compiled. Final focused checks passed: six feature suites, ten integration checks,
and one additional compatibility-context check. The integration checks enter a
real Exhibition match and verify both 1- and 20-minute countdown initialization.
The original penalty instruction test reproduces the interference before
applying the correction for either shooter. A fresh independent source review
was completed and its actionable findings corrected. Physical device acceptance
and complete Cup/World Series playthroughs are still pending.
