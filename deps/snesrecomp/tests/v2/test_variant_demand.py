"""Whole-program M/X demand must constrain first-pass emission."""

import pathlib
import re
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]


def test_static_m1x1_call_does_not_manufacture_three_dead_variants():
    with tempfile.TemporaryDirectory() as raw:
        root = pathlib.Path(raw)
        rom = bytearray([0xFF] * 0x8000)
        rom[0:4] = bytes([
            0x20, 0x10, 0x80,  # $8000: JSR $8010
            0x60,              # $8003: RTS
        ])
        rom[0x10] = 0x60       # $8010: RTS
        rom_path = root / 'game.sfc'
        rom_path.write_bytes(rom)
        cfg_dir = root / 'cfg'
        cfg_dir.mkdir()
        (cfg_dir / 'bank00.cfg').write_text(
            'bank = 00\n'
            'func Caller 8000 end:8004\n'
            'func Callee 8010 end:8011\n',
            encoding='utf-8')
        out_dir = root / 'gen'

        result = subprocess.run([
            sys.executable, str(REPO / 'tools' / 'v2_regen.py'),
            '--rom', str(rom_path), '--cfg-dir', str(cfg_dir),
            '--out-dir', str(out_dir), '--jobs', '1',
        ], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr

        source = (out_dir / 'bank00_v2.c').read_text(encoding='utf-8')
        callee_variants = set(re.findall(
            r'\bCallee_M([01])X([01])\b', source))
        assert callee_variants == {('1', '1')}, callee_variants
        assert 'auto-promote pass 1: added 3 entries' not in result.stdout
        assert 'emit_pass_1' not in result.stdout


def test_undeclared_callees_keep_only_transitively_discovered_mode():
    """A reachable non-root JSR chain must not expand to all four widths.

    The first discovery pass can see Caller -> $8010 at M0X0 before $8010
    has been promoted into cfg.entries.  That exact demand must constrain
    first-pass codegen.  Once $8010 is promoted, its own JSR must discover
    $8020 at the same exact width.
    """
    with tempfile.TemporaryDirectory() as raw:
        root = pathlib.Path(raw)
        rom = bytearray([0xFF] * 0x8000)
        rom[0:6] = bytes([
            0xC2, 0x30,        # $8000: REP #$30 -> M0X0
            0x20, 0x10, 0x80,  # $8002: JSR $8010
            0x60,              # $8005: RTS
        ])
        rom[0x10:0x14] = bytes([
            0x20, 0x20, 0x80,  # $8010: JSR $8020
            0x60,              # $8013: RTS
        ])
        rom[0x20] = 0x60       # $8020: RTS
        rom_path = root / 'game.sfc'
        rom_path.write_bytes(rom)
        cfg_dir = root / 'cfg'
        cfg_dir.mkdir()
        (cfg_dir / 'bank00.cfg').write_text(
            'bank = 00\n'
            'func Caller 8000 end:8006\n'
            'symbol 8010 Callee\n'
            'symbol 8020 Grandchild\n',
            encoding='utf-8')
        out_dir = root / 'gen'

        result = subprocess.run([
            sys.executable, str(REPO / 'tools' / 'v2_regen.py'),
            '--rom', str(rom_path), '--cfg-dir', str(cfg_dir),
            '--out-dir', str(out_dir), '--jobs', '1',
        ], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr

        source = (out_dir / 'bank00_v2.c').read_text(encoding='utf-8')
        for name in ('bank_00_8010', 'bank_00_8020'):
            variants = set(re.findall(
                rf'\b{name}_M([01])X([01])\b', source))
            assert variants == {('0', '0')}, (name, variants)
        assert 'added 4 entries' not in result.stdout
