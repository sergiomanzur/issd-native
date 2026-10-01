"""Compare native password calls with original instructions in the owned ROM."""
from pathlib import Path
import json
import subprocess
import struct
import pytest
from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not ROM.exists(), reason="requires user's supported cartridge ROM")


@pytest.fixture(scope="module")
def codec(tmp_path_factory):
    folder = tmp_path_factory.mktemp("password-bin")
    wrapper = folder / "password.c"
    wrapper.write_text('#define ISSD_PASSWORD_NATIVE_TEST 1\n#include "' + (ROOT / "tests/test_password_codec.c").as_posix() + '"\n')
    exe = folder / "password.exe"
    compile_c(exe, ROOT, [wrapper, ROOT / "ISSDNative/issd_password.c", ROOT / "deps/snesrecomp/runner/src/sha256.c", ROOT / "deps/snesrecomp/runner/src/snes/interp816.c"], [ROOT / "ISSDNative", ROOT / "deps/snesrecomp/runner/src"])
    return exe


def call(exe, folder, ram, action, symbols, count=None):
    args = [str(exe), str(ROM), str(ram), action, str(symbols)]
    if count is not None: args.append(str(count))
    result = subprocess.run(args, cwd=folder, text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def fixture(folder, world=False):
    # Values observed in actual Cup setup1120 and World Series setup1302.
    ram = bytearray(0x20000)
    ram[0x1f9c] = 2
    if world:
        ram[0x1648] = 0x21; ram[0x1642] = 1; ram[0x1656] = 4; ram[0xda0] = 60
        expected = bytes.fromhex("00341e08000400310310") + bytes(40)
    else:
        struct.pack_into("<H",ram,0x1648,0x205)
        ram[0xdc00:0xdc03] = bytes([60,62,64])
        expected = bytes.fromhex("003c10010200002437070800")
    path = folder / "campaign.wram"; path.write_bytes(ram)
    return path, ram, expected


@pytest.mark.parametrize("world", [False, True], ids=["cup", "world"])
def test_cartridge_password_fixtures_round_trip(codec, tmp_path, world):
    path, ram, expected = fixture(tmp_path, world)
    original = tmp_path / "original.bin"
    call(codec, tmp_path, path, "encode", original)
    assert original.read_bytes() == expected
    native = tmp_path / "native.bin"
    assert "accepted=1" in call(codec, tmp_path, path, "native-encode", native)
    assert native.read_bytes() == expected
    assert path.read_bytes() == ram
    assert "accepted=1" in call(codec, tmp_path, path, "decode", native, len(expected))
    original_ram = (tmp_path / "decoded.wram").read_bytes()
    assert "accepted=1" in call(codec, tmp_path, path, "native-decode", original, len(expected))
    decoded = (tmp_path / "decoded.wram").read_bytes()
    assert decoded == original_ram


@pytest.mark.parametrize("damage", ["symbol", "checksum", "short", "long"])
def test_invalid_password_does_not_publish_staged_state(codec, tmp_path, damage):
    path, _, expected = fixture(tmp_path)
    bad = bytearray(expected)
    if damage == "symbol": bad[0] = 64
    elif damage == "checksum": bad[2] ^= 1
    elif damage == "short": bad = bad[:-1]
    else: bad += b"\0"
    symbols = tmp_path / "bad.bin"; symbols.write_bytes(bad)
    assert "accepted=0" in call(codec, tmp_path, path, "native-decode", symbols, len(bad))
    assert (tmp_path / "decoded.wram").read_bytes() == b"\xaa" * 0x20000


def test_submit_requires_verified_original_screen_and_preserves_invalid_input(codec, tmp_path):
    path, ram, expected = fixture(tmp_path)
    symbols = tmp_path / "cup.bin"; symbols.write_bytes(expected)
    assert "accepted=0" in call(codec, tmp_path, path, "native-submit", symbols, len(expected))
    assert (tmp_path / "submitted.wram").read_bytes() == ram

    ram[0x32] = 6; ram[0x70] = 12
    ram[0x1446:0x1449] = ram[0x1538:0x153b] = bytes.fromhex("20ea8a")
    path.write_bytes(ram)
    assert "accepted=1" in call(codec, tmp_path, path, "native-submit", symbols, len(expected))
    after = (tmp_path / "submitted.wram").read_bytes()
    assert after[0xe2d0:0xe2d0 + len(expected)] == expected
    assert after[0xe2d0 + len(expected):0xe2d0 + 60] == b"\xff" * (60 - len(expected))
    for address, value in [(0x1542,12),(0x1546,12),(0x1548,2),(0x1544,0x47)]:
        assert struct.unpack_from("<H",after,address)[0] == value
    assert after[0x1446:0x1449] == after[0x1538:0x153b] == bytes.fromhex("adea8a")
    # Submission queues only the original task protocol; campaign fields await
    # the cartridge's actual next-frame decoder/restore path.
    assert after[0x1640:0x1698] == ram[0x1640:0x1698]
    bad = bytearray(expected); bad[0] = 64; symbols.write_bytes(bad)
    assert "accepted=0" in call(codec,tmp_path,path,"native-submit",symbols,len(bad))
    assert (tmp_path / "submitted.wram").read_bytes() == ram
    bad = bytearray(expected); bad[2] ^= 1; symbols.write_bytes(bad)
    assert "accepted=0" in call(codec,tmp_path,path,"native-submit",symbols,len(bad))
    assert (tmp_path / "submitted.wram").read_bytes() == ram


@pytest.mark.parametrize("address,value", [(0x32,4),(0x70,8),(0x1446,0xeaad),
                                         (0x1538,0xeaad),(0x154e,1),(0x1460,1),(0x1462,1)])
def test_submit_rejects_busy_fading_or_other_original_tasks(codec,tmp_path,address,value):
    path, ram, expected = fixture(tmp_path)
    ram[0x32] = 6; ram[0x70] = 12
    ram[0x1446:0x1449] = ram[0x1538:0x153b] = bytes.fromhex("20ea8a")
    struct.pack_into("<H",ram,address,value); path.write_bytes(ram)
    symbols = tmp_path / "cup.bin"; symbols.write_bytes(expected)
    assert "accepted=0" in call(codec,tmp_path,path,"native-submit",symbols,len(expected))
    assert (tmp_path / "submitted.wram").read_bytes() == ram


def test_gameplay_context_restrictions_and_complete_glyph_domain(codec, tmp_path):
    path, _, _ = fixture(tmp_path)
    assert "accepted=1" in call(codec,tmp_path,path,"native-context",tmp_path / "unused.bin")


@pytest.mark.parametrize("name", ["cupsemicommitted", "cupfinaltable", "worldcommitted", "worldfinaltable"])
def test_actual_cartridge_result_and_completion_password_goldens(codec,tmp_path,name):
    # These semantic fields were captured after original game-result/ceremony
    # code ran. No executable ROM bytes or full graphics-containing WRAM are
    # fixtures. Selected regions reproduce the complete original export.
    captured = json.loads((ROOT / "tests/fixtures/password/campaign_states.json").read_text())[name]
    ram = bytearray(0x20000)
    for address, hex_bytes in captured["campaign_regions"].items():
        start = int(address,16); data = bytes.fromhex(hex_bytes)
        ram[start:start+len(data)] = data
    source = tmp_path / "captured.wram"; source.write_bytes(ram)
    original = tmp_path / "original.bin"; native = tmp_path / "native.bin"
    call(codec,tmp_path,source,"encode",original)
    expected = bytes.fromhex(captured["symbols"])
    assert original.read_bytes() == expected
    assert "accepted=1" in call(codec,tmp_path,source,"native-encode",native)
    assert native.read_bytes() == expected
    assert "accepted=1" in call(codec,tmp_path,source,"decode",native,len(expected))
    original_ram = (tmp_path / "decoded.wram").read_bytes()
    assert "accepted=1" in call(codec,tmp_path,source,"native-decode",native,len(expected))
    assert (tmp_path / "decoded.wram").read_bytes() == original_ram
    assert source.read_bytes() == ram


@pytest.mark.parametrize("flags", [0x1005,0x405,0x7,0x1021,0x421,0x23])
def test_other_mode_bits_do_not_masquerade_as_campaign(codec,tmp_path,flags):
    path, ram, _ = fixture(tmp_path)
    struct.pack_into("<H",ram,0x1648,flags); path.write_bytes(ram)
    assert "accepted=0" in call(codec,tmp_path,path,"native-encode",tmp_path / "invalid.bin")


@pytest.mark.parametrize("flags,length", [(0x205,12),(0x5,15),(0xd,39),(0x21,50),(0xe1,11),(0x161,8)])
@pytest.mark.parametrize("team,progress,seed", [(0,0,0),(60,1,55),(62,3,173)])
def test_original_variants_multiple_teams_and_progress(codec, tmp_path, flags, length, team, progress, seed):
    # Addresses/bit widths come from original DATA_81B2E4..81B449;
    # encode/decode are compared with original instructions, not our helpers.
    ram = bytearray(0x20000)
    struct.pack_into("<H",ram,0x1648,flags)
    ram[0x78] = seed; ram[0xda0] = team; ram[0x1652] = progress
    ram[0x1f9c] = 2; ram[0xdc00:0xdc04] = bytes([team,60,62,64])
    ram[0xddb0:0xddce] = bytes([team] * 30)
    for address in range(0xdc26,0xdc6e,2): ram[address] = progress
    path = tmp_path / "variant.wram"; path.write_bytes(ram)
    original = tmp_path / "original.bin"; native = tmp_path / "native.bin"
    call(codec,tmp_path,path,"encode",original)
    assert len(original.read_bytes()) == length
    assert "accepted=1" in call(codec,tmp_path,path,"native-encode",native)
    assert native.read_bytes() == original.read_bytes()
    assert "accepted=1" in call(codec,tmp_path,path,"decode",native,length)
    original_ram = (tmp_path / "decoded.wram").read_bytes()
    assert "accepted=1" in call(codec,tmp_path,path,"native-decode",original,length)
    assert (tmp_path / "decoded.wram").read_bytes() == original_ram
