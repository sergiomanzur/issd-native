"""Pin v2 decoder behaviour for the SMW SMC-dispatch phantom pattern.

The byte sequence `A9 imm 8D FC 1D high` parses two ways:

  M=1 (canonical SMW):
    A9 imm      LDA #$imm        (2 bytes)
    8D FC 1D    STA $1DFC        (3 bytes — patches a JSR-stub in WRAM)
    high ...    next insn        (1+ bytes)

  M=0 (phantom — only reached if some upstream REP cleared M):
    A9 imm 8D   LDA #$8Dimm      (3 bytes)
    FC 1D high  JSR ($high1D,X)  (3 bytes — operand is data bytes!)
    ...

The M=0 decode is what drove cf_debt_report to flag 11 CALL_INDIRECT
sites that runtime evidence (phantom-PC trap, 2026-05-03) confirms are
never executed as real instruction starts. The fix shifts to the
decoder: when a JSR (abs,X) has no cfg `indirect_call_table` entry,
suppress its fall-through edge (and emit a build-time diagnostic) so
the M=0 phantom doesn't pollute downstream decode/codegen.

These tests pin:

  1. M=1 entry on the canonical bytes never produces a JSR (a,X) in
     the graph — the FC byte is consumed as the operand of the M=1
     STA $1DFC.
  2. M=0 entry on the same bytes does produce JSR (a,X). Without cfg
     authorisation, the JSR's fall-through (the byte AFTER the JSR's
     last operand byte) is NOT decoded — i.e., $8006 should not be in
     the graph if entered only via the M=0 phantom path.
  3. A transient REP #$20 on one branch must not poison an unrelated
     branch's M=1 decode into a phantom JSR (a,X). The unrelated branch
     stays M=1 even though some other path through the function went
     M=0.
"""
from _helpers import make_lorom_bank0  # noqa: E402

from v2.decoder import decode_function  # noqa: E402
from snes65816 import INDIR_X  # noqa: E402


def _has_jsr_indir_x(graph) -> bool:
    """Return True iff any DecodedInsn in the graph is JSR with INDIR_X."""
    for di in graph.insns.values():
        if di.insn.mnem == 'JSR' and di.insn.mode == INDIR_X:
            return True
    return False


def test_m1_entry_smc_pattern_does_not_emit_jsr_indir_x():
    """Bytes `A9 25 8D FC 1D EA 60` entered at $8000 with M=1, X=1 must
    decode as LDA #$25 ; STA $1DFC ; NOP ; RTS — no JSR (a,X)."""
    rom = make_lorom_bank0({
        0x8000: bytes([0xA9, 0x25, 0x8D, 0xFC, 0x1D, 0xEA, 0x60]),
    })
    graph = decode_function(rom, bank=0, start=0x8000, entry_m=1, entry_x=1)

    # Real M=1 sequence: $8000 LDA, $8002 STA, $8005 NOP, $8006 RTS.
    expected_pcs = {0x008000, 0x008002, 0x008005, 0x008006}
    actual_pcs = {k.pc for k in graph.insns}
    assert expected_pcs.issubset(actual_pcs), (
        f"expected M=1 decode to cover {sorted(hex(p) for p in expected_pcs)}, "
        f"got {sorted(hex(p) for p in actual_pcs)}"
    )
    # Specifically: the byte at $8003 (which is the JSR opcode under M=0)
    # must NOT exist as a decoded insn — under M=1 the STA at $8002
    # absorbs $8003 and $8004 as its operand bytes.
    has_8003 = any(k.pc == 0x008003 for k in graph.insns)
    assert not has_8003, (
        "M=1 entry should NOT decode an insn at $8003 (it's the FC operand "
        "byte of STA $1DFC, not an instruction start). Phantom-decode "
        "regression — recheck post_mx / fall-through propagation."
    )
    assert not _has_jsr_indir_x(graph), (
        "M=1 decode of canonical SMC pattern must not produce any "
        "JSR (abs,X) — the FC byte is the operand of STA $1DFC, not a "
        "JSR opcode."
    )


def test_m0_entry_smc_pattern_jsr_present_but_not_authorised():
    """Bytes `A9 25 8D FC 1D EA 60` entered at $8000 with M=0, X=1 do
    decode the JSR (a,X) at $8003 (because under M=0 the LDA #imm is
    3 bytes long).

    Without a cfg `indirect_call_table` authorisation, the JSR's
    fall-through edge MUST be suppressed — i.e., the byte at $8006
    (which is RTS) must NOT be in the graph along this M=0 path.

    Authorising the site should restore the fall-through. (We don't
    test the authorised path here yet — that's part of Step 2-A;
    this test pins the suppression behaviour.)
    """
    rom = make_lorom_bank0({
        0x8000: bytes([0xA9, 0x25, 0x8D, 0xFC, 0x1D, 0xEA, 0x60]),
    })
    graph = decode_function(rom, bank=0, start=0x8000, entry_m=0, entry_x=1)

    # Under M=0: LDA at $8000 (3 bytes), JSR (a,X) at $8003 (3 bytes).
    has_lda = any(k.pc == 0x008000 and graph.insns[k].insn.mnem == 'LDA'
                  for k in graph.insns)
    assert has_lda, "M=0 entry must decode LDA at $8000"
    has_jsr = any(k.pc == 0x008003 and graph.insns[k].insn.mnem == 'JSR'
                  and graph.insns[k].insn.mode == INDIR_X
                  for k in graph.insns)
    assert has_jsr, (
        "M=0 entry must decode JSR (abs,X) at $8003 — that's the whole "
        "point of phantom detection: the bytes DO parse as a JSR (a,X) "
        "under M=0; the question is whether we follow its fall-through."
    )
    # CRITICAL: JSR (abs,X) must have NO successors when not authorised
    # via cfg. The fall-through to $8006 (RTS) must be severed so the
    # M=0 phantom doesn't pollute downstream decode.
    jsr_keys = [k for k in graph.insns
                if k.pc == 0x008003 and graph.insns[k].insn.mnem == 'JSR'
                and graph.insns[k].insn.mode == INDIR_X]
    for k in jsr_keys:
        di = graph.insns[k]
        assert di.successors == [], (
            f"unauthorised JSR (abs,X) at {hex(k.pc)} (m={k.m}, x={k.x}) "
            f"must have no successors; got {di.successors}. Suppression "
            f"of the fall-through edge is what stops the phantom from "
            f"polluting the rest of the function's decode."
        )


def test_unrelated_m1_path_is_not_poisoned_by_other_branch_rep():
    """Two-branch function: one branch REPs to M=0; the other stays M=1
    and runs the SMC sequence. The M=1 branch must decode the SMC bytes
    as STA $1DFC, not as the phantom JSR (a,X).

    Layout (bank 0):
      $8000  10 06        BPL $8008                   ; M=1, X=1
      $8002  C2 30        REP #$30                    ; M=0 path enters
      $8004  AD 00 00     LDA $0000                   ; M=0 (3-byte addr)
      $8007  60           RTS
      $8008  A9 25        LDA #$25                    ; M=1 SMC sequence
      $800A  8D FC 1D     STA $1DFC
      $800D  60           RTS
    """
    rom = make_lorom_bank0({
        0x8000: bytes([
            0x10, 0x06,                  # $8000 BPL $8008
            0xC2, 0x30,                  # $8002 REP #$30
            0xAD, 0x00, 0x00,            # $8004 LDA $0000   (M=0)
            0x60,                        # $8007 RTS
            0xA9, 0x25,                  # $8008 LDA #$25    (M=1)
            0x8D, 0xFC, 0x1D,            # $800A STA $1DFC   (M=1)
            0x60,                        # $800D RTS
        ]),
    })
    graph = decode_function(rom, bank=0, start=0x8000, entry_m=1, entry_x=1)

    # The M=1 branch must decode STA $1DFC at $800A.
    has_sta = any(k.pc == 0x00800A and k.m == 1 and k.x == 1
                  and graph.insns[k].insn.mnem == 'STA'
                  for k in graph.insns)
    assert has_sta, (
        "M=1 fall-through branch ($8008 onward) must decode STA $1DFC "
        "at $800A. If it didn't, REP/SEP propagation on the OTHER branch "
        "leaked into this one — the v1 last-writer-wins regression."
    )
    # And no JSR (abs,X) should appear in the M=1 branch (i.e., no
    # INDIR_X JSR with key.m == 1).
    bad = [k for k in graph.insns
           if k.m == 1 and graph.insns[k].insn.mnem == 'JSR'
           and graph.insns[k].insn.mode == INDIR_X]
    assert not bad, (
        f"M=1 keys must not contain any JSR (abs,X); found {bad}. The "
        f"REP on the other branch must not poison the M=1 fall-through "
        f"path's decode."
    )
