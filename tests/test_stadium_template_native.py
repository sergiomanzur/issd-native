from pathlib import Path
import subprocess
import pytest
from test_config_persistence import compile_c
from test_snapshot_transactional import function
from tools.mod_studio.stadium_geometry import read_template,read_preview_resources


def test_native_template_loader_matches_private_original_descriptors(tmp_path):
    root = Path(__file__).resolve().parents[1]
    source = root / 'ISSDNative/issd_stadium_template.c'
    assert source.exists(), 'Native template reader missing'
    rom = root / 'International Superstar Soccer Deluxe (USA).sfc'
    if not rom.exists():
        pytest.skip('Private cartridge required for native descriptor parity')
    decompressor = tmp_path / 'decompress.c'
    decompressor.write_text('#include "issd_decompress.h"\n#include <string.h>\n'
        '#define WINDOW_SIZE 0x400\n#define WINDOW_MASK 0x3ff\n' +
        function((root / 'ISSDNative/issd_decompress.c').read_text(), 'ISSD_Decompress('))
    executable = tmp_path / 'template.exe'
    compile_c(executable, root, [root / 'tests/test_stadium_template_native.c',
              source, decompressor], [root / 'ISSDNative'])
    for base in range(8):
        output = tmp_path / f'{base}.bin'
        result = subprocess.run([str(executable), str(rom), str(output), str(base)],
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        expected = read_template(rom.read_bytes(), base)
        assert output.read_bytes() == b''.join(expected['metatiles'][i]+expected['world_maps'][i]
                                               for i in range(2))
        assert Path(str(output)+'.pixels').read_bytes()==b''.join(read_preview_resources(rom.read_bytes(),base))
