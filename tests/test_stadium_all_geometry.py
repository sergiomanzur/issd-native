from pathlib import Path
import subprocess
import pytest
from tools.mod_studio.stadium_geometry import read_template,compile_geometry
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_every_geometry_candidate_matches_native_success_or_budget_rejection(tmp_path):
    root=Path(__file__).resolve().parents[1]
    rom=root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom.exists():pytest.skip('Requires private cartridge')
    native=root/'ISSDNative'
    decoder=tmp_path/'decoder.c'
    decoder.write_text('#include "issd_decompress.h"\n#include <string.h>\n#define WINDOW_SIZE 0x400\n#define WINDOW_MASK 0x3ff\n'+
                      function((native/'issd_decompress.c').read_text(),'ISSD_Decompress('))
    executable=tmp_path/'all-geometry.exe'
    compile_c(executable,root,[root/'tests/test_stadium_all_geometry.c',decoder,
        native/'issd_stadium_geometry.c',native/'issd_stadium_template.c'],[native])
    result=subprocess.run([str(executable),str(rom),str(tmp_path)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    cartridge=rom.read_bytes()
    for base in range(8):
        template=read_template(cartridge,base)
        for length in range(1536,template['length_units']+1,32):
            profile={'version':1,'base_layout':base,'geometry':{
                'length_units':length,'width_units':template['width_units']}}
            compiled=compile_geometry(profile,template)
            if compiled.diagnostics:
                assert (tmp_path/f'{base}-{length}.rejected').exists(),(base,length,compiled.diagnostics)
                assert not (tmp_path/f'{base}-{length}.bin').exists()
                continue
            assert not (tmp_path/f'{base}-{length}.rejected').exists(),(base,length)
            assert (tmp_path/f'{base}-{length}.bin').read_bytes()==b''.join(
                compiled.metatiles[layer]+compiled.world_maps[layer] for layer in range(2)),(base,length)
