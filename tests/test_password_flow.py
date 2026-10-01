"""Exercise production password wrappers, original restore, autosave and Continue.

The production import loads an unedited original Password-screen snapshot and
queues symbols through the same wrapper as the native UI. Only original guest
instructions restore the campaign.
"""
from pathlib import Path
import hashlib
import os
import struct
import subprocess

import pytest
from test_password_codec import codec, call, ROM, ROOT

EXE = ROOT / "build/ISSDNative.exe"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's ROM")


def guest(folder, frames, *arguments):
    result = subprocess.run(
        [str(EXE), "--rom", str(ROM), "--headless", str(frames),
         "--config", str(folder / "isolated.cfg"),
         "--save-dir", str(folder / "owned-saves"), *arguments],
        cwd=folder, env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"),
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return result


def input_script(downs, campaign):
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "".join(f"{500+i*30} DOWN\n{510+i*30} NONE\n" for i in range(downs))
    script += "560 A\n570 NONE\n" if campaign else "620 A\n630 NONE\n"
    if campaign:
        script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1300, 100))
    return script


@pytest.fixture(scope="module")
def password_screen(tmp_path_factory):
    folder = tmp_path_factory.mktemp("original-password-screen")
    script = folder / "input.txt"
    script.write_text(input_script(3, False), encoding="ascii")
    guest(folder, 1302, "--script", str(script), "--save-state", "1302",
          "--dump-state", str(folder / "screen"))
    ram = (folder / "screen.wram").read_bytes()
    assert ram[0x1446:0x1449] == ram[0x1538:0x153b] == bytes.fromhex("20ea8a")
    envelope = (folder / "owned-saves/quicksave.sav").read_bytes()
    offset = envelope.find(ram)
    assert offset >= 192 and envelope.find(ram, offset + 1) == -1
    return envelope, ram


@pytest.mark.parametrize("world,progress", [(False, 0), (True, 0), (True, 3)],
                         ids=["actual-cup-setup", "actual-world-setup", "world-progress-team"])
def test_original_guest_restores_native_export(codec, password_screen, tmp_path, world, progress):
    script = tmp_path / "campaign.txt"
    script.write_text(input_script(2 if world else 1, True), encoding="ascii")
    production_export = tmp_path / "production.bin"
    guest(tmp_path, 1302 if world else 1120, "--script", str(script),
          "--dump-state", str(tmp_path / "campaign"),
          "--export-password-symbols", str(production_export))
    source = tmp_path / "campaign.wram"
    ram = bytearray(source.read_bytes())
    expected_flags = 0x21 if world else 0x205
    assert struct.unpack_from("<H", ram, 0x1648)[0] == expected_flags
    if progress:
        # Additional state uses the original field table's World progress and
        # team fields; the pure codec suite covers all six retail variants.
        struct.pack_into("<H", ram, 0x1652, progress)
        struct.pack_into("<H", ram, 0xda0, 62)
        expected_flags = 0x20  # A continuing World campaign, as actual result fixtures.
        struct.pack_into("<H", ram, 0x1648, expected_flags)
        source.write_bytes(ram)
    symbols = tmp_path / "native.bin"
    original = tmp_path / "original.bin"
    call(codec, tmp_path, source, "encode", original)
    assert "accepted=1" in call(codec, tmp_path, source, "native-encode", symbols)
    assert symbols.read_bytes() == original.read_bytes()
    if not progress:
        assert production_export.read_bytes() == symbols.read_bytes()
    assert len(symbols.read_bytes()) == (50 if world else 12)
    assert source.read_bytes() == ram

    envelope, screen = password_screen
    screen_path = tmp_path / "screen.wram"
    screen_path.write_bytes(screen)
    count = len(symbols.read_bytes())
    assert "accepted=1" in call(codec, tmp_path, screen_path, "native-submit", symbols, count)
    queued = (tmp_path / "submitted.wram").read_bytes()
    assert queued[0x1640:0x1698] == screen[0x1640:0x1698]
    assert queued[0x1446:0x1449] == bytes.fromhex("adea8a")
    # Load the unedited original screen; the production wrapper both queues
    # guest input and arms the import observer, exactly as the native UI does.
    imported = tmp_path / "imported"
    imported.mkdir()
    save_dir = imported / "owned-saves"
    save_dir.mkdir()
    (save_dir / "quicksave.sav").write_bytes(envelope)
    guest(imported, 600, "--load-state", "1", "--import-password-at", "2",
          "--import-password-symbols", str(symbols),
          "--dump-state", str(imported / "restored"))
    restored = (imported / "restored.wram").read_bytes()
    assert struct.unpack_from("<H", restored, 0x32)[0] == 6
    assert struct.unpack_from("<H", restored, 0x70)[0] == 12
    assert struct.unpack_from("<H", restored, 0x1648)[0] == expected_flags
    assert restored[0x1446:0x1449] == bytes.fromhex("14948b" if world else "3c958b")
    assert restored[0xda0:0xda2] == ram[0xda0:0xda2]
    if world:
        assert restored[0x1652:0x1654] == ram[0x1652:0x1654]
        assert restored[0x1656:0x1658] == ram[0x1656:0x1658]
    else:
        assert restored[0xdc00:0xdc03] == ram[0xdc00:0xdc03]
    saves = list((save_dir / "campaigns").glob("*/campaign.sav"))
    assert len(saves) == 1, "production import did not autosave its settled campaign"
    saved = saves[0].read_bytes()
    assert int.from_bytes(saved[24:32], "little") == 1, "import repeatedly autosaved"
    assert saved[160:192] == hashlib.sha256(saved[:160] + saved[192:]).digest()
    label = saved[80:144].split(b"\0")[0]
    assert label.startswith(b"World Series password: " if world else b"Cup password: ")
    guest(imported, 80, "--continue", "--dump-state", str(imported / "continued"))
    continued = (imported / "continued.wram").read_bytes()
    assert continued[0x1640:0x1698] == restored[0x1640:0x1698]
    assert continued[0xdc00:0xdf00] == restored[0xdc00:0xdf00]
    assert saves[0].read_bytes() == saved, "Continue immediately resaved the imported campaign"
