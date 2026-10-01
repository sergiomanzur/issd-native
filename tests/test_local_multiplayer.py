"""Exercise real SDL virtual gamepads and the SNES multitap protocol."""
import os
import shlex
from pathlib import Path
import subprocess
import pytest

from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


def test_four_local_gamepads(tmp_path):
    # Uses the same SDL installation as the desktop build, with no physical pads.
    if os.name == "nt":
        cache = (ROOT / "build/CMakeCache.txt").read_text()
        values = dict(line.split("=", 1) for line in cache.splitlines()
                      if "=" in line and not line.startswith(("#", "//")))
        include = values["SDL2_INCLUDE_DIR:PATH"]
        library = values["SDL2_LIBRARY:FILEPATH"]
        flags = ["-I" + include, library]
    else:
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
    exe = tmp_path / "input.exe"
    command = ["clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I" + str(ROOT / "ISSDNative"),
               str(ROOT / "tests/test_local_input.c"),
               str(ROOT / "ISSDNative/issd_input.c"), *flags, "-o", str(exe)]
    built = subprocess.run(command, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    env = dict(os.environ)
    if os.name == "nt":
        env["PATH"] = str(Path(library).parent) + os.pathsep + env["PATH"]
    result = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
    assert result.returncode == 0, result.stdout + result.stderr


def test_multitap_protocol(tmp_path):
    exe = tmp_path / "multitap.exe"
    compile_c(exe, ROOT, [ROOT / "tests/test_multitap.c", RUNTIME / "snes/joypad.c"],
              [RUNTIME, RUNTIME / "snes"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_independent_scripted_players(tmp_path):
    script = tmp_path / "input.txt"
    script.write_text("0 B\n0 P2 A\n0 P3 X\n0 P4 RIGHT\n20 P2 NONE\n")
    exe = tmp_path / "script.exe"
    compile_c(exe, ROOT, [ROOT / "tests/test_multiplayer_script.c",
                          ROOT / "ISSDNative/issd_script.c"], [ROOT / "ISSDNative"])
    result = subprocess.run([str(exe), str(script)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_multitap_snapshot_and_v7_compatibility(tmp_path):
    snes = (RUNTIME / "snes/snes.c").read_text()
    serialization = snes[snes.index("static uint32_t s_saveload_version"):
                         snes.index("void snes_reset(")]
    source = tmp_path / "snapshot.c"
    source.write_text('#include <assert.h>\n#include <stddef.h>\n#include "snes.h"\n' +
                      serialization + (ROOT / "tests/test_multitap_snapshot.c").read_text())
    exe = tmp_path / "snapshot.exe"
    compile_c(exe, ROOT, [source], [RUNTIME, RUNTIME / "snes"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize("source", ["auto_joypad_test.c", "manual_joypad_test.c"])
def test_legacy_two_pad_protocol(tmp_path, source):
    exe = tmp_path / "legacy.exe"
    compile_c(exe, ROOT, [ROOT / "deps/snesrecomp/tests/joypad" / source,
                          RUNTIME / "snes/joypad.c"], [RUNTIME, RUNTIME / "snes"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.skipif(not (ROOT / "build/ISSDNative.exe").exists() or
                    not (ROOT / "International Superstar Soccer Deluxe (USA).sfc").exists(),
                    reason="requires native build and the user's ROM")
def test_original_game_reads_four_players_and_reloads_snapshot(tmp_path):
    exe = ROOT / "build/ISSDNative.exe"
    rom = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
    (tmp_path / "inputs.txt").write_text("0 P1 B\n0 P2 A\n0 P3 X\n0 P4 RIGHT\n")

    def run(extra):
        result = subprocess.run([str(exe), "--headless", "60", "--script", "inputs.txt",
                                 "--save-dir", str(tmp_path / "saves"),
                                 "--config", str(tmp_path / "test.cfg"),
                                 "--dump-state", "state", *extra, str(rom)],
                                cwd=tmp_path, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "[interp_cap]" not in result.stderr
        assert "NMI-busy=0000" in result.stdout
        ram = (tmp_path / "state.wram").read_bytes()
        assert [int.from_bytes(ram[i:i+2], "little") for i in (16, 18, 20, 22)] == [
            0x8000, 0x0080, 0x0040, 0x0100]
        assert [int.from_bytes(ram[i:i+2], "little") for i in (0x90, 0x92, 0x94, 0x96)] == [1]*4

    run(["--save-state", "40"])
    snapshot = tmp_path / "saves/quicksave.sav"
    envelope = snapshot.read_bytes()
    assert envelope[:4] == b"ISCE"
    assert int.from_bytes(envelope[196:200], "little") == 8
    run(["--load-state", "20"])


@pytest.mark.skipif(not (ROOT / "build/ISSDNative.exe").exists() or
                    not (ROOT / "International Superstar Soccer Deluxe (USA).sfc").exists(),
                    reason="requires native build and the user's ROM")
def test_secondary_scripts_preserve_primary_autostart(tmp_path):
    (tmp_path / "inputs.txt").write_text("0 P2 NONE\n0 P3 NONE\n0 P4 NONE\n")
    result = subprocess.run([str(ROOT / "build/ISSDNative.exe"), "--headless", "1200",
                             "--save-dir", str(tmp_path / "saves"),
                             "--config", str(tmp_path / "test.cfg"),
                             "--auto-start", "60", "--script", "inputs.txt",
                             "--dump-state", "state",
                             str(ROOT / "International Superstar Soccer Deluxe (USA).sfc")],
                            cwd=tmp_path, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    ram = (tmp_path / "state.wram").read_bytes()
    assert int.from_bytes(ram[0x70:0x72], "little") == 8, "P1 input did not reach a live match"
    assert "S=01AF PB=80 NMI-busy=0000" in result.stdout
