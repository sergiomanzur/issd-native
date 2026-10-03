import subprocess
from pathlib import Path
from test_config_persistence import compile_c

def test_visual_cosmetics_preserve_neutrals_and_crt_legacy(tmp_path):
    root=Path(__file__).resolve().parents[1]
    exe=tmp_path / "visual_effects.exe"
    compile_c(exe,root,[root / "tests/test_visual_effects.c"],[root / "ISSDNative"])
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
