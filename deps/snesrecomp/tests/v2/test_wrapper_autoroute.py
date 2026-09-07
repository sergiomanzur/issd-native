"""Pin v2 wrapper_autoroute behaviour.

The SMW DB-transition wrapper byte signature is
    8B 4B AB 20 LO HI AB 6B   (PHB PHK PLB JSR LO HI PLB RTL)
sitting in some bank ahead of a callable body. Cross-bank `JSL <stub_pc>`
runs the stub so DB is set to the body's bank before the body's
`abs,X`/`abs,Y` ROM-table reads. cfg aliases that route cross-bank
callers AROUND the stub (to the body's PC) leave DB at the caller's
bank — silent miscompile.

`v2.wrapper_autoroute.detect_and_route` scans every bank's ROM for the
8-byte signature and rewrites cfg aliases at wrapper PCs to a synthetic
wrapper-specific name. The cross-bank-name -> BankEntry promotion that
runs immediately afterward then emits the wrapper as its own callable
with its PHB/PHK/PLB code.

Two bypass shapes are common in SMW:
  - same-name double-alias: cfg has `name <wrapper_pc> <fn>` AND
    `name <body_pc> <fn>` (or `func <fn> <body_pc>`). E.g. $01:9138
    HandleNormalSpriteLevelCollision.
  - wrong-body alias: cfg has `name <wrapper_pc> <fn>` where <fn> is
    declared at some OTHER body's PC (a different wrapper's body).
    E.g. $01:8042 GenericSprGfxRt0 aliased to GenericGFXRtDraw1Tile16x16
    which lives at $01:9F0D (the $90B2 wrapper's body, not $8042's).

Tests below cover both shapes plus the "wrapper already has a proper
func declaration" non-trigger case.
"""
from _helpers import make_lorom_bank0  # noqa: E402

from dataclasses import dataclass
from v2.wrapper_autoroute import detect_and_route  # noqa: E402


@dataclass
class _NameDecl:
    """Test stand-in for v2.cfg_loader.NameDecl — same shape (addr_24,
    name) used by the cross-bank-promotion logic in v2_regen.py."""
    addr_24: int
    name: str


@dataclass
class _BankEntry:
    """Test stand-in for v2.emit_bank.BankEntry — only `name` + `start`
    fields are read by the auto-router."""
    name: str
    start: int
    end: int = None
    entry_m: int = 1
    entry_x: int = 1


@dataclass
class _BankCfg:
    bank: int
    entries: list
    names: list


def _wrapper_bytes(body_lo: int, body_hi: int) -> bytes:
    """8-byte SMW DB-transition wrapper: PHB PHK PLB JSR LO HI PLB RTL."""
    return bytes([0x8B, 0x4B, 0xAB, 0x20, body_lo, body_hi, 0xAB, 0x6B])


def _empty_lorom(num_banks: int) -> bytes:
    """num_banks * 32KB of zeros."""
    return bytes(0x8000 * num_banks)


def test_detects_same_name_double_alias_bypass():
    """The Issue F-fall pattern: cfg aliases both wrapper and body to
    the SAME function name. Auto-router renames the wrapper-PC alias."""
    rom = bytearray(0x8000)  # bank 0 only
    # Wrapper at $00:8042, JSR target = body at $00:9F0D
    rom[0x0042:0x004A] = _wrapper_bytes(0x0D, 0x9F)
    rom_bytes = bytes(rom)

    # Body is declared as a func with the canonical name.
    body_pc = (0x00 << 16) | 0x9F0D
    wrapper_pc = (0x00 << 16) | 0x8042
    cfg = _BankCfg(
        bank=0x00,
        entries=[_BankEntry(name='HandleSpriteColl', start=0x9F0D)],
        names=[],
    )
    parsed = [(0x00, 'bank00.cfg', cfg)]

    # name_map mirrors v2_regen post-load state: BOTH wrapper PC and
    # body PC are aliased to 'HandleSpriteColl'.
    name_map = {wrapper_pc: 'HandleSpriteColl', body_pc: 'HandleSpriteColl'}

    # cross_bank_names: simulate a NameDecl at the wrapper PC, owned by
    # bank 0 (where the wrapper lives).
    wrapper_alias = _NameDecl(addr_24=wrapper_pc, name='HandleSpriteColl')
    cross_bank_names = {0x00: [wrapper_alias]}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert len(fixes) == 1
    fx = fixes[0]
    assert fx.bank == 0x00
    assert fx.wrapper_pc16 == 0x8042
    assert fx.body_pc16 == 0x9F0D
    assert fx.orig_name == 'HandleSpriteColl'
    assert fx.synthetic_name == '_AutoWrap_HandleSpriteColl__00_8042'

    # name_map at the wrapper PC was rewritten; body PC alias untouched.
    assert name_map[wrapper_pc] == '_AutoWrap_HandleSpriteColl__00_8042'
    assert name_map[body_pc] == 'HandleSpriteColl'
    # The NameDecl in cross_bank_names was renamed in place.
    assert wrapper_alias.name == '_AutoWrap_HandleSpriteColl__00_8042'


def test_detects_wrong_body_alias_bypass():
    """The Issue G pattern: cfg aliases wrapper to a DIFFERENT body
    name (a real func that lives at some other PC). Caller's JSL
    resolves to that other body's PC, fully bypassing the wrapper.
    Auto-router must catch this too."""
    rom = bytearray(0x8000)
    # Wrapper at $00:8042, JSR target = $00:9CF3 (this wrapper's REAL body)
    rom[0x0042:0x004A] = _wrapper_bytes(0xF3, 0x9C)
    rom_bytes = bytes(rom)

    wrapper_pc = (0x00 << 16) | 0x8042
    real_body_pc = (0x00 << 16) | 0x9CF3
    other_body_pc = (0x00 << 16) | 0x9F0D
    cfg = _BankCfg(
        bank=0x00,
        entries=[
            _BankEntry(name='Draw4TilesBody', start=0x9CF3),
            _BankEntry(name='Draw1TileBody', start=0x9F0D),
        ],
        names=[],
    )
    parsed = [(0x00, 'bank00.cfg', cfg)]

    # Cfg WRONG-aliases the $8042 wrapper to Draw1TileBody (which lives
    # at the OTHER wrapper's body PC $9F0D, not at $8042's body $9CF3).
    name_map = {
        wrapper_pc: 'Draw1TileBody',     # the bypass alias
        real_body_pc: 'Draw4TilesBody',
        other_body_pc: 'Draw1TileBody',
    }
    wrapper_alias = _NameDecl(addr_24=wrapper_pc, name='Draw1TileBody')
    cross_bank_names = {0x00: [wrapper_alias]}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert len(fixes) == 1
    fx = fixes[0]
    assert fx.wrapper_pc16 == 0x8042
    assert fx.body_pc16 == 0x9CF3   # the wrapper's JSR target
    assert fx.orig_name == 'Draw1TileBody'
    assert name_map[wrapper_pc] == '_AutoWrap_Draw1TileBody__00_8042'
    # Unrelated aliases untouched.
    assert name_map[real_body_pc] == 'Draw4TilesBody'
    assert name_map[other_body_pc] == 'Draw1TileBody'
    assert wrapper_alias.name == '_AutoWrap_Draw1TileBody__00_8042'


def test_no_fix_when_wrapper_has_own_func_declaration():
    """If cfg already declares the wrapper as its own func at the
    wrapper PC, no bypass exists — caller's name alias resolves to
    the wrapper's emit. Auto-router must NOT touch it."""
    rom = bytearray(0x8000)
    rom[0x0042:0x004A] = _wrapper_bytes(0x0D, 0x9F)
    rom_bytes = bytes(rom)

    wrapper_pc = (0x00 << 16) | 0x8042
    body_pc = (0x00 << 16) | 0x9F0D
    cfg = _BankCfg(
        bank=0x00,
        entries=[
            _BankEntry(name='WrapperFn', start=0x8042),  # wrapper IS declared
            _BankEntry(name='BodyFn', start=0x9F0D),
        ],
        names=[],
    )
    parsed = [(0x00, 'bank00.cfg', cfg)]
    name_map = {wrapper_pc: 'WrapperFn', body_pc: 'BodyFn'}
    cross_bank_names = {0x00: []}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert fixes == []
    assert name_map[wrapper_pc] == 'WrapperFn'   # unchanged
    assert name_map[body_pc] == 'BodyFn'


def test_no_fix_when_wrapper_pc_has_no_cfg_alias():
    """A wrapper with no caller routing through it (no `name` alias at
    the wrapper PC) needs no remediation — the bytes will be ignored
    by the recompiler entirely."""
    rom = bytearray(0x8000)
    rom[0x0042:0x004A] = _wrapper_bytes(0x0D, 0x9F)
    rom_bytes = bytes(rom)

    cfg = _BankCfg(
        bank=0x00,
        entries=[_BankEntry(name='BodyFn', start=0x9F0D)],
        names=[],
    )
    parsed = [(0x00, 'bank00.cfg', cfg)]
    name_map = {(0x00 << 16) | 0x9F0D: 'BodyFn'}
    cross_bank_names = {0x00: []}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert fixes == []


def test_signature_mismatch_is_not_detected_as_wrapper():
    """A region of ROM that happens to match part of the wrapper
    signature but not the full 8 bytes must be skipped."""
    rom = bytearray(0x8000)
    # PHB PHK PLB JSR LO HI ... but the tail is RTS (60) not PLB RTL
    rom[0x0042:0x004A] = bytes([0x8B, 0x4B, 0xAB, 0x20, 0x0D, 0x9F, 0x60, 0x60])
    rom_bytes = bytes(rom)

    wrapper_pc = (0x00 << 16) | 0x8042
    body_pc = (0x00 << 16) | 0x9F0D
    cfg = _BankCfg(
        bank=0x00,
        entries=[_BankEntry(name='BodyFn', start=0x9F0D)],
        names=[],
    )
    parsed = [(0x00, 'bank00.cfg', cfg)]
    name_map = {wrapper_pc: 'BodyFn', body_pc: 'BodyFn'}
    cross_bank_names = {0x00: [_NameDecl(addr_24=wrapper_pc, name='BodyFn')]}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert fixes == []


def test_body_in_different_bank_is_not_detected():
    """The wrapper's JSR target must be in the same bank — JSR is
    near-call, can't reach a different bank. If the matching bytes
    point at a PC < $8000 the entry is invalid; skip."""
    rom = bytearray(0x8000)
    # JSR operand $00:1234 (low RAM, not valid code) — must be rejected.
    rom[0x0042:0x004A] = _wrapper_bytes(0x34, 0x12)
    rom_bytes = bytes(rom)

    wrapper_pc = (0x00 << 16) | 0x8042
    cfg = _BankCfg(bank=0x00, entries=[], names=[])
    parsed = [(0x00, 'bank00.cfg', cfg)]
    name_map = {wrapper_pc: 'SomeName'}
    cross_bank_names = {0x00: []}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert fixes == []


def test_multiple_wrappers_in_one_bank():
    """Multiple disjoint wrappers in the same bank should all be
    detected independently."""
    rom = bytearray(0x8000)
    # Wrapper A at $00:8042 -> body at $00:9F0D
    rom[0x0042:0x004A] = _wrapper_bytes(0x0D, 0x9F)
    # Wrapper B at $00:90B2 -> body at $00:9F0D (same body, different wrapper)
    rom[0x10B2:0x10BA] = _wrapper_bytes(0x0D, 0x9F)
    rom_bytes = bytes(rom)

    wA = (0x00 << 16) | 0x8042
    wB = (0x00 << 16) | 0x90B2
    body = (0x00 << 16) | 0x9F0D
    cfg = _BankCfg(
        bank=0x00,
        entries=[_BankEntry(name='BodyFn', start=0x9F0D)],
        names=[],
    )
    parsed = [(0x00, 'bank00.cfg', cfg)]
    name_map = {wA: 'BodyFn', wB: 'BodyFn', body: 'BodyFn'}
    nd_A = _NameDecl(addr_24=wA, name='BodyFn')
    nd_B = _NameDecl(addr_24=wB, name='BodyFn')
    cross_bank_names = {0x00: [nd_A, nd_B]}

    fixes = detect_and_route(parsed, name_map, cross_bank_names, rom_bytes)
    assert len(fixes) == 2
    wrapper_pcs = sorted(fx.wrapper_pc16 for fx in fixes)
    assert wrapper_pcs == [0x8042, 0x90B2]
    # Each NameDecl got its OWN synthetic name (PC-suffixed so they
    # don't collide even though orig_name was identical).
    assert nd_A.name == '_AutoWrap_BodyFn__00_8042'
    assert nd_B.name == '_AutoWrap_BodyFn__00_90B2'
    assert nd_A.name != nd_B.name
