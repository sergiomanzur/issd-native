import subprocess
from pathlib import Path
from test_config_persistence import compile_c

def test_readability_is_optional_and_presentation_only(tmp_path):
    root = Path(__file__).resolve().parents[1]
    exe = tmp_path / "readability.exe"
    compile_c(exe, root, [root / "tests/test_readability.c", root / "ISSDNative/issd_readability.c"], [root / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
