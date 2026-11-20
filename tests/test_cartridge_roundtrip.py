import hashlib
import json
import subprocess
import sys
from pathlib import Path

import pytest

from test_config_persistence import compile_c

RETAIL_NAME = "International Superstar Soccer Deluxe (USA).sfc"
RETAIL_SIZE = 2_097_152
COPIER_HEADER = 512

# Bytes the synthetic hack changes on purpose, spanning three reported
# categories: roster attributes, roster names and a head palette. Flipping the
# retail byte guarantees the value differs, so the converter always has a
# record to write. None of these touches the cartridge header.
CHANGED = (0x050000, 0x0381A0, 0x04DD70)

# A constant run of at least 16 bytes makes the converter emit an IPS RLE
# record, so the gate also proves the writer's RLE branch against the loader's
# reader instead of only literal records. It sits in the roster-attribute
# region, which the report calls known data.
RUN_AT = 0x050120
RUN_LEN = 32
RUN_FILL = 0x5A


def _normalized_retail(rom):
    """The user's cartridge, minus a 512-byte copier header when present."""
    data = rom.read_bytes()
    if len(data) == RETAIL_SIZE + COPIER_HEADER:
        data = data[COPIER_HEADER:]
    if len(data) != RETAIL_SIZE:
        pytest.skip(
            "retail cartridge is %d bytes; expected %d or %d"
            % (len(data), RETAIL_SIZE, RETAIL_SIZE + COPIER_HEADER)
        )
    return data


def _has_rle_record(patch):
    """True when the IPS carries at least one RLE record (length word 0)."""
    assert patch.startswith(b"PATCH")
    at = 5
    while at + 3 <= len(patch):
        if patch[at:at + 3] == b"EOF":
            return False
        at += 3
        size = int.from_bytes(patch[at:at + 2], "big")
        at += 2
        if size == 0:
            return True
        at += size
    return False


def test_converter_ips_applies_through_the_loader(tmp_path):
    """Cross-chain gate: the converter writes the patch, the C loader reads it.

    The hack is a copy of the user's retail cartridge with a few bytes flipped
    plus a constant run, written to the pytest scratch folder. No hack or ROM
    bytes enter the repo.
    """
    repo = Path(__file__).resolve().parents[1]
    rom = repo / RETAIL_NAME
    if not rom.exists():
        pytest.skip("requires user's supported cartridge ROM")

    retail = _normalized_retail(rom)
    hack = bytearray(retail)
    for offset in CHANGED:
        hack[offset] ^= 0xFF
    hack[RUN_AT:RUN_AT + RUN_LEN] = bytes([RUN_FILL]) * RUN_LEN
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

    patch_path = stem / "roundtrip_hack.ips"
    assert _has_rle_record(patch_path.read_bytes()), (
        "the converter emitted no RLE record, so this gate would not exercise "
        "the RLE path"
    )

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
            str(exe), str(tmp_path), str(patch_path), str(hack_path),
        ],
        cwd=repo, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "cartridge round-trip test passed" in result.stdout, result.stdout
