import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_tactical_actors_keep_signed_coordinates_and_transaction(tmp_path):
    root = Path(__file__).resolve().parents[1]
    exe = tmp_path / 'camera_actors.exe'
    compile_c(exe, root, [root / p for p in ['tests/test_camera_actors.c',
        'ISSDNative/issd_camera.c', 'ISSDNative/issd_camera_render.c',
        'ISSDNative/issd_widescreen.c', 'ISSDNative/issd_pose_history.c',
        'ISSDNative/issd_animation.c']], [root / p for p in ['ISSDNative',
        'deps/snesrecomp/runner/src', 'deps/snesrecomp/runner/src/snes']])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
