# Exhibition shortcuts and presets

User intent: spend less time navigating menus, retain complete exhibition setups,
repeat a drill, and offer existing rules as Classic, Casual and Custom presets.
Balance changes require a reproducible input-origin scoring exploit.

Exhibition only: healthy frame, scene $0032=6 and preserved flags $DE07=0
(restored $1648 in the original menu scene). $1648 becomes player scratch
during play. Capture the original setup at $0070=15/$0072=0 with both flags
zero, before CODE_83B02C and later constructors
consume rules and assets. Preserve the entire runtime snapshot, including
teams, kits, weather, stadium, lineup and controller ownership. Capture kickoff
at the first healthy live frame after this setup. Rematch restores kickoff;
mark/restart drill stores a separate live exhibition checkpoint. Neither path
may roll back campaign progress or replace numbered saves.

Original rules are the default. Selected rules apply only at the witnessed
setup boundary or when explicitly starting the captured setup with new rules.
Classic uses verified retail defaults; Casual uses three minutes, easiest AI,
offsides/fouls/cards disabled, and no extra time. Custom exposes 3/5/7 minutes,
five difficulty levels and binary offside/foul/card/extra-time choices. Never
expose the hidden 45-minute duration or invent shooting/physics modifiers.

A single favorite persists the setup snapshot in the save root, with the
existing snapshot validation, ROM/mod/gameplay compatibility, integrity and
atomic publication. It has a dedicated path and does not touch campaign or
numbered saves. Playing it restores its saved rules; applying selected rules
is a separate explicit action. Resident kickoff/drill caches are invalidated
by external load, applied gameplay context changes and application restart.
Unavailable actions explain why; failed saves/restores do not claim success.

The Gameplay Tweaks page links to a scrolling Match Shortcuts page. Keyboard,
pad and touch navigation use existing overlay conventions. Successful restores
clear held controls and resume. Capture/restore must use full runtime snapshots
and the existing transactional restore, never selective WRAM resets.

Verification covers phase gating, rules ranges/defaults/persistence, independent
checkpoints, failure preservation, favorites across process restart and context
rejection. Retail-ROM acceptance exercises retained setup fields, original
kickoff initialization under presets, rematch/drill replay, and absence of
campaign writes. Without an authentic reproducible scoring exploit, document
the playtest protocol and leave goalkeeper/shooting balance unchanged.
