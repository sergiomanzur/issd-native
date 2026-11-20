import hashlib
import json
import subprocess
import sys
from pathlib import Path

import pytest

from test_config_persistence import compile_c

RETAIL_NAME = "International Superstar Soccer Deluxe (USA).sfc"

# Bytes the synthetic hack changes on purpose, spanning three reported
# categories: roster attributes, roster names and a head palette. Flipping the
# retail byte guarantees the value differs, so the converter always has a
# record to write. None of these touches the cartridge header.
CHANGED = (0x050000, 0x0381A0, 0x04DD70)


def test_converter_ips_applies_through_the_loader(tmp_path):
    """Cross-chain gate: the converter writes the patch, the C loader reads it.

    The hack is a copy of the user's retail cartridge with a few bytes flipped,
    written to the pytest scratch folder. No hack or ROM bytes enter the repo.
    """
    repo = Path(__file__).resolve().parents[1]
    rom = repo / RETAIL_NAME
    if not rom.exists():
        pytest.skip("requires user's supported cartridge ROM")

    retail = rom.read_bytes()
    hack = bytearray(retail)
    for offset in CHANGED:
        hack[offset] ^= 0xFF
    hack_path = tmp_path / "roundtrip_hack.sfc"
    hack_path.write_bytes(hack)

    stem = tmp_path / "roundtrip_hack"
    convert = subprocess.run(
        [
            sys.executable, str(repo / "tools/convert_rom_hack.py"),
            "--rom", str(hack_path),
            "--name", "Cross-chain probe",
            "--out", str(stem),
            "--base", str(rom),
        ],
        cwd=repo, capture_output=True, text=True,
    )
    assert convert.returncode == 0, convert.stdout + convert.stderr

    pack = json.loads(stem.with_suffix(".json").read_text(encoding="utf-8"))
    assert pack["cartridge"]["sha256"] == hashlib.sha256(hack).hexdigest()
    assert pack["cartridge"]["patch"] == "roundtrip_hack/roundtrip_hack.ips"

    exe = tmp_path / "test_cartridge_roundtrip.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_cartridge_roundtrip.c",
            repo / "ISSDNative/issd_cartridge.c",
            repo / "deps/snesrecomp/runner/src/sha256.c",
        ],
        [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"],
    )
    result = subprocess.run(
        [
            str(exe), str(tmp_path),
            str(stem / "roundtrip_hack.ips"), str(hack_path),
        ],
        cwd=repo, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "cartridge round-trip test passed" in result.stdout, result.stdout
