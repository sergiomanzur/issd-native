import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_scene_view_patches_only_selected_geometry_and_preserves_canonical(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    source = repo / 'ISSDNative/issd_stadium_rom.c'
    assert source.exists(), 'Selected scene ROM builder missing'
    executable = tmp_path / 'scene.exe'
    compile_c(executable, repo, [repo / 'tests/test_stadium_scene_rom.c', source],
              [repo / 'ISSDNative'])
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
