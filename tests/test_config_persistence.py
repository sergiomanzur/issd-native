import subprocess
from pathlib import Path
import shutil


def c_compiler():
    for name in ("cc", "clang", "gcc", "cl"):
        path = shutil.which(name)
        if path:
            return name
    raise AssertionError("No C compiler found on PATH")


def compile_c(exe, repo, sources, includes):
    compiler = c_compiler()
    if compiler == "cl":
        cmd = [compiler, "/nologo"]
        for include in includes:
            cmd.extend(["/I", str(include)])
        cmd.extend(str(source) for source in sources)
        cmd.append("/Fe:" + str(exe))
    else:
        cmd = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror"]
        for include in includes:
            cmd.extend(["-I", str(include)])
        cmd.extend(str(source) for source in sources)
        cmd.extend(["-o", str(exe)])
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=repo)
    assert result.returncode == 0, result.stdout + result.stderr


def test_cfg_persistence_round_trip(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_config_persistence.exe"
    config = tmp_path / "issd_native.cfg"
    compile_c(
        exe,
        repo,
        [repo / "tests/test_config_persistence.c", repo / "ISSDNative/issd_config.c"],
        [repo / "ISSDNative"],
    )
    run_result = subprocess.run([str(exe), str(config)], capture_output=True, text=True)
    assert run_result.returncode == 0, run_result.stdout + run_result.stderr
