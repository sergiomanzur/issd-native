"""Asm context + bounds heuristics for CALL_INDIRECT sites.

Companion to cf_debt_report.py. Given a list of (site_pc24, table_base,
function) tuples, produces:

  - 6-10 lines of decoded asm before and after each JSR (abs,X) site.
  - Bounds-pattern detection (CMP/CPX/AND immediates + branch shape +
    ASL ladder count) → candidate table count.
  - First-N entry dump for ROM-table sites; deferred mark for RAM-table
    sites whose table base is < $8000.

Methodology
-----------
We use `recompiler.v2.decoder.decode_function` to build a
FunctionDecodeGraph for each containing function. That decoder follows
real control flow (REP/SEP propagation, branches, JSR-fall-through,
cfg `end:` boundary), so site PCs land on real instruction boundaries
— unlike a naive linear forward decode, which can land mid-operand
when a `STA $1DFC`-shaped byte sequence fools a byte-stream walker
into reading the operand byte $FC as a JSR (abs,X) opcode.

For each site we:

  1. Locate the cfg `func NAME PC [end:END]` entry for the JSR's
     containing function (name is read from the generated C; variant
     suffix `_M0X1` etc. is stripped to find the cfg name and used to
     seed `decode_function(entry_m, entry_x)`).
  2. Decode the function. Find the DecodedInsn at site_pc24.
  3. Build a predecessor map by inverting `.successors`. Walk back
     from the JSR following the most-recently-visited predecessor
     edge (i.e., the predecessor whose PC is just below the JSR;
     usually the fall-through path). This gives the immediate
     control-flow chain, which is what bounds detection needs to see
     `CMP #$nn ; ASL A ; TAX ; JSR (abs,X)`.
  4. Display: sort all decoded insns by PC; show pre_window insns
     before the JSR, the JSR itself, and post_window insns after.
     This is a linear-PC view (good for human eyeballing).
  5. Bounds detection runs on the control-flow predecessor chain,
     not the linear PC slice — bounds are relative to flow.
  6. Table dump reads N 16-bit entries at `bank:operand` from ROM.
     Mark `ram-runtime` for operand < $8000 (not statically resolvable).

Confidence scoring
------------------
  high   — bounds + ASL + TAX/TAY all present and consistent
  medium — bounds present but unusual stride, or no ASL detected
  low    — no bounds found, or chain reconstruction failed

Reachability
------------
NOT determined here. cf_debt_report's reachability column is filled
by an external runtime probe (deferred). This module only reasons
about static structure.
"""

from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Optional, Tuple, Set

# Snes65816 + v2 decoder modules live a couple directories up. Prepend
# the recompiler dir to sys.path so we can import without an installed
# package layout.
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_RECOMPILER_DIR = os.path.normpath(os.path.join(_THIS_DIR, '..', '..', 'recompiler'))
if _RECOMPILER_DIR not in sys.path:
    sys.path.insert(0, _RECOMPILER_DIR)
_V2_DIR = os.path.join(_RECOMPILER_DIR, 'v2')
if _V2_DIR not in sys.path:
    sys.path.insert(0, _V2_DIR)

import snes65816 as snes  # type: ignore  # noqa: E402
from decoder import decode_function, FunctionDecodeGraph, DecodeKey, DecodedInsn  # type: ignore  # noqa: E402


# ---------- cfg parsing -------------------------------------------------

RE_BANK = re.compile(r'^\s*bank\s*=\s*([0-9a-fA-F]{2})\s*$')
# `func NAME PC [end:END] ...` — captures end: too.
RE_FUNC = re.compile(
    r'^\s*func\s+([A-Za-z_][A-Za-z_0-9]*)\s+([0-9a-fA-F]{4})\b'
    r'(?:.*?\bend:([0-9a-fA-F]{4}))?'
)


@dataclass
class CfgFunc:
    name: str
    bank: int
    pc16: int
    end16: Optional[int] = None


def load_cfg_funcs(cfg_dir: str) -> Dict[str, CfgFunc]:
    """Return {func_name: CfgFunc}. Names are unique across banks today."""
    out: Dict[str, CfgFunc] = {}
    if not os.path.isdir(cfg_dir):
        return out
    for fn in sorted(os.listdir(cfg_dir)):
        if not (fn.startswith('bank') and fn.endswith('.cfg')):
            continue
        path = os.path.join(cfg_dir, fn)
        cur_bank: Optional[int] = None
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                m = RE_BANK.match(line)
                if m:
                    cur_bank = int(m.group(1), 16)
                    continue
                m = RE_FUNC.match(line)
                if m and cur_bank is not None:
                    name = m.group(1)
                    pc = int(m.group(2), 16)
                    end_str = m.group(3)
                    end = int(end_str, 16) if end_str else None
                    out[name] = CfgFunc(name=name, bank=cur_bank,
                                         pc16=pc, end16=end)
    return out


# ---------- variant suffix handling ---------------------------------

RE_VARIANT_SUFFIX = re.compile(r'_M([01])X([01])$')


def strip_variant(name: str) -> Tuple[str, int, int]:
    """Strip _MnXn suffix and return (base_name, entry_m, entry_x).
    Defaults to (1, 1) when no suffix is present."""
    m = RE_VARIANT_SUFFIX.search(name)
    if not m:
        return name, 1, 1
    base = name[:m.start()]
    return base, int(m.group(1)), int(m.group(2))


# ---------- predecessor graph + chain reconstruction ----------------

def build_pred_map(graph: FunctionDecodeGraph) -> Dict[DecodeKey, List[DecodeKey]]:
    """Invert `.successors` so each key knows its predecessors."""
    preds: Dict[DecodeKey, List[DecodeKey]] = {k: [] for k in graph.insns}
    for k, di in graph.insns.items():
        for s in di.successors:
            if s in preds:
                preds[s].append(k)
    return preds


def find_keys_for_pc(graph: FunctionDecodeGraph, pc24: int) -> List[DecodeKey]:
    return graph.keys_at_pc(pc24)


def walk_back_chain(
    graph: FunctionDecodeGraph,
    preds: Dict[DecodeKey, List[DecodeKey]],
    start_key: DecodeKey,
    *,
    max_depth: int = 20,
) -> List[DecodedInsn]:
    """Walk backward through predecessors. At each step, prefer the
    predecessor with the IMMEDIATELY-LOWER PC (the fall-through arrival).
    Fallback to any predecessor. Stops at the entry or at a join with
    no clear linear predecessor.

    Returns a list ordered from earliest-to-latest (so chain[-1] is the
    insn just before the JSR). Limits depth to keep output bounded.
    """
    chain: List[DecodedInsn] = []
    seen: Set[DecodeKey] = set()
    cur = start_key
    for _ in range(max_depth):
        plist = preds.get(cur, [])
        if not plist:
            break
        # Sort: predecessor at PC < cur.pc (fall-through arrival path)
        # comes first; among those, the highest PC (closest to cur).
        cur_pc = cur.pc & 0xFFFF
        same_bank = (cur.pc >> 16) & 0xFF
        def score(k: DecodeKey):
            kpc = k.pc & 0xFFFF
            kbank = (k.pc >> 16) & 0xFF
            # Prefer same-bank, fall-through (k.pc < cur.pc), small gap.
            in_bank = 0 if kbank == same_bank else 1
            is_fall = 0 if kpc < cur_pc else 1
            gap = (cur_pc - kpc) if kpc < cur_pc else (cur_pc + 0x10000 - kpc)
            return (in_bank, is_fall, gap)
        best = min(plist, key=score)
        if best in seen:
            break
        seen.add(best)
        di = graph.insns.get(best)
        if di is None:
            break
        chain.insert(0, di)
        cur = best
    return chain


def slice_around_pc(
    graph: FunctionDecodeGraph,
    site_key: DecodeKey,
    *,
    pre: int = 8,
    post: int = 4,
) -> Tuple[List[DecodedInsn], DecodedInsn, List[DecodedInsn]]:
    """Linear-PC slice. Sort all insns by PC, find the JSR, return
    neighbouring insns. Useful for human-readable display."""
    rows = sorted(graph.insns.values(),
                  key=lambda d: (d.key.pc, d.key.m, d.key.x))
    site = graph.insns[site_key]
    # Find rows[i] == site by identity (key equality).
    idx = next(
        (i for i, d in enumerate(rows) if d.key == site_key),
        None,
    )
    if idx is None:
        return [], site, []
    pre_rows = rows[max(0, idx - pre):idx]
    post_rows = rows[idx + 1:idx + 1 + post]
    return pre_rows, site, post_rows


# ---------- formatting ----------------------------------------------

def fmt_insn(di: DecodedInsn) -> str:
    bank = (di.key.pc >> 16) & 0xFF
    pc = di.key.pc & 0xFFFF
    flags = f'[M={di.key.m} X={di.key.x}]'
    return f'${bank:02X}:{pc:04X} {flags} {di.insn.mnem:<5} {di.insn._fmt()}'


# ---------- bounds detection ----------------------------------------

# Plausible static-dispatch table size cap. Real 65816 dispatchers
# almost always have ≤256 entries (X is usually 8-bit; even 16-bit X
# rarely enumerates beyond a few dozen entries). A CMP #$D002-shaped
# bound is decoder noise (non-IMM byte stream interpreted as IMM under
# the wrong M/X), not a real upper-exclusive index check.
_MAX_PLAUSIBLE_TABLE_SIZE = 256

# SMW SMC-dispatch idiom we want to flag. The canonical pattern is
# `LDA #imm ; STA $1DFC ; ...` — code patches the operand byte of a
# JSR (a,X) instruction sitting in WRAM at $7E:1DFC, then JMP/JSL's
# to it. Decoders that scan the same byte sequence under M=0 get
# fooled: bytes `A9 imm 8D FC 1D` parse as M=0 `LDA #$8DXX ; STA $1DFC`
# but bytes `imm 8D FC 1D yy` starting one byte later parse as
# `... JSR ($yy1D,X)`. The JSR (a,X) decode is a phantom from the
# wrong-mode entry; real execution under M=1 never lands on it.
SMC_TARGET_ADDRS = {0x1DFC}


@dataclass
class BoundsResult:
    candidate_count: Optional[int]
    confidence: str    # 'high' | 'medium' | 'low'
    detected: List[str] = field(default_factory=list)
    notes: List[str] = field(default_factory=list)
    smc_suspicion: bool = False     # STA-to-WRAM ($1DFC etc.) seen
    smc_evidence: List[str] = field(default_factory=list)


def detect_smc_phantom(
    rom: bytes,
    site_pc24: int,
    table_base: int,
) -> Tuple[bool, List[str]]:
    """Detect the SMW SMC-dispatch phantom-decode pattern.

    Under M=1, SMW emits `LDA #imm ; STA $1DFC ; ...` (5 bytes total:
    A9 imm 8D FC 1D). Under M=0 those same bytes parse as
    `LDA #$8Dimm ; JSR ($XX1D,X)` (3 + 3 bytes), because under M=0
    the LDA-immediate is 16-bit, so it consumes the 8D byte that was
    the STA opcode under M=1, AND the next byte (FC) becomes a JSR
    (a,X) opcode followed by 1D and the next byte (XX) as the operand.

    The decoder reaches the M=0 site only when some upstream REP
    cleared M while the function was decoded under M=1 entry. In real
    M=1-only execution the JSR (a,X) at site_pc24 is dead bytes —
    they're consumed as STA's operand.

    Signature for the M=0 decode:
      ROM byte at (site_pc24 - 1) ∈ {0x8D, 0x99, 0x9D}
        (STA abs / STA abs,Y / STA abs,X — all 3-byte instructions
        under M=1 that consume site_pc24 and site_pc24+1 as their
        operand bytes)

    Under M=1 the 3-byte STA at site_pc24-1 absorbs the FC byte
    (which becomes the STA operand's low byte) and the byte at
    site_pc24+1 (the STA operand's high byte). Under M=0 the LDA #imm
    starting at site_pc24-3 is 3 bytes, ending at site_pc24-1 (which
    becomes the high byte of the M=0 immediate); the next insn at
    site_pc24 is then decoded as JSR (a,X).

    Returns (suspect: bool, evidence: List[str]).
    """
    bank = (site_pc24 >> 16) & 0xFF
    pc = site_pc24 & 0xFFFF
    if pc < 0x8001:
        return False, []
    try:
        prev_off = snes.lorom_offset(bank, pc - 1)
    except AssertionError:
        return False, []
    if prev_off >= len(rom):
        return False, []
    prev_byte = rom[prev_off]
    # 3-byte STA-family opcodes that, under M=1, consume the FC byte
    # at site_pc24 and the next byte as their operand.
    STA_3BYTE_OPS = {0x8D: 'STA abs', 0x99: 'STA abs,Y', 0x9D: 'STA abs,X'}
    if prev_byte not in STA_3BYTE_OPS:
        return False, []
    sta_kind = STA_3BYTE_OPS[prev_byte]
    # Read the M=1 STA's effective target = bytes (site_pc24,
    # site_pc24+1) little-endian.
    try:
        next_off = snes.lorom_offset(bank, pc)
    except AssertionError:
        return False, []
    sta_lo = rom[next_off]                              # = 0xFC (JSR opcode under M=0)
    sta_hi = rom[next_off + 1] if next_off + 1 < len(rom) else 0
    sta_target = sta_lo | (sta_hi << 8)
    # The canonical SMW SMC-dispatch slot is $1DFC (WRAM mirror).
    # Other near-WRAM slots (< $2000) are also plausible.
    canonical_slot = sta_target == 0x1DFC
    plausible_slot = sta_target < 0x2000
    if not plausible_slot:
        # STA target outside WRAM — could still be SMC into ROM mirror
        # or work-RAM via cartridge mapper, but mark only if the
        # opcode + alignment match strongly suggests a phantom.
        # Don't suppress — leave the decision to the human reviewer
        # by flagging anyway with a softer note.
        pass
    evidence = [
        f'ROM byte at ${bank:02X}:{pc-1:04X} == ${prev_byte:02X} '
        f'({sta_kind} opcode under M=1)',
        f'M=1 STA target = ${sta_target:04X}'
        + (' (canonical SMW SMC-dispatch slot $1DFC)' if canonical_slot
           else ' (WRAM' if plausible_slot
           else ' (unusual — outside WRAM mirror)'),
        f'M=1 reinterpretation: LDA #imm ; {sta_kind} ${sta_target:04X}'
        + (',Y' if prev_byte == 0x99 else (',X' if prev_byte == 0x9D else '')),
    ]
    return True, evidence


def detect_bounds(chain: List[DecodedInsn]) -> BoundsResult:
    """Scan the control-flow predecessor chain for index-bound patterns.

    chain is ordered earliest→latest, so chain[-1] is the insn just
    before the JSR. Looks for, anywhere in the chain:
      CMP #$nn    → bound = nn   (gates A; assumes A→X via TAX/TAY/ASL)
      CPX #$nn    → bound = nn   (canonical 65816 bound check)
      CPY #$nn    → bound = nn
      AND #$nn    → mask; bound = nn + 1 if nn = 2^k - 1
      ASL A       → ×2 stride; one occurrence ⇒ short table (16-bit)
      ASL A; ASL A → ×4 stride; rare
      TAX/TAY     → which register became the dispatch index
      BCS/BCC/BEQ/BNE/BMI/BPL → branch shape (recorded but not gated on)
    """
    if not chain:
        return BoundsResult(candidate_count=None, confidence='low',
                            notes=['chain empty (decoder did not reach site)'])

    bound_imm: Optional[int] = None
    bound_kind: Optional[str] = None  # 'CMP'/'CPX'/'CPY'/'AND'
    asl_count = 0
    transfer: Optional[str] = None
    detected: List[str] = []
    smc_suspicion = False
    smc_evidence: List[str] = []

    # Walk the chain in order so the LAST matching CMP/CPX/AND wins
    # (the bound check immediately preceding the JSR is what gates X).
    for d in chain:
        mnem = d.insn.mnem
        mode = d.insn.mode
        operand = d.insn.operand
        if mnem in ('CMP', 'CPX', 'CPY') and mode == snes.IMM:
            # Reject obviously-out-of-range immediates — they're decode
            # noise from wrong-mode parsing of byte-stream bytes.
            if operand <= _MAX_PLAUSIBLE_TABLE_SIZE:
                bound_imm = operand
                bound_kind = mnem
                detected.append(f'{mnem} #${operand:02X}')
            else:
                detected.append(f'{mnem} #${operand:04X} (rejected: > '
                                f'{_MAX_PLAUSIBLE_TABLE_SIZE} = noise)')
        elif mnem == 'AND' and mode == snes.IMM:
            if operand and (operand & (operand + 1)) == 0 \
                    and operand + 1 <= _MAX_PLAUSIBLE_TABLE_SIZE:
                bound_imm = operand + 1
                bound_kind = 'AND'
                detected.append(f'AND #${operand:02X} (mask, count {operand+1})')
            else:
                detected.append(f'AND #${operand:02X} (mask not usable)')
        elif mnem == 'ASL' and mode == snes.ACC:
            asl_count += 1
            detected.append('ASL A')
        elif mnem in ('TAX', 'TAY', 'TXA', 'TYA', 'TXY', 'TYX'):
            transfer = mnem
            detected.append(mnem)
        elif mnem in ('BCS', 'BCC', 'BNE', 'BEQ', 'BMI', 'BPL', 'BVC', 'BVS'):
            detected.append(mnem)
        # SMC-dispatch suspicion: STA-to-WRAM-low (e.g. $1DFC) right
        # before this JSR strongly suggests the JSR is a wrong-mode
        # decode of bytes that real M=1 execution consumes as the STA's
        # operand. Look for STA absolute / STA dp pointing at a known
        # SMC target (or any address < $2000, the WRAM mirror window).
        if mnem == 'STA':
            target = operand & 0xFFFF
            if target in SMC_TARGET_ADDRS:
                smc_suspicion = True
                smc_evidence.append(
                    f'STA ${target:04X} at ${(d.key.pc >> 16) & 0xFF:02X}:'
                    f'{d.key.pc & 0xFFFF:04X} (canonical SMW SMC dispatch slot)'
                )
            elif target < 0x2000:
                smc_evidence.append(
                    f'STA ${target:04X} (WRAM target, possible SMC patch)'
                )

    has_bound = bound_imm is not None
    has_stride = asl_count >= 1
    has_index_route = transfer in ('TAX', 'TAY') or asl_count >= 1
    notes: List[str] = []
    if asl_count >= 2:
        notes.append(f'ASL count = {asl_count} (×{1 << asl_count} stride)')
    if bound_kind == 'CMP' and transfer != 'TAX':
        notes.append('CMP gates A; verify A→X via TAX or shifts before JSR')
    if smc_suspicion:
        notes.append(
            'SMC-dispatch suspected: predecessor writes to $1DFC. The '
            'bytes decoded as JSR (abs,X) are likely the operand of a '
            'STA $1DFC under M=1 execution; the JSR decode is a phantom '
            'from a wrong-mode (M=0) entry. Verify with runtime trap '
            'before authoring a static dispatch table.'
        )

    if smc_suspicion:
        # SMC suspicion overrides the bound/stride heuristic — even
        # when those exist, they're parsed from byte stream that
        # might not be real instructions.
        conf = 'low'
    elif has_bound and has_stride and has_index_route:
        conf = 'high'
    elif has_bound or has_stride:
        conf = 'medium'
    else:
        conf = 'low'

    return BoundsResult(
        candidate_count=bound_imm,
        confidence=conf,
        detected=detected,
        notes=notes,
        smc_suspicion=smc_suspicion,
        smc_evidence=smc_evidence,
    )


# ---------- table dump ---------------------------------------------

@dataclass
class TableDump:
    location: str        # 'rom' | 'ram' | 'unknown'
    base: int
    entries: List[int] = field(default_factory=list)
    note: str = ''


def dump_table(rom: bytes, bank: int, base: int, count: int = 8) -> TableDump:
    """Read `count` 16-bit entries at `bank:base` from ROM.

    Operand < $8000 ⇒ table is in WRAM/SRAM/I-O area in this bank's
    address space (LoROM mapping puts ROM at $8000-$FFFF only; lower
    half is hardware/RAM). Mark as ram-runtime in that case.
    """
    if base < 0x8000:
        return TableDump(location='ram', base=base, entries=[],
                         note='operand below $8000 — RAM/MMIO region, '
                              'not a static ROM table')

    try:
        offset = snes.lorom_offset(bank, base)
    except AssertionError:
        return TableDump(location='unknown', base=(bank << 16) | base,
                         note='lorom_offset rejected the base')

    entries: List[int] = []
    invalid = 0
    for i in range(count):
        off = offset + i * 2
        if off + 1 >= len(rom):
            break
        e = rom[off] | (rom[off + 1] << 8)
        entries.append(e)
        if not (0x8000 <= e <= 0xFFFF):
            invalid += 1
    note = ''
    if invalid:
        note = f'{invalid}/{len(entries)} entries outside $8000-$FFFF (likely overshot table end)'
    return TableDump(location='rom', base=(bank << 16) | base,
                     entries=entries, note=note)


# ---------- top-level: enrich a CALL_INDIRECT site ------------------

@dataclass
class AsmContext:
    site_pc24: int
    table_base: int
    function: str
    chain_before: List[str]      # control-flow predecessor chain (earliest→latest)
    site_line: Optional[str]
    chain_after: List[str]       # successor PCs (linear)
    linear_window: List[str]     # PC-sorted view (pre + site + post)
    bounds: BoundsResult
    table: TableDump
    classification: str          # 'rom-static' | 'ram-runtime' | 'undecodable'
    confidence: str
    decode_status: str           # 'reached' | 'not_reached' | 'no_cfg'

    def to_dict(self):
        return {
            'site_pc24': f'{self.site_pc24:06X}',
            'table_base': f'{self.table_base:04X}',
            'function': self.function,
            'chain_before': self.chain_before,
            'site_line': self.site_line,
            'chain_after': self.chain_after,
            'linear_window': self.linear_window,
            'bounds': {
                'candidate_count': self.bounds.candidate_count,
                'confidence': self.bounds.confidence,
                'detected': self.bounds.detected,
                'notes': self.bounds.notes,
                'smc_suspicion': self.bounds.smc_suspicion,
                'smc_evidence': self.bounds.smc_evidence,
            },
            'table': asdict(self.table),
            'classification': self.classification,
            'confidence': self.confidence,
            'decode_status': self.decode_status,
        }


# Cache decoded function graphs across enrich_site calls — RunPlayerBlockCode
# alone has ≥4 variants and we'd otherwise re-decode it for each.
_GRAPH_CACHE: Dict[Tuple[int, int, int, int, Optional[int]], FunctionDecodeGraph] = {}


def _get_graph(rom: bytes, bank: int, entry_pc16: int,
               entry_m: int, entry_x: int,
               end16: Optional[int]) -> FunctionDecodeGraph:
    key = (bank, entry_pc16, entry_m, entry_x, end16)
    g = _GRAPH_CACHE.get(key)
    if g is None:
        g = decode_function(rom, bank, entry_pc16, entry_m, entry_x, end=end16)
        _GRAPH_CACHE[key] = g
    return g


def enrich_site(
    rom: bytes,
    cfg_funcs: Dict[str, CfgFunc],
    site_pc24: int,
    table_base: int,
    function_name: str,
    *,
    pre_window: int = 8,
    post_window: int = 4,
    table_dump_count: int = 8,
) -> AsmContext:
    """Build the full asm-context + bounds + table record for one site."""
    bank = (site_pc24 >> 16) & 0xFF
    site_pc16 = site_pc24 & 0xFFFF

    base_name, entry_m, entry_x = strip_variant(function_name)
    cfunc = cfg_funcs.get(base_name) or cfg_funcs.get(function_name)

    classification = 'rom-static' if table_base >= 0x8000 else 'ram-runtime'

    if cfunc is None:
        bounds = BoundsResult(candidate_count=None, confidence='low',
                              notes=[f'no cfg entry for {base_name}'])
        smc_phantom, smc_ev = detect_smc_phantom(rom, site_pc24, table_base)
        if smc_phantom:
            bounds.smc_suspicion = True
            bounds.smc_evidence = smc_ev
            classification = 'rom-static (PHANTOM — SMC under M=1)'
        table = dump_table(rom, bank, table_base, count=table_dump_count)
        return AsmContext(
            site_pc24=site_pc24, table_base=table_base, function=function_name,
            chain_before=[], site_line=None, chain_after=[], linear_window=[],
            bounds=bounds, table=table,
            classification=classification, confidence='low',
            decode_status='no_cfg',
        )

    try:
        graph = _get_graph(rom, cfunc.bank, cfunc.pc16,
                           entry_m, entry_x, cfunc.end16)
    except Exception as e:
        bounds = BoundsResult(candidate_count=None, confidence='low',
                              notes=[f'decode_function failed: {e}'])
        table = dump_table(rom, bank, table_base, count=table_dump_count)
        return AsmContext(
            site_pc24=site_pc24, table_base=table_base, function=function_name,
            chain_before=[], site_line=None, chain_after=[], linear_window=[],
            bounds=bounds, table=table,
            classification=classification, confidence='low',
            decode_status='not_reached',
        )

    site_keys = find_keys_for_pc(graph, site_pc24)
    if not site_keys:
        bounds = BoundsResult(
            candidate_count=None, confidence='low',
            notes=[f'site PC ${site_pc24:06X} not in decoded graph for '
                   f'{base_name} (entry ${cfunc.bank:02X}:{cfunc.pc16:04X}, '
                   f'm={entry_m} x={entry_x}); '
                   f'graph has {len(graph.insns)} insns'],
        )
        table = dump_table(rom, bank, table_base, count=table_dump_count)
        return AsmContext(
            site_pc24=site_pc24, table_base=table_base, function=function_name,
            chain_before=[], site_line=None, chain_after=[], linear_window=[],
            bounds=bounds, table=table,
            classification=classification, confidence='low',
            decode_status='not_reached',
        )

    # Pick the variant matching our (entry_m, entry_x) if multiple keys
    # share the PC; else first.
    site_key = next((k for k in site_keys
                     if k.m == entry_m and k.x == entry_x),
                    site_keys[0])
    site_di = graph.insns[site_key]

    preds = build_pred_map(graph)
    cf_chain = walk_back_chain(graph, preds, site_key, max_depth=12)
    chain_before = [fmt_insn(d) for d in cf_chain[-pre_window:]]
    site_line = fmt_insn(site_di)
    # Successors of JSR (a,X) — likely empty (we don't expand the
    # dispatch table here), but include any that the graph recorded.
    after = []
    for s_key in site_di.successors:
        d = graph.insns.get(s_key)
        if d:
            after.append(fmt_insn(d))
            if len(after) >= post_window:
                break
    # If no graph-side successors, fall back to linear PC-after.
    pre_rows, _, post_rows = slice_around_pc(graph, site_key,
                                              pre=pre_window, post=post_window)
    chain_after = after if after else [fmt_insn(d) for d in post_rows]
    linear_window = (
        [fmt_insn(d) for d in pre_rows]
        + ['  ' + site_line + '   <-- JSR site']
        + [fmt_insn(d) for d in post_rows]
    )

    bounds = detect_bounds(cf_chain)

    # SMC-phantom detector — direct ROM-byte pattern match; runs
    # independently of the predecessor-chain heuristic. The two paths
    # are unioned: a site flagged by either is treated as suspect.
    smc_phantom, smc_phantom_ev = detect_smc_phantom(
        rom, site_pc24, table_base
    )
    if smc_phantom:
        bounds.smc_suspicion = True
        bounds.smc_evidence = list(bounds.smc_evidence) + smc_phantom_ev

    table = dump_table(rom, bank, table_base, count=max(
        bounds.candidate_count or 0, table_dump_count
    ))

    if bounds.smc_suspicion:
        confidence = 'low'
        classification = 'rom-static (PHANTOM — SMC under M=1)'
    elif classification == 'ram-runtime':
        confidence = 'low'
    else:
        confidence = bounds.confidence

    return AsmContext(
        site_pc24=site_pc24, table_base=table_base, function=function_name,
        chain_before=chain_before, site_line=site_line, chain_after=chain_after,
        linear_window=linear_window,
        bounds=bounds, table=table,
        classification=classification, confidence=confidence,
        decode_status='reached',
    )


def format_asm_context(ctx: AsmContext) -> str:
    out: List[str] = []
    pc = ctx.site_pc24
    bank = (pc >> 16) & 0xFF
    out.append('-' * 78)
    out.append(f'  site:        ${bank:02X}:{pc & 0xFFFF:04X}')
    out.append(f'  table base:  ${ctx.table_base:04X}    [{ctx.classification}]')
    out.append(f'  function:    {ctx.function}')
    out.append(f'  confidence:  {ctx.confidence}    decode: {ctx.decode_status}')
    out.append('')
    if ctx.linear_window:
        out.append('  asm window (linear PC view):')
        for ln in ctx.linear_window:
            out.append(f'    {ln}')
    elif ctx.bounds.notes:
        out.append('  asm window: (unavailable)')
        for n in ctx.bounds.notes:
            out.append(f'    note: {n}')
    if ctx.chain_before:
        out.append('')
        out.append('  control-flow predecessor chain (earliest -> latest):')
        for ln in ctx.chain_before:
            out.append(f'    {ln}')
        if ctx.site_line:
            out.append(f'    {ctx.site_line}    <-- JSR')
    out.append('')
    out.append('  bounds:')
    if ctx.bounds.detected:
        out.append('    detected: ' + ', '.join(ctx.bounds.detected))
    else:
        out.append('    detected: (none)')
    out.append(f'    candidate count: {ctx.bounds.candidate_count}')
    out.append(f'    confidence:      {ctx.bounds.confidence}')
    if ctx.bounds.smc_suspicion:
        out.append('    SMC SUSPICION:   YES — likely phantom JSR (a,X) decode')
    for ev in ctx.bounds.smc_evidence:
        out.append(f'    SMC evidence: {ev}')
    for n in ctx.bounds.notes:
        out.append(f'    note: {n}')
    out.append('')
    out.append('  table dump:')
    if ctx.table.location == 'ram':
        out.append('    [RAM table - deferred / runtime-dispatch required]')
        if ctx.table.note:
            out.append(f'    {ctx.table.note}')
    elif ctx.table.location == 'unknown':
        out.append(f'    [unknown - {ctx.table.note}]')
    else:
        out.append(f'    {len(ctx.table.entries)} entries at '
                   f'${(ctx.table.base >> 16) & 0xFF:02X}:'
                   f'{ctx.table.base & 0xFFFF:04X}:')
        for i, e in enumerate(ctx.table.entries):
            marker = '' if 0x8000 <= e <= 0xFFFF else '  <-- outside ROM'
            out.append(f'      [{i:>2}] -> ${(ctx.table.base >> 16) & 0xFF:02X}:'
                       f'{e:04X}{marker}')
        if ctx.table.note:
            out.append(f'    note: {ctx.table.note}')
    return '\n'.join(out)
