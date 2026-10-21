# Fork quality improvements implementation plan

**Goal:** Backport the four improvements selected in chat from Mabec-games/iss-deluxed at 54344cb, preserving ISSD Native defaults, overlay, platforms and saved configuration meanings.

**Approach:** Adapt focused source patches, credit the fork, and verify with native harnesses and ROM-backed checks. Work on codex/fork-quality-improvements; keep Master League stash untouched.

- [x] Display: append Fit Pixel Perfect, persist Widescreen Menus (default on), integrate menu and window-size selection. Test literal output rectangles, small windows, config round trips and menu navigation.
- [x] Widescreen: backport weather tile filtering, bounded edge fill and framed banner windows. Test authentic center pixels, weather/HUD separation, independent stadiums and snapshot compatibility.
- [x] Animation: accept verified walking states and clean interpolation artifacts. Test walking/running across directions, unsupported action rejection, unchanged WRAM and restored VRAM.
- [x] Controllers: reclaim detached slots before handling delayed removal events. Keep independent identical controllers stable; avoid identifying a device solely by model GUID and timing. Test SDL virtual-controller reconnects and held-input suppression.
- [x] Validate: focused harnesses, build Windows executable, ROM-backed graphics checks, broader suite, fresh review, changelog and feature documentation.

Ruling: Implement inline in the clean existing checkout on a feature branch; no extra worktree is needed for this scoped backport.
Ruling: Model GUIDs identify controller models, not individual devices. Preserve four-player slot identity rather than adopting the fork's ambiguous GUID/time-based reassignment.

Baseline: 8 focused tests passed before changes (video, config, menu, animation, running, host input and widescreen harnesses).

Review: fresh read-only review found a frozen walking phase mismatch; reproduced it with a failing test, fixed the slot match, and obtained a clean follow-up review.

Verification so far: Windows Release build succeeds; 25 focused harness tests pass; all 64 running and 80 walking midpoint cases pass against the local ROM. Standalone five-width widescreen acceptance passes at frame 1200 with identical simulation and authentic center pixels. Full suite was intentionally interrupted without reported failures after its initial campaign checks; previous repository record takes 53 minutes. Focused native acceptance completed: 29 passed in 282.60 seconds, covering Fit widths and center pixels, 64 running/80 walking midpoints, walk/dash simulation and snapshot isolation, replay restoration, tactical cameras, coin/statistics scenes, independent stadiums and four-player save/reload. The separate harness run returned 25 passed in 28.89 seconds. Android and physical-controller/device playtests were not performed.
