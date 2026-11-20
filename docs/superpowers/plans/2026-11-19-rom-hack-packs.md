# Cartridge Packs (ROM Hack Support) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Mod packs can carry a complete replacement cartridge (full `.sfc` image or IPS patch over the user's verified retail ROM), making layout-preserving ROM hacks playable as mods — with a converter tool that reports exactly what carries over.

**Architecture:** Substitution happens between `LoadRomFile` and the first downstream copy (`main.c:2258-2276`) and on the in-process restart path; a new pure-C module `ISSDNative/issd_cartridge.c` holds the retail image, normalizes copier headers, applies/validates, and answers "what is the effective base image". A Python converter turns any hack ROM/IPS into a pack plus a three-category compatibility report. Work splits into two independent chains (C core / Python tooling) merged before docs and release.

**Tech Stack:** C (harness-tested via pytest-compiled C programs), Python (validator + converter), IPS patch format, SNES LoROM facts.

**Spec:** `docs/superpowers/specs/2026-11-19-rom-hack-packs-design.md` — including its Builder-check rulings (binding). All addresses/hashes below were verified in-tree on 2026-11-19.

## Verified facts the implementers must not re-derive

- Boot path: `LoadRomFile` (main.c:1004-1027) → `g_rom_data`/`g_base_rom_data` (main.c:2272-2276) → `issd_mod_rom_set_image` (main.c:2282) → `SnesInit` (main.c:2321). Insert substitution immediately after the ROM-read success log (~main.c:2271), BEFORE `g_base_rom_data` is captured, so `g_base_rom_data`, `s_pristine` and `cart->rom` all derive from the substituted image.
- In-process restart: `main.c:2584-2607` (Android path) — reapply + `issd_mod_copy_applied_rom` into `cart->rom`; desktop restart relaunches the process instead. The restart path must re-ask `issd_cartridge` for the effective base BEFORE `issd_mod_reapply`.
- Retail image constants (single source of truth, currently duplicated in `ISSDNative/issd_launcher_model.c:9-27`): size 2,097,152 (+512-byte copier header tolerated, i.e. 2,097,664 accepted as headered), SHA-256 `d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9`. A SHA-256 implementation already exists in-tree (used by `issd_launcher_model.c` / `issd_save.c` / `issd_password.c` — find and reuse it; do not add a third).
- Image structural checks: mapping byte at file `0x7FD5` must be LoROM (bit 0 clear); cart-type byte at `0x7FD6` must be `0x00` (ROM-only, no coprocessor). Headered input = first 512 bytes dropped (ruling 2).
- IPS format: `"PATCH"` magic; records `[3-byte big-endian offset][2-byte big-endian length][bytes]`; length 0 means RLE: next 2 bytes = run length, 1 byte = fill value; `"EOF"` terminates, an optional 3-byte truncation size after EOF is ignored with a warning (truncation is refused, ruling 2 spirit: result must be exactly 2 MB). Records writing at or beyond 2 MB are refused. `build.bat`/docs precedent: none — this is new code.
- Pack JSON parse lives in `ISSDNative/issd_mod.c` (`pack_member` ~:361-372 parses top-level keys; `team_member` shows the object/array patterns; `json_colour`/`json_u8` are the value helpers; unknown top-level keys are currently SKIPPED whole).
- Resolver precedent: `issd_mod_team_photo_path` / `issd_mod_team_flag_path` (issd_mod.c:623-649) resolve pack-relative files via `issd_asset_resolve` (issd_asset_path.c:39-71 — rejects `..`, absolute, backslashes; accepts any regular file).
- Result/warning system: `issd_mod_result_note_warning` / `issd_mod_result_note_error` (issd_mod.h) fold into the menu's report lines.
- Free-space guard precedent: `rom_is_free` (issd_mod_rom.c ~:342); `open_added_slots` (issd_mod_rom.c:915-965) memcpy's added rosters into `$87:FC40` WITHOUT a free check today (ruling 5 fixes this).
- Validator: `tools/validate_mod.py` — `check_team`/`check_player` style, `Report.error/warn`, `--strict` fails on warnings; shipped packs must keep passing (tests/test_mods_valid.py).
- Deny-listed (interpreted, live code) PCs: `recomp/aot_boot_deny.txt`; plus boot entry `$80:8000` (file offset 0x0000) and NMI `$80:80E0` (file offset 0x00E0).
- Scanner: flat `mods/*.json` only (issd_mod.c:473-510); pack assets live in sibling folders referenced by relative path.
- Test harness: `compile_c` in tests/test_config_persistence.py:1-28 compiles C with `-std=c11 -Wall -Wextra -Werror`; new tests pair a `tests/test_*.c` with a `tests/test_*.py` runner. C tests read the retail ROM at repo root ("International Superstar Soccer Deluxe (USA).sfc") — in a worktree, copy it in first (untracked).
- ROM/IPS content policy: NEVER commit or fixture real ROM content; tests synthesize buffers and patch records in code.

## JSON schema (pinned)

Pack top-level key:

```json
"cartridge": {
  "image": "hack.sfc",            /* OR */
  "patch": "hack.ips",
  "sha256": "64 lowercase hex, optional"
}
```

- Exactly one of `image`/`patch`; both or neither → pack refused with an error note, retail ROM used.
- `image`: pack-relative file, loaded whole; 2,097,152 or 2,097,664 bytes (latter = 512-byte copier header dropped).
- `patch`: pack-relative IPS applied to a copy of the retail image; base must verify against the retail SHA-256.
- `sha256` (either mode): expected hash of the resulting 2 MB image; mismatch → refused with a warning.
- Structural checks on the result: 2,097,152 bytes, LoROM mapping byte, ROM-only type byte.
- Stack rule: the TOPMOST enabled cartridge pack (highest apply_order) supplies the image; any other enabled cartridge pack draws a warning. Regular packs still apply on top (their guards/refusals unchanged).

## Global Constraints

- C89/C11 style matching the codebase: measured-prose comments naming where facts came from.
- Do not modify `recomp/generated/` or `deps/`.
- Every shipped pack in `mods/` must keep passing `validate_mod.py --strict` (tests/test_mods_valid.py).
- Focus suite must be green before every commit: `python -m pytest tests/test_mod_json.py tests/test_mod_rom.py tests/test_mod_stack.py tests/test_mod_document.py tests/test_mods_valid.py tests/test_mod_studio.py tests/test_kit_redirect.py -q -p no:cacheprovider` (plus each task's new test file). The known unrelated failure `tests/test_match_options_native.py` is out of scope.
- No real ROM or hack bytes in commits, fixtures, or tests — synthetic buffers only.
- Chain A works in worktree `../isssdeluxe-recomp-cart-core` on branch `feature/rom-hack-packs-core`; chain B in `../isssdeluxe-recomp-cart-tools` on branch `feature/rom-hack-packs-tools`. Copy the retail ROM into each worktree root before running tests. Commit only your chain's files.

## Review Focus

1. **Back-compat:** a session with no cartridge pack enabled must be byte-identical to today (no substitution, no retained buffers changing behavior). Pinned by chain A tests comparing boot-visible state with/without a disabled cartridge pack.
2. **Refusal safety:** every failure mode (missing file, wrong size, wrong hash, bad IPS record, truncation, headered-image handling, two cartridge packs enabled) must leave the retail image active with a reported reason — never a half-patched buffer, never a crash.
3. **The retail-hash gate:** IPS packs refuse a non-retail base on every entry path (not just the launcher). Pinned by a chain A test with a doctored base.
4. **Free-space safety:** added-team rosters refuse to write over a hack's occupied `$87:FC40` run (ruling 5). Pinned by a chain A test with planted non-0xFF bytes.
5. **Report honesty:** the converter's report must classify boot/NMI/deny-listed PCs as live, known data as honored, and everything else as not-guaranteed — mislabeling any of the three is a defect (ruling 7). Pinned by chain B tests with synthetic regions placed in each category.

---

## Chain A — C core (worktree ../isssdeluxe-recomp-cart-core, branch feature/rom-hack-packs-core)

### Task 1 (Chain A1): `issd_cartridge.c/.h` — pure cartridge module

**Files:**
- Create: `ISSDNative/issd_cartridge.h`, `ISSDNative/issd_cartridge.c`
- Create: `tests/test_cartridge.c`, `tests/test_cartridge.py`
- Modify: `ISSDNative/issd_launcher_model.c` (consume shared constants)

**Interfaces (produced, consumed by A2/A3):**
- `issd_cartridge.h` exports: `ISSD_RETAIL_SIZE 2097152`, `ISSD_RETAIL_SHA256 "d2fe66c1..."` (moved from issd_launcher_model.c, which then includes issd_cartridge.h or re-uses the constants — keep the launcher's behavior identical); `bool issd_cartridge_stash_retail(uint8_t *rom, size_t size);` (retains the retail buffer, normalized: drops a 512-byte copier header when size is 2,097,664 — caller keeps ownership of the returned effective pointer via out-params); `bool issd_cartridge_apply_patch(const uint8_t *patch_data, size_t patch_size, uint8_t *rom_out, size_t rom_cap, char *why, size_t why_cap);` (validates IPS, applies to the stashed retail copy into `rom_out`, refuses expansion/truncation, `why` on failure); `bool issd_cartridge_validate_image(const uint8_t *rom, size_t size, char *why, size_t why_cap);` (size, LoROM byte 0x7FD5 bit0 clear, type byte 0x7FD6 == 0); `bool issd_cartridge_sha256_matches(const uint8_t *rom, size_t size, const char *expected_hex);` (wraps the in-tree SHA-256).
- The module keeps NO global mutable state beyond the retained retail pointer + size, reset by `stash_retail`.

- [ ] **Step 1: Write failing tests** in `tests/test_cartridge.c` (compiled with `ISSDNative/issd_cartridge.c` + the in-tree sha256 source + `issd_asset_path.c` if needed; follow tests/test_mod_rom.py's compile_c pattern):
  1. Retail stash + normalization: 2,097,664-byte buffer with a fake 512-byte prefix stashes as the 2 MB tail; 2,097,152 stashes whole; 2,097,153 fails with a reason.
  2. IPS apply: a synthetic IPS with two records + one RLE run against a zero-filled 2 MB buffer produces the expected bytes; `PATCH` magic missing → refuse; a record at offset 2,097,152 → refuse; a 3-byte EOF truncation → refuse with warning text; short/truncated record → refuse.
  3. Image validation: LoROM byte clear/type byte 0 passes (use the real retail ROM's header bytes for a passing case); HiROM bit set or coprocessor type → refuse with reasons.
  4. sha256 match helper against the real retail ROM file (repo root) with the true hash; a doctored byte → false.
- [ ] **Step 2: Run** `python -m pytest tests/test_cartridge.py -q -p no:cacheprovider` → FAIL (no module).
- [ ] **Step 3: Implement** the module. Reuse the in-tree SHA-256 implementation (find it via `issd_launcher_model.c`'s includes); comment block cites the design doc rulings (header normalization, base verification, refusal-not-corruption).
- [ ] **Step 4: Run the full focus suite + tests/test_cartridge.py** → PASS, and launcher constants refactor keeps `tests/test_mod_stack.py`-covered behavior identical.
- [ ] **Step 5: Commit** `feat(cart): cartridge module - retail stash, IPS apply, image validation`

### Task 2 (Chain A2): Pack JSON `"cartridge"` key + stack resolver

**Files:**
- Modify: `ISSDNative/issd_mod.h` (pack fields), `ISSDNative/issd_mod.c` (parse + resolver)
- Test: `tests/test_cartridge.c` (extend)

**Interfaces:**
- `IssdModPack` gains `char cartridge_image[64]; char cartridge_patch[64]; char cartridge_sha256[73];` (0 = absent).
- `bool issd_mod_cartridge_resolve(char *pack_name_out, size_t name_cap, char *file_out, size_t file_cap, bool *is_patch, char *sha256_out, size_t sha_cap);` — walks ENABLED packs in apply order, topmost cartridge pack wins, emits its name/file/flags/hash; false when none. Warns (result note) for each shadowed cartridge pack. Both image+patch set → the pack's cartridge is invalid: error note, skipped by the resolver.

- [ ] **Step 1: Failing tests**: parse the pinned schema example (all three fields round-trip); both image+patch → both kept but resolver skips + error note; resolver picks the topmost of two enabled cartridge packs and notes the shadowed one; sha256 format stored as given.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** following `team_member`'s object/array parse pattern for the `"cartridge"` object; resolver follows the apply-order walk precedent (`issd_mod_pack_at_order`).
- [ ] **Step 4: Focus suite green.** **Step 5: Commit** `feat(mod): parse cartridge packs and resolve the active one`

### Task 3 (Chain A3): Boot + restart integration, free-space guard

**Files:**
- Modify: `ISSDNative/main.c` (boot path ~2258-2282, restart path ~2584-2607), `ISSDNative/issd_mod_rom.c` (open_added_slots guard)
- Test: `tests/test_cartridge.c` (extend with the guard test via the mod-rom harness pattern)

**Interfaces:**
- Consumes A1/A2. Boot: after `LoadRomFile` succeeds (~main.c:2271) call `issd_cartridge_stash_retail(rom_data, rom_size)`; if `issd_mod_cartridge_resolve(...)` finds a pack: load the file (image → fresh buffer; patch → apply onto a 2 MB buffer from the stashed retail), validate structure + optional sha256; on success replace `rom_data`/`rom_size` with the effective image and log `[Init] Cartridge pack '<name>' active.`; on failure log + result-note the reason and keep retail. The retail buffer ownership stays with `issd_cartridge`.
- Restart path (~main.c:2584): before `issd_mod_reapply`, re-run the same resolve+substitute against the stashed retail and point `s_live`/`g_rom_data` at the effective image (issd_mod_rom needs a way to rebase: extend `issd_mod_rom_set_image` semantics or add `issd_mod_rom_rebase_image`; reapply then patches on top; the existing `issd_mod_copy_applied_rom` moves it into `cart->rom`).
- `open_added_slots` (issd_mod_rom.c:915-965): before the `memcpy` into `$87:FC40`, verify the destination run is free (`rom_is_free` precedent); refuse + warn when the base (possibly a hack) occupies it.

- [ ] **Step 1: Failing test**: plant non-0xFF bytes at the added-roster destination, apply a pack with `new_team` → write refused, warning present (Review Focus 4). (Boot-path glue itself is thin and reviewed, not harness-tested — main.c cannot link standalone.)
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** with the measured-prose comments citing the boot/restart facts above.
- [ ] **Step 4: Full focus suite + tests/test_cartridge.py green.** **Step 5: Commit** `feat(cart): substitute the cartridge at boot and restart; guard added-roster free space`

## Chain B — Python tooling (worktree ../isssdeluxe-recomp-cart-tools, branch feature/rom-hack-packs-tools)

### Task 4 (Chain B1): Validator checks for cartridge packs

**Files:**
- Modify: `tools/validate_mod.py`
- Test: `tests/test_mods_valid.py` (extend)

- [ ] **Step 1: Failing tests**: cartridge present → must be an object (error otherwise); exactly one of image/patch (both/neither → error); image file exists beside the pack and ends `.sfc` (error) — `.smc` → error naming the headerless-only rule; patch ends `.ips` and exists; sha256 is 64 lowercase hex when given; cartridge on a pack that also has teams/stadiums is allowed (they layer, not conflict); a pack with ONLY a cartridge key and nothing else passes.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** beside the existing pack-level checks in the file's error/warn style. **Step 4: focus suite green.** **Step 5: Commit** `feat(validate): check cartridge packs`

### Task 5 (Chain B2): `tools/convert_rom_hack.py` — converter + compatibility report

**Files:**
- Create: `tools/convert_rom_hack.py`
- Create: `tests/test_convert_rom_hack.py`

**Interfaces:**
- CLI: `--rom hack.sfc` XOR `--ips hack.ips`; `--name` (required), `--author`, `--version`, `--out DIR` (default `mods/<slug>`); `--base retail.sfc` (default: auto-discover `International Superstar Soccer Deluxe (USA).sfc` in CWD; verify SHA-256 `d2fe66c1...` — error with instructions if absent/wrong; ruling 6).
- Output: `<out>.json` (flat pack at mods root: name/author/version + cartridge block with `patch` and `sha256` of the result) and `<out>/` folder with the IPS (a `--rom` input is first diffed against the base to produce the IPS; a 512-headered hack input is normalized first — ruling 2) and `REPORT.md`.
- `REPORT.md` (ruling 7): changed regions (runs merged across gaps ≤ 16 bytes) classified: **interpreted/live** (contains file offset 0x0000-0x0200 boot area entry, 0x00E0 NMI, or any deny-listed PC mapped LoROM→file via `bank*0x8000 + (addr & 0x7FFF)` from `recomp/aot_boot_deny.txt`), **known data** (against a curated table seeded from `issd_mod_rom.c` constants + `docs/REVERSE_ENGINEERING.md`: rosters 0x50000, names 0x3818E, kits 0x483C0..0x48AC8, pointer tables 0x1027A/0x102D0/0x1037C/0x103D2, head combos 0x4DD6A..0x4DE8A + index 0x1795F, cell table, free space 0x4FA42.., $82:FB5D/$87:FAC8/$87:FC40 — flag the free-space ones "occupied: pack free space at risk"), **not guaranteed** (everything else, with the hybrid-engine sentence). Header diffs (mapping/size/title) reported explicitly; result-size ≠ 2 MB or expansion → hard error.
- Round-trip guarantee: applying the produced IPS to the base reproduces the (normalized) hack image byte-for-byte — asserted by the tool itself before writing, and by tests.

- [ ] **Step 1: Failing tests** (synthetic buffers, tmp_path — never real ROM bytes in the repo; the real ROM at repo root may be used as the base fixture): diff→IPS→round-trip; a change inside the roster region reports "known data"; a change at file 0x10 (boot) reports "interpreted"; a change at 0x300000-adjacent code space reports "not guaranteed"; a planted deny-listed PC (use one real entry from aot_boot_deny.txt) reports live; free-space occupation flagged; headered hack input normalized; expansion (a change past 2 MB) → error.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: focus suite + new tests green.** **Step 5: Commit** `feat(tools): convert ROM hacks into cartridge packs with a compatibility report`

## Chain C — integration, docs, release (main tree, branch feature/rom-hack-packs)

### Task 6 (Chain C1): Merge chains + docs

**Files:**
- Merge `feature/rom-hack-packs-core` and `feature/rom-hack-packs-tools` into `feature/rom-hack-packs` (disjoint files; run the full focus suite on the merged branch).
- Modify: `docs/MODDING.md` (new "Cartridge packs" section: schema, usage, converter, the hybrid-engine limits, the quick-save hazard from ruling 4, free-space interplay), `docs/SUPPORTED_FEATURES.md`, `README.md` (modding pointer), `docs/REVERSE_ENGINEERING.md` (AOT/interpreter hybrid note), `CHANGELOG.md` (Unreleased).
- Test: docs stay consistent with the validator (tests/test_mods_valid.py green; a shipped example is NOT added — no ROM content ships).

- [ ] **Step 1: Merge both chains, run the full focus suite.**
- [ ] **Step 2: Write the docs** in the repo's measured-prose voice.
- [ ] **Step 3: Suite green.** **Step 4: Commit** `docs(cartridge): cartridge pack documentation and changelog`

### Task 7 (Chain C2): Release v0.7.0-beta.1 (controller-driven)

- Bump: `VERSION` → `0.7.0-beta.1`, `android/app/build.gradle` versionCode → 12, versionName → `0.7.0-beta.1`; add `docs/releases/v0.7.0-beta.1.md` (four standard sections; instructions for cartridge packs); add it to `tools/assemble_release.py`'s docs allow-list; fold Unreleased into a `## 0.7.0-beta.1 - <date>` heading.
- Build what this machine can (probe cmake/WSL/docker/gradle first; report which platforms ship), assemble assets, tag `v0.7.0-beta.1`, `gh release create` with title `v0.7.0-beta.1 — Full ROM hack support via cartridge packs` + notes, comment on issue #3 (what works, what does not, where to find it), close the issue.
