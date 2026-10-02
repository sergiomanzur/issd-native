import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_exhibition_checkpoints_and_rule_presets(tmp_path):
    root = Path(__file__).resolve().parents[1]
    exe = tmp_path / 'match-shortcuts.exe'
    compile_c(exe, root, [root / 'tests/test_match_shortcuts.c',
                         root / 'ISSDNative/issd_match.c',
                         root / 'ISSDNative/issd_config.c'], [root / 'ISSDNative'])
    result = subprocess.run([str(exe), str(tmp_path / 'rules.cfg')],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
