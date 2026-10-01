# Campaign saves and Continue

The current source provides native campaign checkpoints for International Cup
and World Series, alongside quicksave and eight manual state slots. These are
local saves; no account or online service is required.

## Continuing a campaign

On graphical launch, the overlay opens with **Continue Campaign** selected when
a compatible checkpoint exists. Select it to resume. The original game starts
normally when no checkpoint is available. Opening the overlay always exposes
Continue, including its availability and checkpoint date.

Continue checks the current checkpoint and two previous generations. If the
newest file is damaged or cannot be restored, it selects the newest usable
backup and displays **Recovered previous autosave**. It keeps the overlay open
and explains the error if none can be restored. Held host controls are cleared
or suppressed until released after loading.

Autosaving requires a verified campaign transition, a healthy completed guest
frame, and progression data stable across two completed frames. Half-time,
replays, exhibition matches, demonstrations, password editing, and transient
screen loading are excluded. Quitting keeps the last safe checkpoint. Use a
manual state to preserve an exact position during a match.

Checkpoints cover initial campaign setup, committed Cup group and knockout
results, World Series results, and settled completion tables for both modes.
Accepted native password imports create a checkpoint after the original game
restores the campaign. [Cartridge flow evidence](CAMPAIGN_CARTRIDGE_FLOW.md)
records the observed transitions and regression fixtures.

A write failure displays an error and preserves previous generations. It does
not retry on every frame; a later eligible transition can save again.

## Save locations

The two **Gameplay Tweaks** switches are part of save compatibility. Each
enabled combination gets its own campaign context; switch back to the saved
combination before using Continue or loading a manual state. See
[Gameplay Tweaks](GAMEPLAY_TWEAKS.md).

| Platform | Default root |
| --- | --- |
| Windows | `%LOCALAPPDATA%/ISSDNative/saves` (falls back to `%APPDATA%`) |
| Linux / SteamOS | `$XDG_DATA_HOME/issd-native/saves`, or `~/.local/share/issd-native/saves` |
| Android | App-internal `saves` directory |

Use `--save-dir <directory>` for a portable installation or isolated tests.
Quicksave is `quicksave.sav`; numbered slots are `slot_0.sav` through
`slot_7.sav`. Campaign generations are under
`campaigns/<gameplay-context>/campaign.sav`, `campaign.1.sav`, and
`campaign.2.sav`. The zero-length `campaign.sav.lock` file is intentional; the
operating system holds and releases its lock, including after a process exits.

New files contain one checked envelope with metadata and the full snapshot.
Lengths, versions, SHA-256 integrity, context, and snapshot structure are
validated before changing the running game. Files publish through flushed
temporary files and atomic replacement. Unfinished temporary files are never
offered as Continue.

## Mods and compatibility

The save context hashes the supported base ROM, the effective patched ROM, and
gameplay overrides such as debug cheats. An edited pack with the same filename
is detected if its applied gameplay bytes differ. Each context has a separate
campaign history. Returning to the same gameplay configuration makes its
Continue available again.

Pending mod selections do not affect the current context until applied.
Fullscreen, filters, audio settings, presentation FPS, controller layout,
photos, flags, and background tile packs do not invalidate campaign progress.
Continue never switches mods automatically. Manual states with incompatible
gameplay data fail with an explanation.

## Existing states

If a manual slot is absent in the new root, the loader also inspects the old
working-directory `saves/` folder. Raw legacy states are marked **Compatibility
unknown** and require confirmation. They never enable Continue. In the overlay,
confirm Load State twice; F6 requires a second press. Ctrl+1–8 opens the overlay
confirmation for a legacy slot. Headless callers must opt in with
`--allow-legacy-save`. Legacy originals are preserved.

The runner accepts guest snapshot versions 4–8. Older files omit active native
CPU and timing data, so exact continuation cannot be certified for those files.
New snapshots preserve that data explicitly. Advancing replay tests compare
every frame and complete WRAM for title, classic live play, and widescreen live
play.

## Headless use

```powershell
.\build\ISSDNative.exe --rom "path/to/game.sfc" --save-dir "portable-saves" --headless 120 --continue
```

Headless execution never opens or automatically loads Continue. The explicit
option fails if no usable campaign checkpoint exists.

## Password interoperability

Open the overlay's **Password** page to enter a password with the 64-symbol
palette, gamepad, touch, or keyboard. Text input preserves case; whitespace is
ignored. **Import** requires the original cartridge Password screen to be
settled underneath the overlay. **Export** requires a settled Cup or World
Series checkpoint. Invalid codes leave campaign progress unchanged.

The bridge uses the supported cartridge's original packing, checksum, and
restore routines. All six formats are supported, with lengths of 8, 11, 12,
15, 39, or 50 symbols. Gameplay patches and debug cheats disable the bridge;
presentation changes remain compatible. Passwords do not represent arbitrary
mid-match states or custom rosters. See [the password guide](PASSWORD_BRIDGE.md)
for the alphabet, exact restrictions, and developer round-trip tools.

## Verification

Run the storage, cartridge transitions, production integration, and snapshot
checks with:

```powershell
python -m pytest tests/test_campaign_saves.py tests/test_campaign_transitions.py tests/test_campaign_integration.py tests/test_password_codec.py tests/test_password_flow.py tests/test_password_ui.py tests/test_snapshot_transactional.py tests/test_savestate_replay.py -q
```

ROM-backed tests need the user's supported dump and a rebuilt executable. All
tests use isolated save directories. Storage fixtures exercise corruption,
context changes, recovery, failed publication, and concurrent writers.
Transactional fixtures require a failed load to preserve live state, including
unexpected failures during commit. Device playtests remain necessary.
