# Independent Stadiums and Mod Studio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver usable stadium authoring with genuinely independent playable geometry and native/HD artwork, proven in the original game.

**Architecture:** Keep the original engine and Tk/Pillow editor. A validated profile registry supplies a selected-scene ROM view, native resource compilation, geometry predicates and stadium-local HD context. The editor edits the same contract and launches isolated native test sessions.

**Tech Stack:** C11, existing SNESRecomp Python generator and interpreter, SDL2, Python 3.10+, Tk, Pillow, PyInstaller, pytest, Ghidra 12.1.3.

**Spec:** `docs/superpowers/specs/2026-10-09-independent-stadiums-mod-studio-design.md`

**Field/allocation contract:** `docs/STADIUM_PROFILE_CONTRACT.md`

## Global Constraints

- Keep the existing 32-stadium ceiling, eight original layouts and introduction layout 8.
- No-profile packs and saves retain their existing behavior/context.
- `pitch_length` and `pitch_width` remain display metadata; engine units are separate.
- Native art must work at internal 1X; optional HD must remain stadium-local.
- Preserve sprite ownership, transparency, layer priority, color math and fades.
- Generated recompilation files are not hand-edited; regenerate from generator/config changes.
- Do not distribute the user's cartridge, extracted cartridge assets or saves.
- Test launches use owned config/mod/save directories; stop only their own process.
- Preserve all current dirty work; no release, version bump or publication.
- Use targeted red/green tests, then one full suite after integrated source stabilizes.

## Review Focus

- Two profiles with the same logical ID after a pack change must invalidate scene/HD caches, not reuse old assets. Task 5.
- A duplicated stadium must not share mutable artwork with its source or reuse an occupied ID. Task 7.
- Invalid text drafts must disable Test even if the previously committed model is valid. Task 8.
- Reordered JSON and cosmetic HD changes must not change gameplay save identity, while geometry changes must. Task 6.
- A packaged GUI must work without repository imports or host Tcl environment variables. Task 10.

## Execution and source ownership

Execute natively in this chat as requested. Before execution read the worktree
skill and preserve the current modified baseline; do not create a checkout that
loses prior audio, campaign, replay and stadium fixes. The primary implementer
owns integration and works in dependency order. A final independent review is
required by the execution/review skills. Document-only commits below are separate
from product changes; do not stage unrelated existing work.

## Investigation findings that determine the plan

The real pitch constructor is `A4E0B3`, using `81EC47`; `8386BF` constructs goal
apertures. Engine `(u,v)` projects to `(u+v-camera_x,v-camera_y)`. Constructor
completion blocks exist at `8BDB65`, `98F205` and `A4D7CE`, but replay and camera
code also reread tables. The ignored trace report inventories 398 explicit
geometry operands and eight scenery callbacks; it is not an indirect-consumer
closure certificate.

The initial format is fixed-origin and shrink-only, with 32-unit dimensions;
goals and original markings move with those dimensions. Arbitrary translated
origins and independent net/area resizing are excluded from v1. General values
must pass scenery/net envelope checks. First proof profiles use base 0,
1728x576 and 1664x576. This is a real geometry change in the same template,
not a change of base layout. Do not claim unsupported combinations work.

Native owned tile spans and palettes are frozen in the field contract. Palette
budgets are 32 colors for bases 2/6 and 48 otherwise, starting at entry 0x20.
No new free-memory claim is needed. Deferred transfers require scene-generation
tracking; `80B909/80B90D/808DB8` are candidates, not a verified scene boundary.

## Task 1: Close the construction, scenery and transfer evidence

**Files:** Create `tools/ghidra/probe_stadium_contract.py`, `tests/test_stadium_contract_evidence.py`, `docs/STADIUM_CONSTRUCTION_TRACE.md`; update `docs/STADIUM_PROFILE_CONTRACT.md` only with measured refinements.

**Interfaces:** `probe(executable: Path, rom: Path, output: Path) -> dict` and `validate_evidence(evidence: dict) -> bool` in the probe module; evidence has `rom_sha256`, `source_sha256`, `layouts`, `events`, `scenery_envelopes`, `complete`. No artifact is complete merely because bytes differ.

- [ ] Add certificate tests that reject missing layout records, missing replay route, reversed transfer/streamer order and unverified scenery/net overlap. Use the existing certificate testing pattern rather than inventing a pass from filenames.

```python
def test_incomplete_scene_order_is_rejected():
    evidence = {"complete": False, "layouts": [], "events": []}
    assert not validate_evidence(evidence)
```

- [ ] Run `python -m pytest tests/test_stadium_contract_evidence.py -q` and record the expected failure before implementing the validator/probe.
- [ ] Reuse the exact fresh Exhibition input from `tests/test_extra_stadium_native.py`, with LEFT for stadium 0 and RIGHT for IDs above default 1. Capture all eight original constructor/replay states without guest RAM edits. Use existing Ghidra export tools and original ASM for the field/transfer cross-check.
- [ ] Follow palette/decompression intent through synchronous and queued DMA, then the first `8B85E3/8B86E9` world streamer and first PPU line. Verify map lengths, page strides and tile base per template. Record original untouched baseline hashes.
- [ ] Audit `83A07C`'s eight callbacks against the proposed pitch and net envelope, including negative origin, goal depth 72 and width-dependent goal openings. Partition axis-aligned decision regions at original compare thresholds and test representative points through the original interpreter. A shrinking pitch alone is insufficient because its goal can move into a fence.
- [ ] Audit indirect consumers using the 398-reference inventory and runtime traces around throw-ins, goal kicks, corners, penalties, goals, halftime and replay. Record each consumer's original field or dynamic operand.
- [ ] Publish the measured envelope rules and final application state machine in the trace document. If a required consumer/allocation cannot be reconciled, stop that profile from being enabled and report the concrete gap; do not downgrade the acceptance requirement silently.
- [ ] Run certificate tests and commit only the probe/tests/trace documents.

## Task 2: Shared profile validation and native registry

**Files:** Create `tools/stadium_profile.py`, `ISSDNative/issd_stadium.h`, `ISSDNative/issd_stadium.c`, `tests/test_stadium_profile.py`, `tests/test_stadium_profile.c`; modify `issd_mod.h/c`, `tools/validate_mod.py`, `tools/mod_studio/model.py`, `cmake/issd_sources.cmake`.

**Interfaces:**

```c
typedef struct IssdStadiumProfile IssdStadiumProfile;
bool issd_stadium_rebuild_registry(void);
void issd_stadium_clear(void);
const IssdStadiumProfile *issd_stadium_profile(unsigned logical_id);
bool issd_stadium_has_profiles(void);
void issd_stadium_gameplay_digest(uint8_t out[32]);
unsigned issd_stadium_generation(void);
```

```python
@dataclass(frozen=True)
class Diagnostic:
    severity: str
    path: tuple
    message: str

```

Functions: `validate_profile(stadium: dict, pack_dir: Path) -> list[Diagnostic]`
and `validate_pack_data(pack: dict, pack_dir: Path, report: Report) -> None`.

The declarations above are the API contract. Implement all branches using the
field contract, including unsupported versions, nested path context, integer
types (reject booleans/floats), alignment and cross-field/envelope checks.

- [ ] Add a shared fixture matrix covering legacy entries, both acceptance profiles, dimensions below/above policy, invalid camera intervals, missing/escaping assets and pack precedence.

```python
def test_display_yards_do_not_define_playable_geometry(tmp_path):
    st = {"stadium_id": 8, "stadium_profile": {
        "version": 1, "base_layout": 0,
        "geometry": {"length_units": 1728, "width_units": 576}}}
    st.update(pitch_length=138, pitch_width=90)
    assert validate_profile(st, tmp_path) == []
    assert st["stadium_profile"]["geometry"]["length_units"] == 1728
```

- [ ] Run Python/C wrapper tests to establish red results, then implement native parsing/immutable resolved records and Python structured diagnostics. Extend the existing Report without removing its string error/warning lists.
- [ ] Registry publication is atomic: invalid or unreadable profiles never partially publish assets or silently claim custom geometry. Use existing pack ordering; profile consumers obtain the same winning stadium entry as names.
- [ ] Add the module to the shared source list, preserve unknown model fields on round-trip, run `test_mod_json.py`, `test_mod_stack.py`, profile matrix and editor round-trip tests. Commit only this task's files.

## Task 3: Selected-scene ROM view and dynamic AOT operands

**Files:** Modify `deps/snesrecomp/runner/src/snes/cart.c/h`, `deps/snesrecomp/recompiler/v2/cfg_loader.py`, `lowering.py`, `emit_function.py`, `emit_bank.py`, `deps/snesrecomp/tools/v2_regen.py`, `recomp/config/bank03.cfg`, `bank0b.cfg`; create `tests/test_stadium_rom_view.c/.py`, generator tests `deps/snesrecomp/tests/v2/test_runtime_immediate.py`; extend `issd_stadium.c/h`.

**Interfaces:**

```c
bool cart_setRomView(Cart *cart, const uint8_t *view, size_t size);
void cart_clearRomView(Cart *cart);
/* Borrowed same-size LoROM data view; canonical owned ROM remains unchanged. */
bool issd_stadium_rom_view(uint8_t *ram, Cart *cart);
```

Config directive `runtime_immediate <24-bit instruction PC>` lowers only listed
immediate operands to a runtime read from opcode PB and PC+1. It never uses DB.
Unlisted instructions retain existing constant lowering. Apply original M/X
width, flags, instruction timing and branch semantics. The selected private
view updates original geometry/camera/framing tables and listed operand bytes.
Extend `lower(insn, *, value_factory, runtime_immediates=frozenset())` and
`emit_bank(..., runtime_immediates=frozenset())` with an explicit immutable PC
set. Store it in BankCfg and pass it through the regeneration/emission path.

Initial exact operand sites: `838B27` -> `L+8`, `838E1A` -> `L/2`,
`838EB9/838ECC` retain penalty depth 384, `838ED6/838EDB` become centered penalty
bounds `W/2-192` and `W/2+192`. Camera sites `8B82B0` and `838C11` use the
projection/framing rules measured in Task 1, not an assumed `L/2`.
Normalize allowlist matching with `pc & 0x7fffff`; retain the actual instruction
bank for operand reads. The mirrored generated bank03 PC must match the83 entry.

- [ ] Add failing mirror/fast-path tests for CPU 8/16-bit reads, interpreter reads, DMA reads and `cart_getRomPtr`, including 0x00/0x80 bank mirrors and ROM view clearing. Canonical ROM and save hash input must remain unchanged.
- [ ] Add generator tests for 8/16-bit CMP/CPY/SBC, nonmatching directives, operand read bank and unchanged cycles. Execute the generated instruction against an interpreter reference to check A/B/X/Y/D/DB/P and flags at boundary values.

```python
def test_runtime_cmp_reads_operand_from_opcode_bank():
    from itertools import count
    from types import SimpleNamespace
    from v2.lowering import lower, IMM
    from v2.ir import Read, SegKind, Value
    from v2.codegen import emit_op
    insn = SimpleNamespace(mnem="CMP", mode=IMM, operand=0x708,
                           addr=0x838B27, m_flag=0, x_flag=0)
    ids = count()
    ops = lower(insn, value_factory=lambda: Value(vid=next(ids)),
                runtime_immediates=frozenset({0x838B27}))
    read = next(op for op in ops if isinstance(op, Read))
    assert read.width == 2 and read.seg.kind == SegKind.LONG
    assert read.seg.offset == 0x838B28
    c = "\n".join(emit_op(read))
    assert "cpu_read16" in c and "0x83" in c and "0x8B28" in c
```

- [ ] Implement the borrowed ROM view in both pointer and byte-read paths; preserve original Cart ownership/free/save behavior. Size/type mismatch fails without changing the view. Rebuild the view from canonical effective ROM when selection/generation changes; never incrementally patch a stale profile.
- [ ] Thread the explicit PC set through config/decode/lowering/emission, retaining exact instruction timing. Regenerate normally into an owned output directory first:

```powershell
python deps/snesrecomp/tools/v2_regen.py --rom 'International Superstar Soccer Deluxe (USA).sfc' --cfg-dir recomp/config --out-dir build/independent-stadium-validation/generated --banks 03,0b --jobs 4
```

- [ ] Compare output scope and promote only required regenerated units after tests pass. Do not introduce unrelated regeneration churn. Run ROM-view and generator regression tests, commit task files.

## Task 4: Geometry construction and marking compilation

**Files:** Extend `issd_stadium.c/h`, modify `main.c`, `issd_mod_rom.c`; create `tools/mod_studio/stadium_geometry.py`, `tests/test_stadium_geometry.c/.py`, `tests/test_stadium_geometry_compiler.py`.

**Interfaces:**

```c
void issd_stadium_opcode(CpuState *cpu, uint32_t pc24);
void issd_stadium_register_interpreter_hooks(void);
void issd_stadium_reset_scene(void);
void issd_stadium_restore_scene(uint8_t *ram, Ppu *ppu);
```

```python
def world_to_screen(u: int, v: int, camera_x: int, camera_y: int) -> tuple[int,int]:
    return u + v - camera_x, v - camera_y
```

Compiler function: `compile_geometry(profile: dict, template: dict) -> CompiledGeometry`.
Define `CompiledGeometry` in `stadium_geometry.py` as a frozen dataclass with
`metatiles: dict[int,bytes]`, `world_maps: dict[int,bytes]`,
`page_dimensions: tuple[int,int]`, `derived_fields: dict[int,int]`,
`camera: dict[str,int]` and `diagnostics: tuple[Diagnostic,...]`. Keys0/1 are
native layers. Exported examples use authored/generated resources and base
references, never copied cartridge image data.

- [ ] Test the two same-base profiles against the stock constructor's derived-field formulas and each dynamic predicate. Tests must distinguish different geometry rather than merely check selected ID.
- [ ] Implement one native hook dispatcher that invokes stadium policies independently of optional AI/bugfix flags. Clear/re-register interpreter hooks once, then install both existing gameplay and stadium callbacks without overwriting one another. Normalize bank mirrors.
- [ ] At the existing serializer boundary `85A50A`, select the explicit base layout for an active profile. At proven constructor/replay boundaries publish the selected ROM view, keep logical ID, and derive camera/goal state consistently. Introduction layout 8 bypasses the profile.
- [ ] Compile geometry masks in world coordinates, erase inherited old pitch marks/goals where moved, then draw the new boundaries/center/areas at the measured original marking dimensions. Reuse the template's permitted native characters; preserve net layer ownership and existing scenery outside the edited region. Deduplicate generated metatiles; overflow is an error.
- [ ] Test raster projection, old-mark removal, goal relocation, page/metatile indexing, marking alignment and 256-metatile limit. Compile previews from the same result, not independently drawn claims.
- [ ] Compare native/interpreter predicate behavior at inside/on/outside boundaries for throw-in, corner/goal kick, penalty and goal. Preserve original net depths/crossbar behavior. Commit after focused tests pass.

## Task 5: Native resources, local HD and render context

**Files:** Create `tools/mod_studio/stadium_assets.py`; extend `issd_stadium.c/h`, `issd_hd.c/h`, `issd_widescreen.c/h`, `main.c`; create `tests/test_stadium_assets.py`, `tests/test_stadium_art.c/.py`; extend existing HD tests.

**Interfaces:**

Functions: `import_image(source: Path, pack_dir: Path, stadium_id: int,
region: str, layer: int) -> ImportResult` and
`compile_manifest(manifest: dict, pack_dir: Path) -> CompiledAssets`.
Define frozen `TileWrite(slot:int,offset_tiles:int,data:bytes)` and
`CompiledAssets(geometry:CompiledGeometry,tile_writes:tuple[TileWrite,...],
palette:bytes|None,hd:dict[int,Path],dependencies:tuple[Path,...],
preview:Image.Image,diagnostics:tuple[Diagnostic,...])` in `stadium_assets.py`.
`ImportResult` contains `manifest:dict`, `compiled:CompiledAssets` and
`diagnostics:tuple[Diagnostic,...]`. The preview includes the quantized result
and current resource usage, not the original unrestricted image.

```c
void issd_stadium_transfer_complete(CpuState *cpu, uint32_t pc24);
int issd_hd_add_stadium_pack(unsigned id, unsigned generation, const char *directory);
void issd_hd_set_stadium_context(int id, unsigned generation);
void issd_hd_clear_stadium_packs(void);
```

- [ ] Test planar encode/decode, deterministic BGR555 quantization, transparent zero, bit15 rejection, each whitelist's tile bounds, base6 overlap resolution, exact palette/map file lengths and decoded image overflow.
- [ ] Implement conversion using Pillow: quantize to the owned banks, convert each 8x8 indexed tile into four SNES planes, deduplicate within slots and report actual color/tile usage. Palette-loss diagnostics include source and converted previews. Native compiler output is deterministic.
- [ ] Clone template native data before overrides. Keep selected-scene generation pending through original transfer completion; apply native replacements/maps only at the Task 1 verified phase before first world streaming. Palette changes join original mirror/fade construction. Test deferred original uploads cannot overwrite the finished custom scene.
- [ ] Scope HD lookup and per-line context to `(logical_id,generation,key)`, with local/global/native fallback. Reset memo and widescreen scene caches on context changes and load. Preserve existing owner checks, flips, pixel-zero transparency and RGB/color-math guards.

Declare an internal `hd_lookup_local(unsigned id, unsigned generation,
uint64_t key) -> const HdTexture*`; use this order at the existing memo lookup:

```c
const HdTexture *replacement = hd_lookup_local(scene_id, scene_generation, key);
if (!replacement) replacement = hd_lookup(key);
```

The namespace regression loads two known-color BMP fixtures successively for
ID8/generations1 and2, then composites the same native tile at ID9. Assert the
first two outputs have their respective local colors and ID9 has the global
color; compare actual output pixels, not a mock dictionary lookup.

- [ ] Run native 1X, HD, fade/weather, same-color sprite occlusion, replay/load and widened-margin cases before claiming artwork complete. Record distinct screenshots for stock and both custom siblings. Commit after focused checks.

## Task 6: Profile-aware save and password compatibility

**Files:** Modify `issd_save.c/h`, `main.c`, `issd_password.c/h` only if necessary; create `tests/test_stadium_save_context.c/.py`; extend campaign save tests.

**Interfaces:** `void issd_save_set_context_extra(const uint8_t *digest32)`;
NULL means existing context algorithm unchanged. Non-NULL applies an explicitly
versioned domain containing the old context and canonical gameplay registry digest.

- [ ] Test identical semantics across reordered JSON, changed cosmetic HD, changed geometry, disabled profiles and restored original baseline contexts. Failed loads leave guest state and source save bytes unchanged.
- [ ] Hash sorted effective logical IDs, base layouts and normalized gameplay fields/envelopes. Exclude cosmetic assets and paths. Reserve gameplay flag bit16 for enabled independent geometry so retail passwords reject modified gameplay even when canonical ROM bytes match.

```c
if (issd_stadium_has_profiles()) flags |= 16u;
/* Native stadium dispatcher remains independent of AI bits 2/4/8. */
```

- [ ] Refresh context when registry digest changes even if AI flags do not; keep canonical effective ROM separate from selected-scene ROM view. Restore derived resources/caches after successful load without changing the accepted snapshot's physics.
- [ ] Run campaign saves/password codec/transactional snapshot tests and owned real profile saves. Commit exact task files.

## Task 7: Editor document history and allocation

**Files:** Create `tools/mod_studio/document.py`; modify `model.py`, `app.py`; create `tests/test_mod_studio_document.py`; extend `test_mod_studio.py`.

**Interfaces:** `Document(pack, path)` with `begin(label,selection)`, `commit()`,
`cancel()`, `set_value(path,value)`, `undo()/redo() -> str|None`, `mark_saved()`
and `dirty`. `free_stadium_id(pack)->int|None`,
`clone_stadium(pack,source)->int`, `add_stadium_from_template(pack,base)->int`.
Capacity raises `StadiumCapacityError` before any document/asset mutation.

- [ ] Write red tests for occupied8..31, hole reuse, duplication into a fresh ID,
  nested unknown-field independence, local-art manifest independence, and grouped
  add/count/drag/text undo. Savepoint dirty state and redo branching are tested.

```python
def test_capacity_failure_does_not_mutate_document():
    pack = {"stadium_count": 32,
            "stadiums": [{"stadium_id": i} for i in range(8, 32)]}
    before = deepcopy(pack)
    with pytest.raises(StadiumCapacityError):
        clone_stadium(pack, pack["stadiums"][0])
    assert pack == before
```

- [ ] Implement whole-document JSON snapshots initially. Rebuild active bindings under `_suspend` after undo/redo; do not leave callbacks targeting replaced dictionaries. Group typing on focus/idle and dragging on press/release. Keep unknown fields.
- [ ] Route existing team/player/color/import/delete edits through history as well as stadium editing. Immutable imported files can remain orphaned locally after undo; export includes only referenced assets.
- [ ] Add Undo/Redo menus and shortcuts, repair capacity handling and outdated help. Run history plus all existing model round-trip tests. Commit task files.

## Task 8: Integrated stadium workspace and validation

**Files:** Create `tools/mod_studio/stadium.py`; modify `app.py`, `preview.py`,
`repo.py`, `baked.json`; create `tests/test_mod_studio_stadium.py`.

**Interfaces:** `StadiumWorkspace(parent,document,index,on_change)`;
`stadium_geometry(profile,compiled,size)->Image.Image`; inverse
`canvas_to_world`/`world_to_canvas` mappings; structured diagnostics address
`('stadiums',index,'stadium_profile','geometry',field)`.

- [ ] Add tests for typed/dragged values sharing one model, invalid drafts disabling Test, reset preserving identity/art/unknown fields, validation focusing the right control, and same compiler previews/export.
- [ ] Build persistent canvas and Identity/Geometry/Artwork sections. Show actual compiled pixels, pitch/goal/area overlays, camera window and resource usage. Mark display yards separately. Provide template clone/reset, ordinary image import and accessible layer/region selection without hash filenames.
- [ ] Keep raw string drafts separate from committed numbers; do not truncate floats or silently clamp. Validating the last good model must not hide a currently invalid edit. Preserve advanced global TileDialog.
- [ ] Validate against the actual pack directory, not a temporary JSON's directory. Update baked contract/template metadata and parity tests. Rebuild layouts at minimum window size, inspect GUI screenshots and keyboard navigation. Commit after tests pass.

## Task 9: Portable export and isolated Test in game

**Files:** Create `tools/mod_studio/export.py`, `game_launch.py`,
`tests/test_mod_studio_export.py`, `test_mod_studio_launch.py`; modify `app.py`.

**Interfaces:** `export_pack(pack,source_dir,destination)->Path`;
`LaunchRequest(executable,rom,stadium_id,pack,source_dir,test_root)`;
`stage(request)->StagedRun`; `menu_script(id,count)->str`;
`GameRunner.start(run)`, `poll()->RunStatus`, `stop()`.
`StagedRun` is a frozen record with Path fields `directory`, `config`, `mods`,
`saves`, `script`, `log`, plus `environment:dict[str,str]` and `request:LaunchRequest`.
`RunStatus` has `state` (staged/running/exited/failed/stopped), `exit_code:int|None`,
`message:str`, `log:Path`. The Tk layer reads these records via `after` polling.

- [ ] Red tests cover relocated exports with source removed, nested manifest dependencies, existing team photos, missing files, path collision/escape and ROM/save sentinel exclusion. Copy only schema-defined dependencies, never the whole source directory; validate staged output before atomic publication.
- [ ] Write fake-child tests proving responsiveness, nonzero/spawn diagnostics, separate paths and stopping only the owned child. Use argument lists and log handles rather than shell strings or blocking GUI subprocess calls.

```python
argv = [str(executable), "--rom", str(rom), "--config", str(run.config),
        "--mods-dir", str(run.mods), "--save-dir", str(run.saves),
        "--script", str(run.script)]
child = subprocess.Popen(argv, cwd=run.directory, stdout=log,
                         stderr=subprocess.STDOUT, env=run.environment)
```

- [ ] Generate authentic fresh-boot Exhibition scripts including the ID0 LEFT case. Verify native selected IDs0/8/31 with read-only state dumps. Stage no personal config and never overwrite source packs, ROM or saves. Poll using `Studio.after`; expose log and owned session path.
- [ ] Run source GUI author/save/export/test/stop flow, check sentinels unchanged and commit.

## Task 10: Packaged editor and complete Tcl/Tk smoke

**Files:** Modify `tools/build_mod_studio.py`, `tools/mod_studio/app.py` selftest,
`tools/mod_studio/README.md`; create `tools/test_mod_studio_package.py`.

- [ ] Add a failing preflight/package smoke requiring Tk, ttk alt theme, Notebook,
  Spinbox and Pillow ImageTk, then a screenshot of the stadium workspace. Missing
  Tcl theme scripts cause a build/test failure, not a successful skip.
- [ ] Verify Tcl DLL/script version compatibility. The bundled Codex Python has
  complete Tcl8.6 scripts; the host Python312 has PyInstaller but a missing theme.
  Use process-local compatible script paths for the build preflight or a complete
  build environment; do not change global Python/Tcl settings. Package all required
  shared validator/compiler modules and complete matching Tcl/Tk resources.
- [ ] Build `python tools/build_mod_studio.py`. Run the frozen executable from an
  owned directory with repository imports unavailable and `TCL_LIBRARY`,
  `TK_LIBRARY`, `PYTHONPATH` removed from the child environment. The bundled runtime
  must supply its own resources. Inspect screenshot and run the authored-profile
  workflow. Retain build/selftest/version logs. Commit source/docs, not the binary.

## Task 11: Native geometry/art acceptance and platform builds

**Files:** Create `tests/test_independent_stadium_native.py`,
`tools/ghidra/probe_independent_stadium_match.py`,
`docs/INDEPENDENT_STADIUM_ACCEPTANCE.md`; add original-art example resources
under `mods/independent_stadium_example/` and its JSON entry.

- [ ] Create two distributable same-base fixtures using original authored simple
  art (not extracted ROM pixels), lengths1728/1664, width576. Verify distinct
  constructor fields, goal positions and matching compiled markings.
- [ ] Exercise actual original boundary decisions for both profiles: out-of-play,
  throw-in, goal kick/corner, penalty and goal. Controlled test-owned fixtures can
  isolate boundary conditions, but are labeled separately from natural play.
- [ ] Complete one natural match on each profile with genuine goal replay,
  halftime/fulltime and native save/load. Use normal inputs, no clock/score edits;
  record checkpoint continuations honestly. Compare moving replay sequences at
  4:3,16:10,16:9,21:9 and native/HD sprite ownership.
- [ ] Regress all8 originals, previous4 aliases, intro8, weather/fades, substitutions,
  shootouts and campaign transitions. Run focused tests, then the full suite:

```powershell
python -m pytest tests -q -rs --basetemp=build/independent-stadium-validation/full-suite
cmake --build build/release-validation/windows --parallel 4
```

- [ ] Build Linux and Android using existing validated staging procedures after
  the shared source list is stable. Verify current deny/generated data packaging,
  owned Linux startup and Windows packaged GUI. State Android device gameplay
  coverage honestly. Retain source/artifact hashes and exact skipped reasons.
- [ ] Update modding/features/changelog documentation only to measured support,
  including v1 geometry restrictions and asset budgets. Do not call alias-only
  entries independently authored geometry. Keep old validation reports historical.
- [ ] Request final independent code review, resolve findings, run newly justified
  checks and report the actual deliverables/limitations. Do not publish a release.

## Plan self-review

Spec coverage is mapped to tasks: constructor/physics1–4; native/HD art1/5;
save compatibility6; editor history/allocation7; previews/inline validation8;
portable export/isolated launch9; packaged GUI10; proof/regressions/platforms11.
All five Review Focus conditions have owning tests. The v1 bounded geometry and
owned allocation contract is explicit; remaining timing and indirect-consumer
proof is Task1's deliverable, not a claim of existing support. If those checks
invalidate a contract value, revise it explicitly before enabling that value.
No product implementation has started at plan publication.
