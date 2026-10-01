# Controls and profiles

Open the overlay with **Escape/F1**, gamepad **Guide/Home** or **Back+Start**,
then choose **Controls / Profiles**. This guide describes current source;
per-player remapping and touch placement are not in the published v0.2.0-beta.1.

Select P1–P4 with Left/Right on **Player**. Every slot stores its own preset,
native SNES action bindings, stick deadzone and trigger threshold. Choose
Classic, FIFA or PES, or select an action and press A/Enter to capture a new
physical button, left-stick direction or trigger. Release the confirming button
first. Only the selected player's controller can supply the new binding;
Escape cancels. The custom binding replaces the action's previous sources.
Presets restore their built-in aliases. **Reset this player** restores Classic
and the original thresholds without changing other players.

Deadzones range from 0 to 30,000 on SDL's 32,767-unit axis scale (about 0–91%).
Default 12,000 is about 36%. These are thresholds for digital cartridge inputs;
they do not add analog movement, radial response curves or right-stick actions.
Left-stick and trigger thresholds are independent. Displayed percentages are
rounded down. Opposite directions cancel; overlapping sources remain active
until all relevant sources release. Profiles follow P1–P4 slots across sessions,
not controller GUIDs. Connecting controllers in a different order changes which
physical controller receives a saved profile.

Overlay navigation remains D-pad, A/Enter, B/Back/Escape regardless of gameplay
bindings. Guide and the Back+Start chord stay reserved for overlay access.
Changes are saved immediately. Controls held while paused, changing bindings or
regaining focus must be released before affecting gameplay.

## Keyboard

**P1 Keyboard** captures a key for each native action. Escape cancels; fixed
host shortcuts (F1–F11, Tab and 1–8) cannot be assigned. Arrow/WASD navigation
and overlay confirm/cancel stay fixed while the overlay is open. Reset restores
the original keyboard keys. Keyboard and touch feed P1; independent P2–P4 play
requires separate gamepads.

Default action keys retain their documented secondary aliases. Remapping an
action disables that action's legacy aliases. Keyboard sampling uses the saved
configuration and checks scancode bounds; it is independent of the pad presets.

## Touch layout

**Touch Layout** selects each D-pad, action, shoulder, Start/Select, Hide and
Menu control. Adjust horizontal/vertical centers on a 0–1000 viewport scale and
size from 50–200% of its automatic size. `-1` uses automatic placement. Positions
and bounds are recomputed after rotation; controls stay within the viewport.
Reset the selected control or the whole layout to restore defaults. Changes
clear held touches. Desktop can edit the saved layout, but touch rendering is
enabled by default only on Android.

Sizes/positions are per control, rather than a drag-and-drop editor. Users can
choose overlapping placements; Menu/Hide hit-testing has priority. Keep menu
access and thumb reach in mind when placing controls. Device ergonomics still
need testing on different screen sizes.

## Background and resume

Background events pause the resident game and audio, clear controls and save
settings. Foreground completion returns to the paused overlay. Release controls,
then choose Resume. If Android kills the process, resident state is lost;
Continue restores the most recent compatible safe campaign checkpoint. This is
not an arbitrary mid-match autosave or a certified device lifecycle result.

See [supported behavior](SUPPORTED_FEATURES.md) and [acceptance checks](ACCEPTANCE_TESTS.md)
for test evidence and remaining physical-device checks.
