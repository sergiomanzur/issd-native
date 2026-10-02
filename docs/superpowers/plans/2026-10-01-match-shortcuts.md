# Match shortcuts implementation plan

**Goal:** Safe exhibition rematch/drill checkpoints, a persistent favorite and
original-option rule presets, with reproducible acceptance evidence.
**Architecture:** Full snapshot caches and the verified pre-constructor phase;
dedicated context-checked favorite storage; overlay page and persisted rules.
**Tech stack:** C, existing runtime/save system, SDL2, pytest/native harnesses.
**Spec:** ../specs/2026-10-01-match-shortcuts-design.md

Constraints: preserve existing defaults, use no private ROM/captures in Git,
do not alter campaigns or shooting physics without evidence. No commit/push
or new release without a user request for this new work.

- [x] Favorite storage: add failing integrity/context/publication tests, then
  issd_save_match_favorite(data,size,label),
  issd_save_read_match_favorite(void **data,size_t *size), and
  issd_save_match_favorite_info(out,size) in issd_save.[ch]. Output memory is
  caller-owned and only returned after complete validation.
- [x] Rules/config: persist Original/Classic/Casual/Custom selection and bounded
  custom values. Test legacy defaults, malformed/range handling and roundtrip.
- [x] Match runtime: tests first for preconstructor capture, healthy/exhibition
  gating, kickoff/drill independence, repeated restore, external/context reset,
  failed capture/restore preservation. Add issd_match.[ch] and host hooks using
  memory snapshot APIs. Observe real preconstructor phase in a ROM-backed run.
- [x] Overlay: add Match Shortcuts page under Gameplay Tweaks, scroll/click,
  selected-rule editor, guarded actions and clear errors; test actual menu paths.
- [x] Acceptance: headless original kickoff under default/Classic/Casual/custom,
  same-setup rematch, drill replay, favorite persistence and incompatibility;
  report input-origin balance probe evidence and remaining playtesting.
- [x] Integration: run relevant regression checks, build Windows/Linux/Android,
  inspect UI and update supported features, controls/gameplay docs and README.

Ownership: save worker owns issd_save.[ch] and favorite storage tests; root owns
all other production/config/menu changes and integration. Investigation agents
are read-only. Review focuses on stale checkpoints, campaign admission,
incompatible favorites, rule-derived assets/timers, and held input after restore.

Verification: full suite 204 passed / 2 existing failures / 2 skipped / 7 subtests (890.47s). All eight retail exhibition cases passed, including exact WRAM replay, native clocks/CPU levels, favorite persistence/compatibility and rule reinitialization. Final UI/model/mod-stack regression 3 passed; final Windows favorite smoke passed. Windows/Linux/Android builds and visual inspection succeeded. Review fixes cover unsafe scenes, completed-frame admission, recapture rollback and visible paused errors. No verified scoring exploit; balance unchanged with a reproducible playtest protocol.
