"""Pin v2 exit_mx_autoroute behaviour (per-variant, non-leaf-capable).

`detect_and_route` decodes every cfg `func` entry under all four
(entry_m, entry_x) combos. For each variant where the decode succeeds
and the exit (m, x) is unambiguous, it records a per-variant tuple
in `BankCfg.exit_mx_at_per_variant`. Non-leaf functions (with internal
JSR/JSL) are included; iterative fixpoint with monotonic information
flow handles their callee dependencies.

Tests pin:
  - Canonical leaf shapes (REP/SEP-only patterns).
  - Non-leaf SEP-then-RTS dispatcher class (the F9C9 / Layer1_Init
    shape that was the cfg-hint motivation across three sessions).
  - Per-variant recording: X-preserving leafs record only mutating
    entries; non-mutating entries are absent (decoder default
    suffices).
  - Hand-written cfg `exit_mx_at` directives win at seeded keys.
  - Ambiguous exits skip (analyzer returns None for either component).
  - Bounded iteration (no infinite fixpoint).

History: 2026-05-03 first non-leaf attempt regressed GraphicsDecompress
into an infinite loop via unsound intermediate (m, x) propagation
across the PHP/PLP gap. PHP/PLP tracking landed in snesrecomp 73e3d26
(2026-05-15); this allowed dropping the leaf-only restriction safely
in 2026-05-16.
"""
from _helpers import make_lorom_bank0  # noqa: E402

from dataclasses import dataclass, field
from v2.exit_mx_autoroute import detect_and_route  # noqa: E402


@dataclass
class _BankEntry:
    name: str
    start: int
    end: int = None
    entry_m: int = 1
    entry_x: int = 1
    tail_call_pc16: int = None


@dataclass
class _BankCfg:
    bank: int
    entries: list = field(default_factory=list)
    # Hand-written 4-tuple directives. Auto-router does NOT write to
    # this list; it seeds callee_exit_mx from it before its own
    # analysis runs (hand-written wins).
    exit_mx_at: list = field(default_factory=list)
    # Per-variant exits the auto-router commits.
    exit_mx_at_per_variant: list = field(default_factory=list)


def _per_variant_set(cfg):
    """Helper: return a set of (bank, addr16, em, ex, exit_m, exit_x)
    for assertion comparisons."""
    return {tuple(t) for t in cfg.exit_mx_at_per_variant}


# ── Leaf shapes (regression coverage for prior leaf-only behavior) ─────

def test_rep_20_only_records_x_preserving_mutators():
    """REP #$20 ; RTS — exit M=0 always, X preserved.

    Per-variant table:
      (0, 0) -> (0, 0)  no mutation
      (0, 1) -> (0, 1)  no mutation
      (1, 0) -> (0, 0)  M mutates only
      (1, 1) -> (0, 1)  M mutates only

    Auto-router records the two mutating entries; non-mutating are
    omitted (decoder default = preserve = correct).
    """
    rom = make_lorom_bank0({
        0x8000: bytes([0xC2, 0x20, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    fixes = detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    pv = _per_variant_set(cfg)
    assert (0x00, 0x8000, 1, 0, 0, 0) in pv  # M-mutates, X stays 0
    assert (0x00, 0x8000, 1, 1, 0, 1) in pv  # M-mutates, X stays 1
    # Non-mutating variants are NOT recorded.
    assert (0x00, 0x8000, 0, 0, 0, 0) not in pv
    assert (0x00, 0x8000, 0, 1, 0, 1) not in pv
    # cfg.exit_mx_at stays empty (auto-router doesn't touch the
    # broadcast list).
    assert cfg.exit_mx_at == []


def test_rep_30_records_three_mutators():
    """REP #$30 ; RTS — exit (m=0, x=0) regardless of entry.

    Per-variant table:
      (0, 0) -> (0, 0)  no mutation
      (0, 1) -> (0, 0)  X mutates
      (1, 0) -> (0, 0)  M mutates
      (1, 1) -> (0, 0)  M and X mutate
    """
    rom = make_lorom_bank0({
        0x8000: bytes([0xC2, 0x30, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    pv = _per_variant_set(cfg)
    assert (0x00, 0x8000, 0, 1, 0, 0) in pv
    assert (0x00, 0x8000, 1, 0, 0, 0) in pv
    assert (0x00, 0x8000, 1, 1, 0, 0) in pv
    assert (0x00, 0x8000, 0, 0, 0, 0) not in pv  # non-mutating
    assert cfg.exit_mx_at == []


def test_sep_only_x_flag():
    """SEP #$10 ; RTS — exit X=1 always, M preserved."""
    rom = make_lorom_bank0({
        0x8000: bytes([0xE2, 0x10, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000, entry_m=0, entry_x=0)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    pv = _per_variant_set(cfg)
    assert (0x00, 0x8000, 0, 0, 0, 1) in pv  # X-mutates
    assert (0x00, 0x8000, 1, 0, 1, 1) in pv  # X-mutates
    # Non-mutating (entry_x=1) variants are absent.
    assert all(t[3] == 0 for t in cfg.exit_mx_at_per_variant)


def test_two_leafs_in_one_bank_independent():
    """Two unrelated leaf state-mutators in one bank — both detected
    per-variant."""
    rom = make_lorom_bank0({
        0x8000: bytes([0xC2, 0x20, 0x60]),  # REP #$20 ; RTS — M-only
        0x9000: bytes([0xC2, 0x10, 0x60]),  # REP #$10 ; RTS — X-only
    })
    F = _BankEntry(name='F', start=0x8000)
    G = _BankEntry(name='G', start=0x9000)
    cfg = _BankCfg(bank=0x00, entries=[F, G])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    pv = _per_variant_set(cfg)
    assert (0x00, 0x8000, 1, 0, 0, 0) in pv
    assert (0x00, 0x8000, 1, 1, 0, 1) in pv
    assert (0x00, 0x9000, 0, 1, 0, 0) in pv
    assert (0x00, 0x9000, 1, 1, 1, 0) in pv


# ── Non-leaf shape (the F9C9 / Layer1_Init class) ──────────────────────

def test_non_leaf_sep_then_jsl_then_rts():
    """The canonical SMW dispatcher shape:

        SEP #$30       ; force m=x=1
        JSL <leaf>     ; call helper that preserves m,x
        RTS

    Exit (m, x) = (1, 1) regardless of entry. The leaf-only auto-router
    skipped this class entirely because of the JSL — but with the
    leaf-only restriction dropped, the analyzer now sees that all RTS
    paths exit at (1, 1) and records per-variant entries for every
    mutating entry.

    This is the F9C9 / Layer1_Init bug class that motivated three
    cfg hints across three sessions before this fix.
    """
    rom = make_lorom_bank0({
        # F at $8000: SEP #$30 ; JSL $018100 ; RTS
        0x8000: bytes([0xE2, 0x30, 0x22, 0x00, 0x81, 0x01, 0x60]),
        # Leaf callee at $01:8100 — preserves m,x via REP/SEP balance.
        # Won't reach this in this test since callee is in a different
        # bank and not represented; the auto-router needs to handle
        # the unknown-callee case.
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    pv = _per_variant_set(cfg)
    # Every entry variant should converge at exit (1, 1) because the
    # SEP #$30 forces m=x=1 before the JSL, and the JSL's return state
    # is unknown (decoder default = preserve = (1, 1) at this point).
    # Variants entering at (1, 1) don't mutate; the other three do.
    assert (0x00, 0x8000, 0, 0, 1, 1) in pv
    assert (0x00, 0x8000, 0, 1, 1, 1) in pv
    assert (0x00, 0x8000, 1, 0, 1, 1) in pv
    # (1, 1) entry → (1, 1) exit: no mutation, no record.
    assert (0x00, 0x8000, 1, 1, 1, 1) not in pv


def test_non_leaf_php_plp_balanced_preserves_entry():
    """PHP-bracketed body: SEP inside, PLP restores entry M/X. Exit
    matches entry exactly — no per-variant record needed at any entry.

    Decoder's PHP/PLP tracking (snesrecomp 73e3d26) is what makes this
    sound. Without it, the analyzer would see exit at the post-SEP
    state instead of entry-restored.
    """
    rom = make_lorom_bank0({
        # F at $8000: PHP ; SEP #$30 ; PLP ; RTS
        0x8000: bytes([0x08, 0xE2, 0x30, 0x28, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    # All four entry variants exit at the same (m, x) as they entered —
    # no mutation, no records.
    assert cfg.exit_mx_at_per_variant == []


# ── Soundness gates ────────────────────────────────────────────────────

def test_ambiguous_exit_skipped():
    """Two RTS paths exiting with different (M, X) — analyzer returns
    None for the disagreeing component. Per-variant record skipped."""
    # $8000: LDA $05        (A5 05)
    # $8002: BEQ $8008      (F0 04)
    # $8004: REP #$20       (C2 20)
    # $8006: RTS            (60)
    # $8007: NOP            (EA)
    # $8008: RTS            (60)   — exit M=entry; the other RTS exits M=0
    rom = make_lorom_bank0({
        0x8000: bytes([0xA5, 0x05, 0xF0, 0x04, 0xC2, 0x20, 0x60, 0xEA, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    # M is ambiguous (one path exits m=0, the other preserves entry m) →
    # every entry variant produces ambiguous exit → no records.
    assert cfg.exit_mx_at_per_variant == []


def test_no_terminator_skipped():
    """Function with no RTS/RTL — analyzer can't determine an exit."""
    # $8000: BRA $8000   (80 FE)
    rom = make_lorom_bank0({
        0x8000: bytes([0x80, 0xFE]),
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    assert cfg.exit_mx_at_per_variant == []


def test_unnamed_entry_skipped():
    """Entries with name=None are scaffolding, not real functions."""
    rom = make_lorom_bank0({
        0x8000: bytes([0xC2, 0x20, 0x60]),
    })
    F = _BankEntry(name=None, start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    assert cfg.exit_mx_at_per_variant == []


# ── Hand-written cfg directives win ────────────────────────────────────

def test_cfg_declared_exit_mx_seeds_callee_map_and_suppresses_auto():
    """A hand-written cfg `exit_mx_at` is seeded into callee_exit_mx
    BEFORE the auto-router's analysis runs — all 4 entry variants get
    the broadcast tuple. The auto-router's "skip already-known keys"
    check then leaves the function entirely alone.

    This is the contract: hand-written wins (the user is presumed to
    have a reason — e.g., ROM-specific knowledge the analyzer can't
    derive from bytes alone). Per-variant auto-records are suppressed
    even if the body would otherwise produce them.

    If the user wants per-variant correctness instead, they delete
    the hand-written 4-tuple and let the auto-router populate
    cfg.exit_mx_at_per_variant from scratch.
    """
    # $8000: REP #$20 ; RTS — auto-detected exit would be (m=0, x=entry)
    rom = make_lorom_bank0({
        0x8000: bytes([0xC2, 0x20, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(
        bank=0x00, entries=[F],
        exit_mx_at=[(0x00, 0x8000, 1, 1)],  # hand-written says (1, 1)
    )

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    # The hand-written 4-tuple is preserved.
    assert cfg.exit_mx_at == [(0x00, 0x8000, 1, 1)]
    # No per-variant records emitted — the seed claimed all 4 entries.
    assert cfg.exit_mx_at_per_variant == []


# ── Dispatch-terminator class (Layer1_Init shape) ─────────────────────

def test_dispatch_terminator_routes_via_handler_exits():
    """A function ending with `JSL <helper>; <table>` has NO RTS in its
    body — the dispatch JSL is the terminator. Without this fix,
    `analyze_function_exit_mx` returned (None, None) and the auto-
    router skipped the function entirely. Class fix: extend
    `analyze_function_exit_mx` to treat the dispatch JSL as an exit
    point, with effective exit (m, x) equal to the meet of the
    dispatched handlers' exits at the dispatch site's (m, x).

    Layout (canonical SMW `BufferScrollingTiles_Layer1_Init` shape):

        $8000  Dispatcher: SEP #$30 ; JSL helper ; <table:2 short>
        $8500  helper    : PLA ; ASL A ; TAY ; JMP (table,X)
        $85AA  Handler1  : RTL          (preserves m, x — exits (1, 1)
                                         at entry (1, 1))
        $85BB  Handler2  : RTL          (same)

    At the dispatch JSL site, m=1, x=1 (set by the prior SEP #$30).
    Both handlers entered at (1, 1) exit at (1, 1). Dispatcher exit
    is (1, 1) for every entry variant. The (1, 1) → (1, 1) case is
    no-mutation (already the decoder's default-preserve assumption),
    so it isn't recorded. The other three variants ARE mutations
    and must be recorded so callers entering at those (em, ex) get
    re-routed to the post-call (1, 1) label.

    This is the root-cause class of the 2026-05-16
    `BufferScrollingTiles_Layer2_Init_M0X0` M/X claim verifier trip
    (caller `InitializeLevelLayer1And2Tilemaps_M1X1` tracked m=0, x=0
    after `JSL Layer1_Init` because the auto-router never recorded an
    exit-(m, x) for the dispatcher).
    """
    rom = make_lorom_bank0({
        # Dispatcher: SEP #$30 ; JSL $00:8500 ; .dw $85AA, $85BB
        0x8000: bytes([0xE2, 0x30,                # SEP #$30
                       0x22, 0x00, 0x85, 0x00,    # JSL $00:8500
                       0xAA, 0x85,                # table[0] -> $85AA
                       0xBB, 0x85]),              # table[1] -> $85BB
        # Helper (canonical short-table dispatch signature):
        # PLA ; ASL A ; TAY ; JMP ($1000,X)
        0x8500: bytes([0x68, 0x0A, 0xA8, 0x7C, 0x00, 0x10]),
        # Handlers: bare RTL, preserve entry m, x.
        0x85AA: bytes([0x6B]),
        0x85BB: bytes([0x6B]),
    })
    Dispatcher = _BankEntry(name='Dispatcher', start=0x8000)
    Handler1   = _BankEntry(name='Handler1',   start=0x85AA)
    Handler2   = _BankEntry(name='Handler2',   start=0x85BB)
    cfg = _BankCfg(bank=0x00, entries=[Dispatcher, Handler1, Handler2])

    # Must pass dispatch_helpers — without it the decoder can't see the
    # JSL <table> pattern and would misdecode the table bytes as
    # garbage, exactly matching the pre-fix behaviour.
    dispatch_helpers = {0x008500: 'short'}

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom,
                     dispatch_helpers=dispatch_helpers)

    pv = _per_variant_set(cfg)

    # Handlers preserve entry (m, x) — no records for either handler.
    assert not any(t[1] in (0x85AA, 0x85BB) for t in pv)

    # Dispatcher: at the JSL terminator the site (m, x) = (1, 1) post-
    # SEP, and handlers exit (1, 1). Effective exit is (1, 1) for
    # every entry variant. Mutating entries get records; (1, 1) entry
    # matches exit and is absent.
    assert (0x00, 0x8000, 0, 0, 1, 1) in pv
    assert (0x00, 0x8000, 0, 1, 1, 1) in pv
    assert (0x00, 0x8000, 1, 0, 1, 1) in pv
    assert (0x00, 0x8000, 1, 1, 1, 1) not in pv


def test_dispatch_terminator_ambiguous_handlers_skipped():
    """Two handlers exiting with different (m, x) at the same dispatch
    site → ambiguous exit for the dispatcher → no per-variant record.

    This is the soundness gate: when handlers disagree, we don't
    paper over with a guess; the dispatcher stays unrouted and the
    caller falls back to its pre-call assumption."""
    rom = make_lorom_bank0({
        # Dispatcher: SEP #$30 ; JSL $00:8500 ; .dw $85AA, $85BB
        0x8000: bytes([0xE2, 0x30,
                       0x22, 0x00, 0x85, 0x00,
                       0xAA, 0x85,
                       0xBB, 0x85]),
        # Helper
        0x8500: bytes([0x68, 0x0A, 0xA8, 0x7C, 0x00, 0x10]),
        # Handler1 exits m=0, x=0
        0x85AA: bytes([0xC2, 0x30, 0x6B]),  # REP #$30 ; RTL
        # Handler2 exits m=1, x=1 (preserves entry — no REP/SEP)
        0x85BB: bytes([0x6B]),              # RTL
    })
    Dispatcher = _BankEntry(name='Dispatcher', start=0x8000)
    Handler1   = _BankEntry(name='Handler1',   start=0x85AA)
    Handler2   = _BankEntry(name='Handler2',   start=0x85BB)
    cfg = _BankCfg(bank=0x00, entries=[Dispatcher, Handler1, Handler2])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom,
                     dispatch_helpers={0x008500: 'short'})

    pv = _per_variant_set(cfg)
    # No dispatcher records — handlers disagree at (1, 1) site:
    # Handler1 exits (0, 0); Handler2 exits (1, 1).
    assert not any(t[1] == 0x8000 for t in pv)


def test_dispatch_terminator_unknown_handler_skipped():
    """Handler not cfg-declared → callee_exit_mx has no entry for it →
    dispatcher's exit is unknown → no per-variant record. The
    auto-router refuses to guess."""
    rom = make_lorom_bank0({
        0x8000: bytes([0xE2, 0x30,
                       0x22, 0x00, 0x85, 0x00,
                       0xAA, 0x85]),  # one table entry → $85AA
        0x8500: bytes([0x68, 0x0A, 0xA8, 0x7C, 0x00, 0x10]),
        0x85AA: bytes([0xC2, 0x30, 0x6B]),  # REP #$30 ; RTL
    })
    # NOTE: Handler1 NOT declared as a cfg func entry.
    Dispatcher = _BankEntry(name='Dispatcher', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[Dispatcher])

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom,
                     dispatch_helpers={0x008500: 'short'})

    # No record for the dispatcher: handler exit unknown.
    assert cfg.exit_mx_at_per_variant == []


# ── Fixpoint must be order-independent (re-derive every pass) ─────────

def test_caller_exit_revised_when_callee_exit_known_later():
    """Pin the WallRun/F465 class:

      Caller F (at $8000): `REP #$20 ; JSR <callee> ; RTS`
      Callee G (at $8100): `SEP #$20 ; RTS`

    F enters at (em=1, ex=1). After the REP, body is in (0, 1). JSR
    enters G at (em=0, ex=1). G's exit is (m=1, x=1) (SEP forces m=1).
    Post-JSR state in F is therefore (1, 1), and F's RTS exits (1, 1)
    — i.e. F at M1X1 entry → exit (1, 1), no mutation.

    The bug: when the autoroute analysed F before G's exit was
    recorded, F's decoder default-preserved across the JSR and
    derived exit (0, 1). The old `commit-once` rule then kept that
    (0, 1) record forever, even after G committed (1, 1).

    With the fixpoint fix, each pass re-derives F using the current
    callee_exit_mx. Pass 1: F sees no G record → (0, 1). G commits
    (1, 1). Pass 2: F sees G record → (1, 1). Pass 3: stable.

    Final committed state: F's M1X1 entry yields no per-variant
    record (exit equals entry). The bogus (0, 1) record must NOT
    appear.

    To stress order-dependence, put F BEFORE G in the cfg entry
    list (the autoroute iterates entries in declared order; F-first
    is the order that triggered the bug).
    """
    rom = make_lorom_bank0({
        # F at $8000: REP #$20 (C2 20) ; JSR $8100 (20 00 81) ; RTS (60)
        0x8000: bytes([0xC2, 0x20, 0x20, 0x00, 0x81, 0x60]),
        # G at $8100: SEP #$20 (E2 20) ; RTS (60)
        0x8100: bytes([0xE2, 0x20, 0x60]),
    })
    F = _BankEntry(name='F', start=0x8000)
    G = _BankEntry(name='G', start=0x8100)
    cfg = _BankCfg(bank=0x00, entries=[F, G])  # F BEFORE G

    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)

    pv = _per_variant_set(cfg)

    # F's M1X1 entry → exit (1, 1), no mutation. Must NOT appear.
    assert (0x00, 0x8000, 1, 1, 0, 1) not in pv, (
        "F at M1X1 → (0, 1) is the staleness bug: F was analysed "
        "before G, default-preserved across JSR, committed (0, 1); "
        "the fixpoint should re-derive F once G commits (1, 1)."
    )
    assert (0x00, 0x8000, 1, 1, 1, 1) not in pv  # no-mutation, no record

    # G's mutating entries record correctly. M1X1 entry: exit (1, 1)
    # — no mutation, no record. M0X0/M0X1 entries: exit (1, 0)/(1, 1).
    assert (0x00, 0x8100, 0, 0, 1, 0) in pv
    assert (0x00, 0x8100, 0, 1, 1, 1) in pv


# ── Bounded iteration ─────────────────────────────────────────────────

def test_iteration_terminates_for_self_recursive():
    """A function that BRAs to itself is non-terminating in asm; the
    auto-router must NOT spin. With no RTS reachable, the exit is
    indeterminate and gets skipped."""
    rom = make_lorom_bank0({
        0x8000: bytes([0x80, 0xFE]),  # BRA $8000
    })
    F = _BankEntry(name='F', start=0x8000)
    cfg = _BankCfg(bank=0x00, entries=[F])

    # Should return quickly. If it doesn't, the test runner times out
    # before this assertion fires.
    detect_and_route([(0x00, 'bank00.cfg', cfg)], rom)
    assert cfg.exit_mx_at_per_variant == []
