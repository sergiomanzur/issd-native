import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_widescreen_replay_excludes_unrecorded_players(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_widescreen_goal_replay.exe"
    compile_c(exe, repo,
              [repo / "tests/test_widescreen_goal_replay.c",
               repo / "ISSDNative/issd_widescreen.c",
               repo / "ISSDNative/issd_pose_history.c",
               repo / "ISSDNative/issd_animation.c"],
              [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src",
               repo / "deps/snesrecomp/runner/src/snes"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
