"""Cx4 (HG51B S169) data-ROM synthesis gate.

The Cx4's internal 1024-entry x 24-bit data ROM is not part of any game ROM, so
it used to be a file developers had to supply. It is not authored data: it is SIX
CLOSED-FORM MATHEMATICAL TABLES, and all 1024 entries reproduce bit-exactly. So
the engine synthesizes it and **no firmware file is required by anyone** — while
the emulation stays fully LLE, which is strictly better than HLE-ing the
requirement away.

    [  0.. 255]  floor(2**23 / n)                  reciprocal; n=0 saturates
    [256.. 511]  floor(sqrt(n) * 2**20)            square root
    [512.. 639]  floor(2**24 * sin(n*pi/256))      sine,     n=0..127
    [640.. 767]  floor((2**24/pi) * asin(n/128))   arcsine,  n=0..127
    [768.. 895]  floor(2**16 * tan(n*pi/256))      tangent,  n=0..127
    [896..1023]  floor(2**24 * cos(n*pi/256))      cosine,   n=0..127

This module is the gate. `synthesize_data_rom()` builds the table; the tests
assert it is bit-exact against a real dump when one is available, and assert the
structural invariants that make the double-precision implementation SAFE when one
is not.

A near-miss is a failure, deliberately. A data ROM off by one LSB produces subtly
wrong geometry in Mega Man X2/X3 — far worse than a loud error, because it looks
almost right and never announces itself.

MEASURED, and it is why every entry has to be right: on X2's boot self-test the
Cx4 program reads ALL 1024 entries exactly once each (`dbgprobe.py cx4` reports
`rdrom_block:[256,256,256,256]`, `rdrom_distinct:1024`). That linear full-table
sweep is the game checksumming its own coprocessor's data ROM — so there is no
"the games only use part of it" escape hatch, and the game itself is the final
oracle on the synthesis.

The reference dump is ROM data and is NOT committed. Point the tests at one with
  SNESRECOMP_CX4_ROM=<path>
or drop `cx4.rom` / `cx4.data.rom` beside this file, in the repo root, or one
level up. Without a dump the comparison tests report a NOTE and skip; the
self-consistency and precision-safety tests still run.
"""
from __future__ import annotations

import math
import os
from pathlib import Path

TWO24 = 1 << 24
SQRT_SCALE = 1 << 20
TAN_SCALE = 1 << 16
RECIPROCAL_NUMERATOR = 1 << 23
MAX24 = 0xFFFFFF

# (name, first index, count)
BLOCKS = (
    ("reciprocal", 0, 256),
    ("sqrt", 256, 256),
    ("sin", 512, 128),
    ("asin", 640, 128),
    ("tan", 768, 128),
    ("cos", 896, 128),
)


def synthesize_data_rom():
    """Build all 1024 entries. No None, no guesses — the table is fully solved.

    Mirrors `cx4_synthesize_data_rom()` in runner/src/snes/cx4.c exactly; if one
    changes, this test is what catches the drift.
    """
    rom = [0] * 1024

    for n in range(256):
        rom[n] = MAX24 if n == 0 else RECIPROCAL_NUMERATOR // n

    for n in range(256):
        # Integer-exact: floor(sqrt(n) * 2**20) == isqrt(n << 40). Deliberately
        # avoids trusting a double for this block at all.
        rom[256 + n] = math.isqrt(n << 40)

    for n in range(128):
        a = math.pi * n / 256.0
        rom[512 + n] = int(math.floor(TWO24 * math.sin(a)))
        rom[640 + n] = int(math.floor((TWO24 / math.pi) * math.asin(n / 128.0)))
        # tan(45deg) is exactly 1, but libm's tan(pi/4) lands just below it and
        # floor() would yield 0xFFFF instead of 0x10000.
        rom[768 + n] = (TAN_SCALE if n == 64
                        else int(math.floor(TAN_SCALE * math.tan(a))))
        # cos(0) = 1 needs 2**24, which does not fit 24 bits: hardware saturates.
        rom[896 + n] = min(int(math.floor(TWO24 * math.cos(a))), MAX24)

    return rom


def block_of(index: int) -> str:
    for name, base, count in BLOCKS:
        if base <= index < base + count:
            return name
    return "?"


# ── reference dump discovery ──────────────────────────────────────────────

_CANDIDATE_NAMES = ("cx4.rom", "cx4.data.rom", "hg51bs169.data.rom")


def find_reference_dump():
    env = os.environ.get("SNESRECOMP_CX4_ROM")
    if env and Path(env).is_file():
        return Path(env)
    here = Path(__file__).resolve().parent
    for d in (here, here.parent, here.parent.parent):
        for name in _CANDIDATE_NAMES:
            p = d / name
            if p.is_file():
                return p
    return None


def decode_dump(path: Path):
    data = path.read_bytes()
    if len(data) != 3072:
        raise ValueError(f"{path}: {len(data)} bytes, expected exactly 3072")
    return [data[i * 3] | (data[i * 3 + 1] << 8) | (data[i * 3 + 2] << 16)
            for i in range(1024)]


# ── tests ─────────────────────────────────────────────────────────────────

def test_table_shape():
    rom = synthesize_data_rom()
    assert len(rom) == 1024
    for i, v in enumerate(rom):
        assert isinstance(v, int), f"entry {i} is not an int"
        assert 0 <= v <= MAX24, f"entry {i} = {v:#x} does not fit 24 bits"
    assert sum(c for _, _, c in BLOCKS) == 1024


def test_reciprocal_block_self_consistent():
    rom = synthesize_data_rom()
    assert rom[0] == MAX24            # 1/0 has no representation: saturate
    assert rom[1] == 0x800000
    assert rom[2] == 0x400000
    assert rom[4] == 0x200000
    for n in range(1, 256):
        v = rom[n]
        # floor(2**23 / n) is the largest k with k*n <= 2**23
        assert v * n <= RECIPROCAL_NUMERATOR < (v + 1) * n, f"n={n}"


def test_sqrt_block_self_consistent():
    rom = synthesize_data_rom()
    assert rom[256] == 0
    assert rom[256 + 1] == SQRT_SCALE
    assert rom[256 + 4] == 2 * SQRT_SCALE
    assert rom[256 + 64] == 8 * SQRT_SCALE
    for n in range(256):
        v = rom[256 + n]
        # v must be floor(sqrt(n)*2**20), checked with integers only
        assert v * v <= (n << 40) < (v + 1) * (v + 1), f"n={n}"


def test_trig_identities():
    """sin/cos quarter-wave mirror, and the known exact points."""
    rom = synthesize_data_rom()
    assert rom[512] == 0                      # sin(0)
    assert rom[640] == 0                      # asin(0)
    assert rom[768] == 0                      # tan(0)
    assert rom[768 + 64] == TAN_SCALE         # tan(45deg) == 1 exactly
    assert rom[896] == MAX24                  # cos(0) == 1, saturated
    # sin(x) == cos(90deg - x): sin block index n mirrors cos block index 128-n.
    for n in range(1, 128):
        assert rom[512 + n] == rom[896 + (128 - n)], (
            f"quarter-wave mirror broken at n={n}: "
            f"sin={rom[512 + n]:#x} cos={rom[896 + 128 - n]:#x}")


def test_double_precision_is_safe():
    """The synthesis must not depend on libm luck.

    Every trig entry is floor()ed, so an entry sitting a hair above an integer
    could tip the wrong way under a different libm. Assert that the only entries
    close to a floor boundary are the exact mathematical values we special-case.
    """
    close = []
    for n in range(128):
        a = math.pi * n / 256.0
        for base, val in ((512, TWO24 * math.sin(a)),
                          (640, (TWO24 / math.pi) * math.asin(n / 128.0)),
                          (768, TAN_SCALE * math.tan(a)),
                          (896, TWO24 * math.cos(a))):
            frac = val - math.floor(val)
            margin = min(frac, 1.0 - frac)
            if margin < 1e-6:
                close.append((base + n, val, margin))

    # Exactly the five exact values: sin(0), asin(0), tan(0), tan(45), cos(0).
    expected = {512, 640, 768, 768 + 64, 896}
    got = {idx for idx, _, _ in close}
    assert got == expected, (
        f"entries near a floor boundary changed: expected {sorted(expected)}, "
        f"got {sorted(got)}. A new borderline entry means the double-precision "
        f"synthesis is no longer provably libm-independent.")

    # And the nearest non-exact entry must have a wide margin.
    others = []
    for n in range(128):
        a = math.pi * n / 256.0
        for base, val in ((512, TWO24 * math.sin(a)),
                          (640, (TWO24 / math.pi) * math.asin(n / 128.0)),
                          (768, TAN_SCALE * math.tan(a)),
                          (896, TWO24 * math.cos(a))):
            if base + n in expected:
                continue
            frac = val - math.floor(val)
            others.append(min(frac, 1.0 - frac))
    assert min(others) > 1e-5, (
        f"closest non-exact entry is only {min(others):.2e} from a floor "
        f"boundary; double precision is no longer clearly safe")


def test_reference_dump_matches_synthesis():
    """THE GATE: all 1024 synthesized entries bit-exact against a real dump."""
    dump = find_reference_dump()
    if dump is None:
        print('  NOTE: cx4 data-ROM gate skipped - no dump available. Set '
              'SNESRECOMP_CX4_ROM to a 3072-byte dump. None is required to '
              'RUN; a dump only cross-checks the synthesis.')
        return
    real = decode_dump(dump)
    synth = synthesize_data_rom()

    bad = [(i, real[i], synth[i]) for i in range(1024) if real[i] != synth[i]]
    assert not bad, (
        f"{len(bad)}/1024 synthesized entries differ from {dump.name}; first: "
        + ", ".join(f"[{i}:{block_of(i)}] dump={a:#08x} synth={b:#08x}"
                    for i, a, b in bad[:5]))
    print(f"\n{dump.name}: 1024/1024 entries synthesized bit-exact "
          f"(no firmware file required)")


def test_reference_dump_per_block():
    """Per-block match counts, so a partial regression names its own block."""
    dump = find_reference_dump()
    if dump is None:
        print('  NOTE: cx4 per-block check skipped - no dump available.')
        return
    real = decode_dump(dump)
    synth = synthesize_data_rom()
    for name, base, count in BLOCKS:
        ok = sum(1 for n in range(count) if real[base + n] == synth[base + n])
        assert ok == count, f"block {name} [{base}..{base + count - 1}]: " \
                            f"only {ok}/{count} exact"


def test_reference_dump_block_structure():
    """Pin the structure any candidate generator must reproduce.

    NOTE: an earlier version of this test asserted that [512..767] showed a
    'tangent asymptote' — a signed max immediately followed by a signed min.
    That was WRONG: the data is monotonically increasing when read UNSIGNED, and
    the apparent peak/trough was an artefact of applying a signed 24-bit
    interpretation to it. The real structure is four monotone 128-entry curves.
    """
    dump = find_reference_dump()
    if dump is None:
        print('  NOTE: cx4 block-structure check skipped - no dump available.')
        return
    real = decode_dump(dump)

    # Each trig sub-block starts at its own hard reset, which is what marks the
    # 128-entry boundaries.
    assert real[512] == 0, "sin block does not start at 0"
    assert real[640] == 0, "asin block does not start at 0"
    assert real[768] == 0, "tan block does not start at 0"
    assert real[896] == MAX24, "cos block does not start saturated"

    # Monotonicity, read unsigned.
    for name, base, count in (("sin", 512, 128), ("asin", 640, 128),
                              ("tan", 768, 128)):
        rising = sum(1 for n in range(count - 1)
                     if real[base + n + 1] > real[base + n])
        assert rising == count - 1, f"{name} block not strictly increasing " \
                                    f"({rising}/{count - 1})"
    falling = sum(1 for n in range(127) if real[896 + n + 1] < real[896 + n])
    assert falling == 127, f"cos block not strictly decreasing ({falling}/127)"
