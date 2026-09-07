"""snesrecomp.recompiler.v2.wrapper_autoroute

Auto-detect and remediate cross-bank wrapper-bypass cfg aliases.

SMW (and similar SNES games) contain a recurring asm idiom: a tiny
PHB/PHK/PLB/JSR LO HI/PLB/RTL stub sits in some bank ahead of a
callable body. The stub's purpose is to transition the data bank (DB)
to the bank the body lives in, run the body, then restore DB. Cross-
bank callers `JSL <stub_pc>` so the DB is correct when the body runs.

The bypass class manifests when cfg aliases both the stub PC and the
body PC to the same function name — typically:

    bank01.cfg:  func HandleNormalSpriteLevelCollision 9140 ...   # body
                 name 019140 HandleNormalSpriteLevelCollision     # body
    bank02.cfg:  name 019138 HandleNormalSpriteLevelCollision     # stub→body

With no `name` (or `func`) of its own at $01:9138, the recompiler
emits the cross-bank caller's `JSL $01:9138` as a direct call to
`HandleNormalSpriteLevelCollision`, which is positioned at the body
PC ($01:9140). The wrapper bytes are never decoded; DB stays at the
caller's value; the body's `abs,X`/`abs,Y` ROM-table reads land in
the wrong bank.

Fix template (what humans have been doing in cfg one bypass at a time):
- Declare the stub PC as its own func with a wrapper-specific name.
- Re-route the cross-bank `name` alias to the wrapper-specific name.
- The body keeps its existing name; intra-bank `JSR <body_pc>` callers
  still resolve correctly.

This module does the same thing automatically. It runs as a pre-pass
after cfg load and before cross-bank `name` promotion. For every match
of the 8-byte SMW wrapper signature

    8B 4B AB 20 LO HI AB 6B   (PHB PHK PLB JSR LO HI PLB RTL)

in any bank's ROM where cfg also aliases the wrapper PC and the body
PC to the same function name, it rewrites the wrapper-PC alias to a
unique synthetic name. The cross-bank `name` → BankEntry promotion
that runs immediately afterward then creates a real BankEntry for the
wrapper, so it gets emitted as its own callable.

Public API:
    detect_and_route(parsed, name_map, cross_bank_names, rom) -> List[FixRecord]
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Set, Tuple

_THIS_DIR = Path(__file__).resolve().parent
_RECOMPILER_DIR = _THIS_DIR.parent
for p in (str(_THIS_DIR), str(_RECOMPILER_DIR)):
    if p not in sys.path:
        sys.path.insert(0, p)

from snes65816 import lorom_offset  # noqa: E402


# Byte signature for the canonical SMW DB-transition wrapper:
#   PHB (8B) / PHK (4B) / PLB (AB) / JSR abs (20 LO HI) / PLB (AB) / RTL (6B)
# Total 8 bytes. The two operand bytes (LO HI) are matched as wildcards.
_SIG_HEAD = bytes([0x8B, 0x4B, 0xAB, 0x20])   # PHB PHK PLB JSR
_SIG_TAIL = bytes([0xAB, 0x6B])               # PLB RTL
_SIG_LEN = 8


@dataclass(frozen=True)
class FixRecord:
    """One detected wrapper-bypass that was auto-routed."""
    bank: int                 # bank containing both wrapper and body
    wrapper_pc16: int         # 16-bit PC of the wrapper (also $8000+)
    body_pc16: int            # 16-bit PC of the body (JSR target)
    orig_name: str            # the function name shared by both aliases
    synthetic_name: str       # the unique name now bound to the wrapper PC

    @property
    def wrapper_addr_24(self) -> int:
        return (self.bank << 16) | (self.wrapper_pc16 & 0xFFFF)

    @property
    def body_addr_24(self) -> int:
        return (self.bank << 16) | (self.body_pc16 & 0xFFFF)


def _scan_bank_for_wrappers(rom: bytes, bank: int) -> List[Tuple[int, int]]:
    """Scan one bank's ROM ($8000-$FFFF) for the wrapper signature.

    Returns list of (wrapper_pc16, body_pc16) pairs. Both PCs are in
    the bank's local 16-bit address space ($8000-$FFFF).
    """
    bank_start = lorom_offset(bank, 0x8000)
    bank_end = bank_start + 0x8000
    if bank_end > len(rom):
        return []
    hits: List[Tuple[int, int]] = []
    end_off = bank_end - _SIG_LEN
    off = bank_start
    while off <= end_off:
        if (rom[off:off + 4] == _SIG_HEAD
                and rom[off + 6:off + 8] == _SIG_TAIL):
            wrapper_pc16 = 0x8000 + (off - bank_start)
            body_pc16 = rom[off + 4] | (rom[off + 5] << 8)
            # Body must be in this bank's ROM range and not the wrapper itself.
            if 0x8000 <= body_pc16 <= 0xFFFF and body_pc16 != wrapper_pc16:
                hits.append((wrapper_pc16, body_pc16))
            off += _SIG_LEN  # disjoint hits only
        else:
            off += 1
    return hits


def _build_synthetic_name(orig: str, bank: int, wrapper_pc16: int) -> str:
    """Mangle a unique wrapper-specific name. Includes the bank+PC to
    keep it stable across regens and distinguish multiple wrappers that
    happen to alias the same orig name (defensive, unlikely in practice)."""
    return f"_AutoWrap_{orig}__{bank:02X}_{wrapper_pc16:04X}"


def detect_and_route(parsed, name_map: Dict[int, str],
                      cross_bank_names: Dict[int, list],
                      rom: bytes) -> List[FixRecord]:
    """Detect SMW PHB/PHK/PLB/JSR/PLB/RTL wrapper-bypass cfg aliases and
    rewrite them in place.

    Arguments mirror the v2_regen pre-emit state:
      parsed:           list of (bank, cfg_path, BankCfg) tuples
      name_map:         dict[addr_24] -> friendly_name
      cross_bank_names: dict[owning_bank] -> list[NameDecl]
      rom:              loaded ROM bytes

    Mutates `name_map` and the `name` field of `NameDecl` records in
    `cross_bank_names` so that each detected bypass's wrapper-PC alias
    now points at a unique synthetic name. The owning bank's cross-bank
    promotion pass will see the synthetic name as new and create a
    fresh BankEntry at the wrapper PC, so the wrapper gets emitted.

    Returns the list of applied fixes (caller may log a summary).
    """
    # Build a name -> canonical declared PC map from every bank's
    # `func` declarations. These are the authoritative "where does this
    # function live" entries; a `name` alias targeting a different PC
    # means a caller will get re-routed to the func's declared PC, not
    # to the alias PC. That's the bypass condition.
    name_to_func_pc: Dict[str, int] = {}
    for bank, _cfg_path, cfg in parsed:
        for entry in cfg.entries:
            if entry.name:
                addr_24 = (bank << 16) | (entry.start & 0xFFFF)
                # First-seen wins; multiple banks declaring the same
                # name is a linker conflict resolved elsewhere.
                name_to_func_pc.setdefault(entry.name, addr_24)

    fixes: List[FixRecord] = []
    seen_wrapper_addrs: Set[int] = set()

    for bank, _cfg_path, _cfg in parsed:
        for wrapper_pc16, body_pc16 in _scan_bank_for_wrappers(rom, bank):
            wrapper_addr = (bank << 16) | wrapper_pc16
            body_addr = (bank << 16) | body_pc16
            if wrapper_addr in seen_wrapper_addrs:
                continue

            # Get the cfg-aliased name at the wrapper PC. No alias
            # means no caller routes through here; nothing to fix.
            wrapper_name = name_map.get(wrapper_addr)
            if wrapper_name is None:
                continue

            # Where is `wrapper_name` actually declared as a function?
            # Two cases trigger a bypass:
            #   1. declared_pc != wrapper_pc — the cfg points cross-
            #      bank callers at the wrapper PC, but the function
            #      they'll call lives elsewhere. Caller's JSL resolves
            #      to the elsewhere PC, skipping the wrapper. This
            #      covers BOTH the same-name double-alias (declared_pc
            #      == body_pc) AND the wrong-body alias (declared_pc
            #      == some unrelated body of a different wrapper).
            #   2. declared_pc is None — no `func` claims this name.
            #      Then `name <wrapper_pc> <fn>` will trigger the
            #      promotion at the wrapper PC, which is exactly what
            #      we want. No fix needed.
            declared_pc = name_to_func_pc.get(wrapper_name)
            if declared_pc is None:
                continue
            if declared_pc == wrapper_addr:
                continue  # wrapper has its own declaration — fine

            seen_wrapper_addrs.add(wrapper_addr)
            synth = _build_synthetic_name(wrapper_name, bank, wrapper_pc16)

            # Rewrite name_map for the wrapper PC.
            name_map[wrapper_addr] = synth

            # Rewrite every matching NameDecl in the wrapper's owning
            # bank's cross-bank list. There may be multiple if several
            # banks all alias this wrapper PC to the same name.
            owning_bank_list = cross_bank_names.get(bank, [])
            for nd in owning_bank_list:
                if ((nd.addr_24 & 0xFFFFFF) == wrapper_addr
                        and nd.name == wrapper_name):
                    nd.name = synth

            fixes.append(FixRecord(
                bank=bank,
                wrapper_pc16=wrapper_pc16,
                body_pc16=body_pc16,
                orig_name=wrapper_name,
                synthetic_name=synth,
            ))

    return fixes


def format_fix_summary(fixes: List[FixRecord]) -> str:
    """Build a human-readable summary block. Caller decides where to log."""
    if not fixes:
        return "  no wrapper-bypass cfg aliases detected"
    lines = [f"  detected {len(fixes)} wrapper-bypass alias(es); auto-routed:"]
    for fx in fixes:
        lines.append(
            f"    ${fx.bank:02X}:{fx.wrapper_pc16:04X} {fx.orig_name!r} "
            f"-> {fx.synthetic_name!r} (body at ${fx.bank:02X}:{fx.body_pc16:04X})"
        )
    return "\n".join(lines)
