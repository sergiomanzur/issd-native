# Cartridge Packs — Full ROM Hack Support via Mods (issue #3)

**Date:** 2026-11-19
**Status:** approved for implementation (user mandated end-to-end delivery: "see if possible; if possible, implement, test, commit, push, release, comment, close")
**Issue:** sergiomanzur/issd-native#3 — play complete ISS Deluxe ROM hacks (e.g. the World Cup 2026 hack) as mods, without modifying the APK, preserving hack content.

## Intent and why

Modders distribute complete ROM hacks — rebuilt cartridges with new squads, kits, graphics, title screens and sometimes new code. The JSON mod format is a data overlay with a verified-write boundary: it can express rosters, kits, names, plates, photos, flags, stadiums and skin tones, but nothing about a rebuilt cartridge's own layout, graphics or code. The ask is to make such hacks playable as mods.

## Feasibility verdict (from three parallel investigations, facts verified in-tree)

1. **Expressing a full hack as JSON mod data is structurally impossible.** The cartridge has 8-byte name fields, a 42-team ceiling (43-entry roster pointer table, 7×6 select grid), no keys for menu layouts, tournament structure or code, and every mod write is guarded against non-vanilla bytes. The World Cup 2026 hack's headline features (10-character names, group-structured select screen, rebalanced AI) cannot be represented.
2. **Swapping the entire cartridge image is mechanically sound.** The ROM is plain bytes in RAM; every data read resolves dynamically (`cart_getRomPtr`, host helpers read `g_rom_data`); a whole-image view swap (`cart_setRomView`) is already used for expanded stadiums. The clean substitution point is `main.c:2258-2276` — after `LoadRomFile`, before `g_base_rom_data` is captured — so every downstream copy derives from the substitute.
3. **The limit, stated plainly:** the engine is a hybrid. Boot, NMI, deny-listed routines and unknown targets run interpreted from cartridge bytes; everything else runs precompiled, with code and immediates baked into C. Changed *code* bytes are therefore mostly inert, and *relocated* data is still read from its original addresses. Data-preserving hacks (vanilla layout, new data bytes) work; engine rewrites do not, and cannot be made to without an interpreter-everywhere mode that the frame-loop architecture does not safely support today.

## Goals

- A mod pack can carry a **complete replacement cartridge**: either a full image (`hack.sfc`) or an **IPS patch** applied to the user's verified retail ROM.
- The stack rule is simple: the topmost enabled cartridge pack supplies the image; regular packs still apply on top, guarded as today.
- Users keep supplying their own retail ROM; the launcher's verification stays intact. We never ship ROM content.
- A converter tool turns any hack (ROM or IPS) into a pack **and reports what will carry over**, region by region, so nobody is surprised by the hybrid engine.
- Ships as a tagged release with instructions, and the issue is answered honestly.

## Non-goals / anti-goals

- No support for expanded (>2 MB), HiROM/ExHiROM, coprocessor-claiming, or copier-headered images — rejected at load with a reason.
- No code-patching, no tournament/menu schema keys, no interpreter-everywhere mode (revisit separately).
- No bundled ROMs or hack content in this repository.
- Do not break any existing pack, save, or platform path.

## Constraints (verified)

- Image must be exactly 2,097,152 bytes, headerless; mapping byte at file `0x7FD5` must be LoROM (bit 0 clear); cart-type byte `0x7FD6` must be `0x00` (ROM-only).
- IPS: standard records plus RLE runs; writes beyond 2 MB refused (expansion unsupported); truncation (EOF offset) refused with a warning.
- Saves namespace themselves by effective-image hash (`issd_save.c` context) — automatic, desired. The retail password bridge self-disables on hash mismatch — non-fatal, documented.
- Mod scanner is flat `mods/*.json`; pack assets resolve pack-relative with traversal checks (`issd_asset_resolve` accepts any regular file).
- Android receives packs via SAF folder import into `getExternalFilesDir()/mods`; AppImage seeds from tracked `mods/` assets — no changes needed while we ship no cartridge packs.
- Version scheme `X.Y.Z-beta.N` with matching `versionName`/`versionCode` in `android/app/build.gradle`; `tools/assemble_release.py` has a docs allow-list that must list new release docs.

## Approaches considered

1. **Extend the JSON schema to express hack content** (groups, cell table, 48 teams, menus, code) — rejected: structurally impossible inside this release (see feasibility 1); the outstanding table-relocation RE work alone is a project of its own.
2. **Cartridge packs: whole-image / IPS substitution at boot** — chosen: mechanically proven by the runtime's own `cart_setRomView` use, small blast radius, preserves the supply-your-own-ROM posture, and immediately makes every layout-preserving hack playable, including its graphics, palettes, text and title screens.
3. **Interpreter-everywhere compatibility mode** (deny-list all functions so cartridge bytes are authoritative) — rejected for v1: boot/NMI interpretation returns to native each frame; a full-game interpreted loop has no demonstrated cycle-sliced path, host HLEs assume vanilla structure, and the risk profile is wrong for a release. Recorded as future work if a hack community needs engine-level hacks.

## The design (decisions made)

- **Pack JSON gains a top-level `"cartridge"` object:**

```json
"cartridge": {
  "image": "hack.sfc",
  "patch": "hack.ips",
  "sha256": "…64 hex, optional…"
}
```

  Exactly one of `image`/`patch` (both or neither → pack error, pack ignored, retail ROM used). `patch` is applied to the user's verified retail ROM; `image` is loaded directly (user obtains it from the hack's author). `sha256`, when present, must match the substituted image or the pack is refused with a warning.

- **New module `ISSDNative/issd_cartridge.c/.h`** (pure C, no SDL, harness-testable): IPS apply, image validation, file read. `main.c` calls it once at boot between `LoadRomFile` and the base copy; on success `rom_data` points at the substituted buffer and everything downstream follows; on any failure the retail image is used and the result/warning system reports why.
- **Stack rule:** `issd_mod.c` gains a resolver: scan enabled packs in apply order, topmost cartridge pack wins, warn when more than one is enabled. Regular packs keep applying on top of the substituted image; their existing revision guards refuse vanilla-site writes (non-fatal, reported).
- **`tools/convert_rom_hack.py`:** `--rom hack.sfc` (or `--ips hack.ips`) + `--name/--author/--out` → writes a flat pack JSON plus a pack folder with the IPS (a full ROM is diffed against the retail base to produce one) and a `REPORT.md` classifying every changed region against the known data map (rosters, names, kits, pointer tables, head palettes, free space…) as **honored data**, versus **outside known data** — where the precompiled engine ignores changed code and reads relocated data from its original addresses. Header changes get explicit warnings. This report is the honesty contract with hack users.
- **Validator:** `tools/validate_mod.py` checks the cartridge key's shape, file existence, extension (`.sfc` for images, `.ips` for patches) and sha256 format.
- **Docs:** MODDING.md gains a "Cartridge packs" section with usage, the hybrid-engine explanation and the limits; SUPPORTED_FEATURES, README, REVERSE_ENGINEERING (the AOT/interpreted hybrid fact) and CHANGELOG are updated. Release `v0.7.0-beta.1` with `docs/releases/v0.7.0-beta.1.md` instructions.

## Builder-check rulings (2026-11-19, controller)

1. **In-game apply with a cartridge pack toggled:** re-substitute on the in-process restart path too, not only at boot. `issd_cartridge.c` retains the retail image; the restart path (main.c ~2584, the Android/in-process variant) asks it for the effective base before `issd_mod_reapply`. Desktop relaunches the process and gets re-substitution for free. Pause-menu toggling therefore works everywhere.
2. **Copier headers:** normalize inside the cartridge path and the converter only — a 2,097,664-byte buffer is treated as 512-byte-headered (the launcher already accepts exactly that shape) and the header dropped; anything that is not 2,097,152 or 2,097,664 is refused. The general `LoadRomFile` path is left alone (out of scope).
3. **IPS base verification:** enforced in `issd_cartridge.c` — the base must hash to the retail SHA-256 before a patch is applied, on every path (CLI, Android, headless), not just the launcher. The retail hash/size constants move to `issd_cartridge.h` and `issd_launcher_model.c` consumes them (one source of truth). Full-image packs keep structural checks plus the optional pack `sha256` only.
4. **Quick/manual saves are NOT image-scoped** (only campaign saves are). A retail save loaded under a hack restores incompatible state. V1 documents the hazard in MODDING.md and the release notes ("after switching cartridge packs, load only saves made under the same cartridge"); stamping image identity into slot payloads is future work. The spec's "saves namespace themselves" line applies to campaign saves only and is corrected here.
5. **Free-space contract for regular packs over a hack:** added-team roster writes into `$87:FC40` gain the same free-fill guard the stadium and tone-1 paths already have (`rom_is_free`); a hack occupying the run makes the write refuse with a warning instead of clobbering. The converter's report flags hacks that occupy pack free space (`$82:FB5D`, `$87:FAC8/$87:FC40`, `$89:FA42`) as at-risk.
6. **Converter base ROM:** `--base PATH` option; default auto-discovery tries `International Superstar Soccer Deluxe (USA).sfc` in the CWD and verifies its hash; a clear error if absent or wrong. `--ips` mode requires the base (it must apply the patch to produce the report); `--rom` mode requires it (for the diff).
7. **Report categories:** three, not two — (a) known data (honored), (b) interpreted code (live: boot `$80:8000`, NMI `$80:80E0`, and the deny-listed PCs from `recomp/aot_boot_deny.txt`), (c) everything else (precompiled/unknown: inert code, relocated data read from original addresses). Best-effort, stated as such.

**Superseded by shipped behavior (fix wave, 2026-11-19).** Two claims above are stale; the shipped code is the truth.

- Ruling 4 (saves): current-build saves are image-scoped. Every save is an envelope carrying the save context, and `ValidateEnvelope` compares it on load, so campaign, quick, manual and favorite saves made under one cartridge refuse to load under a different one. Only legacy raw saves from older builds have no identity, and those load only through the explicit legacy confirmation.
- The both-sources/shadow wording in "Stack rule" and ruling 4: the resolver now reports through its own three-state status (none / refused with a reason / winner) plus shadow notes, not through callers reading the global result counters or prose "warn"/"reported" inference. Boot and restart restate those notes after `issd_mod_reapply`, whose reset would otherwise erase them. See `issd_mod_cartridge_resolve` in the shipped code.
