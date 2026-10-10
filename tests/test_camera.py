import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_camera_expanded_coverage_and_signed_coordinates(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / 'camera.exe'
    compile_c(exe, repo, [repo / 'tests/test_camera.c', repo / 'ISSDNative/issd_camera.c'],
              [repo / 'ISSDNative'])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
