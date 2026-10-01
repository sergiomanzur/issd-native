"""Per-player profile behavior through real SDL virtual controllers."""
import os
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_control_profiles(tmp_path):
    if os.name == "nt":
        values = dict(line.split("=", 1) for line in
                      (ROOT / "build/CMakeCache.txt").read_text().splitlines()
                      if "=" in line and not line.startswith(("#", "//")))
        library = values["SDL2_LIBRARY:FILEPATH"]
        flags = ["-I" + values["SDL2_INCLUDE_DIR:PATH"], library]
    else:
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
    exe = tmp_path / "profiles.exe"
    result = subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-D_CRT_SECURE_NO_WARNINGS",
                             "-I" + str(ROOT / "ISSDNative"),
                             str(ROOT / "tests/test_control_profiles.c"),
                             str(ROOT / "ISSDNative/issd_config.c"),
                             str(ROOT / "ISSDNative/issd_input.c"), *flags,
                             "-o", str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    env = dict(os.environ)
    if os.name == "nt":
        env["PATH"] = str(Path(library).parent) + os.pathsep + env["PATH"]
    result = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
    assert result.returncode == 0, result.stdout + result.stderr
