import struct
import subprocess
from pathlib import Path
from test_config_persistence import compile_c
from test_stadium_geometry_compiler import template
from tools.mod_studio.stadium_geometry import compile_geometry


def test_native_geometry_matches_editor_compilation(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    source = repo / 'ISSDNative/issd_stadium_geometry.c'
    assert source.exists(), 'Native geometry compiler missing'
    executable = tmp_path / 'geometry.exe'
    compile_c(executable, repo, [repo / 'tests/test_stadium_geometry_native.c', source],
              [repo / 'ISSDNative'])
    original = template()
    fixture = tmp_path / 'template.bin'
    fixture.write_bytes(b''.join(original['metatiles'][i]+original['world_maps'][i]
                                for i in range(2)))
    for length in (1792, 1728, 1664):
        output = tmp_path / f'{length}.bin'
        result = subprocess.run([str(executable), str(fixture), str(output), str(length)],
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        expected = compile_geometry({'version': 1, 'base_layout': 0,
            'geometry': {'length_units': length, 'width_units': 576}}, original)
        assert not expected.diagnostics
        assert output.read_bytes() == b''.join(
            expected.metatiles[i]+expected.world_maps[i] for i in range(2))
