# Per-Player Looks: Fitter, Keeper Parity, Studio Preview (issue #4 follow-up)

**Date:** 2026-11-20
**Status:** approved (the controller's recommendation was accepted verbatim by the repo owner: "re-open it and proceed with your recommendation")
**Issue:** sergiomanzur/issd-native#4 (reopened) — the reporter (Poligraf) needs per-player appearance for a 19,000-player LLM pipeline; he contributed a patch (`issd-skin-hair-color.patch`, dissected 2026-11-20) whose authoring layer we are adopting and whose runtime layer we are NOT (it duplicates v0.7.0's `skin_tones` mechanism with colliding allocators and a wrong keeper writer).

## Intent

Give mod authors (and Poligraf's pipeline) a named per-player look vocabulary — `{"skin": "light|dark", "hair_color": "black|brown|blond|red|grey", "hair_style": 0-13}` — and an OFFLINE fitter that converts it into the format the v0.7.0 loader already accepts (`skin_tones` + per-player `skin_tone` + `hair_style`), plus the three gaps the patch identified: keeper-kit parity, Mod Studio visual feedback, and an appearance guide for LLM labeling.

## Goals / non-goals

- Goals: named vocabulary + offline solver with honest "closest match" warnings; keeper skin parity; studio preview of the two tones and per-player look cards; `docs/APPEARANCE_GUIDE.md`; corrected free-space/strip-sharing docs.
- Non-goals: arbitrary per-player RGB (hard ceiling: two team palettes in data, eight OBJ palettes in hardware — verified `ppu.c:2314`); merging the patch's C (collides with shipped `skin_tones`); a second runtime solver (one solver, in Python, shared by the fitter CLI and the studio — avoiding the patch's C+Python dual implementation); keeper hair (the keeper record has no hair slot — measured: 5 skin shades, 4 kit colours, 2 constants).

## Approaches considered

1. Merge the patch as code — rejected: 9/16 files conflict, scalar-vs-array schema collision, two uncoordinated `$89:FA42` allocators, wrong keeper record layout.
2. Host render-level per-player remap — rejected for now (the only true per-player-RGB path, but a renderer project: sprite identity, hair actors, substitutions, replays, HD tiles, widescreen, close-ups). Recorded as future work; not promised.
3. **Authoring-layer adoption onto the shipped runtime (chosen):** his solver runs offline, emits our format; zero new C except a correct keeper writer.

## The design (decisions made)

1. **`docs/APPEARANCE_GUIDE.md`** lands adapted from his patch: the light/dark + five-colour + 14-style vocabulary and the paste-ready LLM prompt; corrected where it implies 14/15 are writable (loader accepts 0-15, graphics exist for 0-13); points at the fitter workflow.
2. **One Python solver module** (single implementation) — `tools/looks.py` — ported from his `looks.py`: cartridge `Palettes` reader (strip colours 1-6, alt tables 0x1037C/0x103D2, keeper table 0x010326), `hair_class` classifier, the 4-way best-two-palettes search with his tie-break (staying-as-is wins), donor sets from the cartridge's own records plus the five `HAIR_PAINT` classes.
3. **`tools/fit_looks.py`** — CLI fitter: `--pack pack.json --base retail.sfc` (auto-discovery + hash check, `convert_rom_hack.py` precedent). Rewrites the pack IN PLACE or with `--out`: per team, runs the solver on the squad's look requests, emits `"skin_tones": [{skin?, hair}, ...]`. A tone that keeps the team's own palette OMITS `skin` so the loader leaves the stock ramp alone; a donor tone emits the donor's MIDDLE shade ×100/82 so the loader's re-derived 82% step lands back on that middle (the light/dark class is preserved); the `kit_record`+strip case forces emission because the write lands in the target record. Painted classes → HAIR_PAINT; current palette → its measured RGB so output is deterministic. It sets each requesting player's `"skin_tone": 0|1` and passes `"hair_style"` through; warns per player who cannot be represented ("team N: X players with {hair} hair cannot all be shown; closest palette used"). Keys the loader does not know (`skin`, `hair_color`) are REMOVED from the output pack (the fitter consumes them) — the guide must say the loader ignores them.
4. **Keeper parity (the one new C):** when a team authors `skin_tones[0].skin`, `patch_kit`'s skin write is followed by a keeper-record repaint: table `0x010326` (43 words), records are 24 bytes (length word 21 + 11 colours), colours 0-4 = five-step skin ramp via the existing `write_skin_steps` around base index 2, colours 5-10 untouched, in place, guarded by the length-word signature (mismatch → warn + skip). No pack key — automatic, documented.
5. **Mod Studio:** `preview.palette_pair` (both tones' skin-ramp + hair swatches from `skin_tones` RGB), per-player look cards using his verified head/hair-graphics decode (`$81:ED6D`, 13 overlay sets), and a team-pane skin section wired to `skin_tones` — all importing the same `tools/looks.py` solver for counts and warnings.
6. **Truth fixes:** MODDING's `$89:FA42` ceiling is the 43-entry table (43 × 16 = 688 bytes of 1470), not "about 84 teams"; the "teams share strips" claim is false on the retail image (86/86 distinct records) — fix MODDING, drop `SHARED_KIT_TEAMS` from the validator (the runtime's real-pointer-count warning already covers actual sharing), and note the real sharing that does exist (alternate records: 11 teams → 4 records).

## What is left to the builder

Exact fitter CLI ergonomics, studio pane layout, and how much of the hair-graphics decode to port (the full 13-set decode is already measured and tested in his patch's tests — port those tests). No ROM/hack content in commits; the retail ROM at repo root is the only fixture.
