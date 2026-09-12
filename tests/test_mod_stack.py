import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_mods_stack(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_mod_stack.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_mod_stack.c",
            repo / "ISSDNative/issd_menu.c",
            repo / "ISSDNative/issd_config.c",
            repo / "ISSDNative/issd_mod.c",
            repo / "ISSDNative/issd_mod_rom.c",
            repo / "ISSDNative/issd_formation.c",
            repo / "ISSDNative/issd_hd.c",
        ],
        [
            repo / "ISSDNative",
            repo / "deps/snesrecomp/runner/src",
            repo / "deps/snesrecomp/runner/src/snes",
        ],
    )
    result = subprocess.run([str(exe)], capture_output=True, text=True, cwd=repo)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "passed" in result.stdout, result.stdout
