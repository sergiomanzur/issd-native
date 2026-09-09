# Recompiler output regression: the committed tree cannot build a working game

**Status:** open, owned by Sergio. Blocks all widescreen and performance work
that needs a runnable binary.

**Introduced:** the bank00 regeneration of 2026-09-06 19:19, committed as part
of `52f59aa`. Discovered 2026-09-07 while investigating unrelated widescreen
defects.

---

## Symptom

A binary built from the current tree opens a console window, prints
`[Running] Starting main execution loop...`, and then renders a permanently
black screen with no audio. Game mode stays `0x00` forever. The boot watchdog
fires:

```
=== WATCHDOG: Frame 1 exceeded 5.0s ===
Game mode: 0 | WatchdogCheck calls: 0
Call stack (most recent first):
  [0] CODE_808305_M0X0
  [1] bank_00_A0DF_M0X0
  [2] CODE_80A0A8_M0X0
  [3] bank_00_8480_M0X0
  [4] CODE_80844C_M0X0

=== stack-balance auditor: top 1 net-imbalanced funcs ===
  -3 bytes net over 1 calls (1 nonzero, last -3): CODE_80B4FA_M0X0
```

Before that, the reset vector itself wedges — see "Two distinct wedges" below.

## The one binary that works, and why it is not reproducible

`build/ISSDNative.exe` (8,023,552 bytes, linked 2026-09-07 13:18) runs the game
correctly. Verified headless:

```
[Frame 300]  Mode1: 0x01
[Frame 600]  Mode1: 0x06  Clock: 01:52  Cam: (2014, 112)
[Frame 1200] Mode1: 0x06  Clock: 01:50  Cam: (1763, 187)
```

Match mode, camera panning, clock counting down, 89 distinct colours in the
captured frame, no watchdog.

It is a **stale link**. Its object files tell the story:

| evidence | value |
|---|---|
| total objects | 168 |
| objects predating the 2026-09-06 19:19 regeneration | **164** |
| object for `bank00_v2.c` (single, unsplit file) | present |
| objects for `bank00_part00_v2.c` … `bank00_part33_v2.c` | **none** |
| total object size | 11.6 MB (vs 29.8 MB for a clean build) |

So it was linked from objects compiled around 2026-09-05, before bank00 was
split and regenerated. Ninja never recompiled them into that directory, and the
link on 09-07 13:18 picked up the old set.

**The source it was built from no longer exists:**

- not in `recomp/generated/` — that holds the 148-file post-split set
- not in git — `52f59aa` is the initial commit and already contains the
  post-regen output
- not in `recomp/.generated.snesrecomp-staging-_mtt7bki` — also a split set,
  from an earlier 09-06 08:18 attempt
- not in `recomp/ISSDRecomp/generated/` — only a 6-file, 21 KB stub set

A copy of the working binary is worth preserving before any `cmake --build build`
overwrites it.

## Why the regeneration is suspect

`regen_bank00_echo_fix.log` (2026-09-06 19:27) ends with:

```
  structural tier-down: routed 6 exact function variant(s) through the interpreter
  --banks filter active; preserving existing ...\unresolved_stubs_v2.c
  emitted dispatch table with 3109 entries -> ...\dispatch_v2.c
  atomically published generated output -> ...\recomp\generated
```

`--banks filter active` means this was a **partial** regeneration: freshly
generated bank00 was published alongside output for every other bank produced by
an earlier pass. A full pass takes ~788 s. Mixing generations is a plausible
source of the inconsistency, and is the first thing to rule out — a full
unfiltered regeneration.

## Two distinct wedges

They are separate failures and must not be conflated.

### 1. Reset vector — SPC700 port-echo handshake (contained)

Three routines drive the SPC700 IPL handshake. From their AOT bodies they
desynchronise the counter: the SPC stops echoing, its output ports freeze at
`$F1BB`, and `RtlApuWriteWaitEcho` (`deps/snesrecomp/runner/src/common_rtl.c`)
spins on `[apu] port echo timeout`. SDL window creation happens *after* the
reset vector, which is why a broken build shows a console and no window at all.

Measured: **1094** timeouts during a 60-frame headless boot with no deny set.

Contained by `recomp/aot_boot_deny.txt` (`$00B527`, `$00C2F4`, `$00C325`), which
`rtl_aot_node_denied()` uses to tier those nodes down to the interpreter. This
is applied by default now — see below. This is **containment, not a cure**: the
codegen for those nodes is still wrong.

### 2. Frame 1 — still open

With the reset vector fixed, frame 1 wedges instead. Bisection result: adding
the watchdog call stack to the deny set

```
808305  00A0DF  80A0A8  008480  80844C
```

**stops the watchdog firing**, but the game still never leaves mode `0x00` and
still renders black. So the bad codegen is broader than those five nodes.

`SNESRECOMP_EXECUTION_MODE=lle` did not help and did not print its selection
banner, so that path may not be wired in this build.

## Leads

1. **Full unfiltered regeneration** — rule out the mixed-generation hypothesis
   first. Gate the result on `tests/test_boot_reaches_main_loop.py`.
2. **The `-3` byte stack imbalance in `CODE_80B4FA_M0X0`.** A three-byte net
   loss is the width of a long return address — it smells like an RTL/RTS
   mismatch, or a JSL routine returning as if it were JSR. `S` ends at `$0198`
   where boot expects `$01AF`. The scratch files `_b527_current.txt`,
   `_c325_new.txt` and `_c325_old.txt` in the repo root are prior captures
   around exactly these routines.
3. **Continue the deny-set bisection** to find the minimal tier-down that
   restores gameplay. That set is itself a map of which emitted bodies are bad.

## What changed in service of this (kept)

- `recomp/aot_boot_deny.txt` — the deny set, now a tracked and commented asset.
  It was previously `aot_deny_boot.txt` in the repo root, **excluded by
  `.gitignore:61`**, so the one file that made the game bootable was untracked
  local state and a fresh clone could never start.
- `ApplyDefaultAotBootDenySet()` in `ISSDNative/main.c` — applies it by default,
  searching the working directory, the executable's directory, then
  `../recomp`. An explicit `SNESRECOMP_LLE_INTERP_TARGET_FILE` still wins, which
  is what bisecting needs. Warns rather than hanging silently if absent.
- `CMakeLists.txt` — copies the deny set next to the executable at build time.
- `tests/test_boot_reaches_main_loop.py` — runs the binary with every
  `SNESRECOMP_*` variable stripped and asserts the reset vector completes with
  fewer than 5 APU timeouts. Went from 1094 timeouts to passing.

## What this blocks

- `tests/test_widescreen.py` captures **solid black frames** and verifies
  nothing. Its assertions pass vacuously.
- `tests/test_savestate_replay.py` skips, by design, when the recorded frame is
  a single flat colour.
- The two open widescreen defects (margin breakage near the penalty areas,
  players popping in at the 4:3 boundary) cannot be verified, because they need
  a binary that reaches a live pitch.
