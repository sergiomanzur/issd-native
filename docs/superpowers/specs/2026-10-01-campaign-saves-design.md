# Campaign saves, Continue, and password interoperability

Date: 2026-10-01
Status: Proposed specification for user review; implementation has not started.

## Intended outcome

A player can close ISSD Native after a campaign checkpoint and later select
Continue to resume that campaign. A damaged or interrupted autosave must not
destroy the last usable checkpoint. Progress must not silently load against
different gameplay data. The native experience should eventually exchange
campaign progress with the cartridge's password system.

The agreed scope has two stages:

1. Reliable native campaign checkpoints, Continue, recovery, compatibility,
   and the snapshot correctness work they require.
2. Genuine cartridge password import/export, verified against the original
   game's implementation.

Stage 1 must be usable independently. Stage 2 must not be represented as shipped
until cartridge-compatible round trips pass. No online saves, account system,
cloud sync, or general-purpose campaign editor is included.

## Current evidence

- `ISSDNative/issd_save.c` exposes quicksave and eight manual slots. In the
  application, it delegates to `RtlSaveSnapshot`/`RtlLoadSnapshot`; its smaller
  WRAM/PPU fallback is exercised by native tests.
- Production snapshots contain the guest state and ISSD's extra execution
  state. The current runner writes snapshot version 8 and accepts versions
  4 through 8. The fallback ISSD slot's version 2 is a different format.
- Both save paths currently write directly to the final destination.
- `issd_save_get_info` checks magic and file modification time, rather than
  validating the full snapshot or matching the current gameplay data.
- The runner's snapshot loader deserializes into the live machine; a short
  read can fail after changing portions of that machine.
- `main.c` applies roster packs to a pristine ROM copy before boot and again
  during an in-process restart. The Mods page can change the pending selection
  before that selection has been applied.
- The original state machine uses WRAM `$32` and `$70`, plus per-screen task
  state. Existing bridge enum labels and some menu documentation do not fully
  describe the actual states. A generic change in `$70` is not sufficient
  evidence of a committed campaign outcome.
- The project suite has an existing save/load screenshot replay failure.
  Input/snapshot tests passing alone do not establish faithful continuation.
- The documented native autosave and password converter are absent. The
  cartridge's Password menu remains part of the original game.

## Stage 1: player experience

### Continue

Add an explicit Continue Campaign action to the native overlay, distinct from
Resume (closing the overlay) and Load State (a manual slot).

On a normal graphical launch, once the game and save context are initialized,
show the overlay with Continue selected if a valid compatible campaign
checkpoint is available. Do not automatically load it. With no eligible
checkpoint, launch the original game normally; opening the overlay still
shows Continue as unavailable with a reason.

The action shows the campaign type, team, checkpoint description, and local
save time where the original data supports those labels. Metadata must be
validated and decoded; do not invent a tournament round from an unverified
RAM offset. Existing roster names may supply the displayed team name.

Selecting Continue validates and restores the newest usable checkpoint for
the applied context, then closes the overlay and clears/suppresses held host
inputs. A recovery displays a short "Recovered previous autosave" notice.
If loading fails, keep the overlay open and explain the failure.

Headless runs do not open the startup overlay or restore automatically.
Provide an explicit `--continue` option for regression tests and intentional
headless continuation. It must fail clearly when no eligible checkpoint exists.

### Checkpoints

Autosave International Cup and World Series progress, using full snapshots.
Each context has one current campaign checkpoint and two previous generations.
Beginning a new eligible campaign replaces that context's current checkpoint
only when the new campaign reaches its first verified checkpoint; the older
generations remain recoverable. This stage does not introduce named campaign
profiles.

Eligible checkpoint events are:

- A completed campaign setup, after the game has committed the selected team,
  campaign structure, and settings.
- A completed match outcome, after the game has committed the result and
  updated the campaign's standings/bracket/progression.
- Completion of the campaign, after final progress and unlock changes have
  been committed, while an appropriate resumable results screen is available.

Detect the events using verified campaign and screen/task state, not elapsed
wall time or every menu transition. Trace the cartridge's flow and record
fixtures for Cup and World Series before enabling each checkpoint predicate.
Half-time, replays, attract demonstrations, exhibition matches, training,
password editing, and transient screen-loading states are not checkpoints.
Scenario progress persistence is outside Stage 1's campaign checkpoint scope.

Wait until the eligible state has completed a healthy simulation frame and
its progression data is stable across two completed frames. Save once for
that transition. Changes within an already checkpointed screen must not
produce repeated saves unless a separately verified progression event occurs.
If the screen exits too quickly to satisfy the predicate, do not guess or
bank a partial transition; fix the predicate using a cartridge-backed fixture.

A sanitized frame is not automatically healthy: the current frame runner can
clear NMI-busy state and adjust the stack after an incomplete frame. Capture
the real completion/health result before sanitization and exclude those
frames from checkpoint decisions.

Reset checkpoint observation after boot/reset, an applied-mod change, or a
load. Continuing must not immediately autosave the restored checkpoint again.
An I/O failure leaves previous generations intact, displays a brief failure
notice, and does not stall or continuously retry on every simulation frame.
A later verified checkpoint can try again.

Quitting does not manufacture a mid-match campaign checkpoint. Keep the last
safe checkpoint; players can use existing manual slots for exact mid-match
saves. Adjust quit labels so they do not imply campaign progress was saved
when only settings were written.

## Storage and compatibility

### Save location

Introduce a stable per-user save root: Windows app data, Linux/SteamOS XDG
data storage, and Android app-internal storage. Configuration and the save
root remain separate concerns. Provide an explicit save-directory override
for portable installations, native test harnesses, and headless fixtures.

Keep existing manual filenames and expose existing slots. When a manual file
is absent in the stable root, inspect the old working-directory `saves/`
location as a legacy source. Do not delete or silently overwrite legacy
files; copying/migration occurs only after a successful validated load/save.
Tests must set an isolated save root so they cannot read or replace user saves.

### Context identity

Derive a stable context identifier from:

- The normalized supported base ROM image.
- The effective ROM image after the actual ordered roster/gameplay patches
  have been applied.
- Host settings or overrides that actually change gameplay data/behavior,
  such as enabled debug cheats, using an explicit canonical representation.

Use SHA-256; the runner already provides `sha256_compute`.

Do not use only pack filenames, directory paths, or version labels. Editing
a pack in place must be detected if it changes gameplay. The pending Mods
selection is not the applied context. Recompute identity only after applying
the stack or an effective gameplay override.

Changes to fullscreen, audio device, filters, presentation FPS, controller
layout, photos/flags, and HD tile packs do not make campaign data incompatible.
Keep a separate snapshot schema/runtime compatibility identifier for changes
that cannot be handled by the loader; do not reject every source rebuild.

Store autosaves under their context identifier. Starting a campaign with a
different applied context must not overwrite another context's autosaves.
Returning to a previous compatible context makes its Continue available again.
Do not silently switch mods or load a different context as a fallback.

### Single-file envelope

Wrap new autosaves and newly written manual snapshots in a versioned envelope
containing the snapshot payload and its metadata in one file. A sidecar must
not be required to decide whether a save is valid.

The envelope includes format/runtime versions, bounded payload length,
context identity, a monotonic generation number, timestamp, checkpoint type,
validated display metadata, and an integrity digest covering both metadata
and payload. Encode fields explicitly with fixed widths and byte order;
do not persist a host C structure with ABI-dependent padding.

Read and validate bounds, exact file length, supported versions, digest, and
context identity before attempting deserialization. Unsupported future files
are rejected without mutation. Apply independent reasonable size limits so
an untrusted length cannot trigger excessive allocation.

Legacy raw runner snapshots remain available through explicit manual load.
Label them "Compatibility unknown" and require an in-app confirmation before
loading with the current applied context. They never become automatic Continue
candidates. Preserve supported runner versions rather than confusing them
with the fallback slot format. New manual saves use the same compatibility
and integrity protection as autosaves; quicksave and the eight numbered slots
remain separate from campaign generations.

### Writes and recovery

Write a complete candidate to a temporary file in the same directory, check
all writes and close errors, flush it durably, and validate its complete
envelope before publication. Publish with platform-correct atomic replacement.
On POSIX, sync the containing directory when publishing; on Windows use the
appropriate file flush and replace operations.

Preserve two prior valid generations without removing the sole good primary
before its replacement is committed. Interrupted rotation must leave at least
one complete previous checkpoint. Never promote a corrupted primary into a
backup or treat an unfinished temporary file as committed progress.

For Continue, inspect the primary and backups in the current context and
select the newest fully valid compatible generation. If the primary is
damaged, recover automatically from a valid backup and report that recovery.
If none is usable, show "No recoverable campaign save" and retain the files
for diagnosis. Generation order comes from validated metadata, not file mtimes.

## Transactional snapshot loading

Envelope validation catches storage corruption but does not by itself make
deserialization transactional. A valid-looking payload can still contain
unsupported or structurally inconsistent snapshot data.

Preflight deserialization through a validation/staging path that does not
write to the live machine or call the game reconciliation hook. Commit only
after all guest and ISSD extra-state chunks are accepted. If the existing
serializer requires a live commit pass, keep an in-memory rollback snapshot
and restore it on any commit error before returning control to gameplay.

The no-mutation guarantee covers CPU, RAM, PPU, audio, controller/multitap
state, ISSD task contexts, and host resume bookkeeping. Protect audio with
the existing lock/pause boundaries. Reconcile guest execution state and
reset presentation history only after successful restoration. No partial
load may become the source of a new autosave.

The existing save/load replay mismatch is part of this work where it affects
continuation. Diagnose the first divergence using guest/host state and
rendered frames; do not weaken the screenshot test or accept a frozen image
as proof. A restored campaign must advance identically under identical input
through the next checkpoint.

## Integration boundaries

- Extend save management in `issd_save.[ch]`; separate envelope/file handling
  from campaign predicates if it would otherwise obscure either responsibility.
- `main.c` supplies the applied context, frame health, checkpoint observations,
  startup Continue behavior, and CLI/save-root overrides.
- `issd_menu.[ch]` supplies Continue, status/error messages, and explicit legacy
  load confirmation, with keyboard, gamepad, and touch navigation.
- The runner's snapshot APIs gain validation/transaction guarantees required
  by ISSD; preserve existing callers and raw snapshot formats.
- Shared source lists and Android builds include any new production modules.
- Tests use an explicit temporary save root and production serialization/file
  paths. Update README, changelog, and save documentation only for implemented
  and verified behavior.

## Stage 2: cartridge password bridge

Identify and verify the original encoder, decoder, alphabet, field packing,
validity checks, and restore path using the supported ROM. Native imports
must reproduce the cartridge's accepted campaign state, rather than guessing
fields from menu documentation.

Add native Import Password and Export Password actions with keyboard/controller
and Android touch access. Follow the verified alphabet, grouping, and length.
Accept harmless formatting differences, but reject invalid symbols,
unsupported states, and failed cartridge validity checks without mutation.

Import restores through the verified original password restore flow, reaches
a verified campaign checkpoint, and persists that checkpoint using Stage 1's
safe save path. Export produces a password accepted by the original cartridge
for the same supported campaign progress. Passwords are not arbitrary frame
snapshots and cannot promise mid-match restoration or unsupported modes.

Retail cartridge passwords cannot describe arbitrary native rosters or added
teams. Offer retail import/export only in a context whose campaign gameplay
data matches the supported cartridge. Explain why it is unavailable in a
modified context rather than silently remapping a team. Presentation-only
changes remain allowed.

Verify with cartridge-generated fixtures and native/cartridge round trips for
both campaign types, multiple teams and progression stages, and corrupted
passwords. If investigation proves a documented campaign/state cannot be
represented, expose that verified restriction and never claim support for it.

## Acceptance checks

Stage 1 is complete only when:

1. A freshly launched graphical application offers Continue for a compatible
   checkpoint; selecting it restores and advances a real campaign.
2. Cup and World Series setup and post-result fixtures save exactly once at
   verified safe points, with committed progression preserved across restart.
3. Transient menus, half-time, replays, demos, and unhealthy frames produce
   no campaign saves. Manual loads/resets do not trigger spurious autosaves.
4. Truncation, bit corruption, metadata edits, future versions, and oversized
   lengths are rejected before live state changes.
5. Fault injection at writing, flushing, rotation, and replacement leaves a
   recoverable valid generation. A damaged primary recovers from a backup;
   three unusable generations fail clearly without deleting user data.
6. Incompatible gameplay/roster changes reject loading. Same-name edited packs
   are detected; presentation changes and equivalent effective ROM data remain
   compatible. Pending unapplied mod changes do not alter the active identity.
7. Separate contexts preserve their own progress; explicit legacy loading
   works with a compatibility warning and preserves the original files.
8. Failed commit/deserialization leaves guest state and host execution state
   unchanged. Successful loads clear held controls and resume healthy frames.
9. Existing snapshot/manual-slot/multiplayer checks and deterministic campaign
   replay checks pass. Remaining unrelated baseline failures are reported
   explicitly rather than described as a fully passing project suite.
10. Windows builds pass, Linux/Android builds are exercised where their
    toolchains are available, and device-only validation gaps are recorded.

Stage 2 is complete only when accepted cartridge fixtures round-trip in both
directions, invalid imports leave state unchanged, eligible imports produce a
native checkpoint, and modified campaign contexts cannot masquerade as retail
cartridge progress.

## Design review

The specification preserves manual saves and the original gameplay/password
menus, protects progress across mod contexts, and distinguishes verified
campaign checkpoints from arbitrary snapshots. Stage 1 owns the reliability
work; Stage 2 depends on it and owns password-specific investigation and UI.
No implementation status is inferred from the existing enhancement documents.
