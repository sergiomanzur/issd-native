"""Fresh normal-input native profile construction; never changes guest memory."""
from pathlib import Path
import os
import subprocess
import pytest
from tools.mod_studio.game_launch import LaunchRequest, stage
from tools.mod_studio.stadium_geometry import read_template, compile_geometry, derived_fields


@pytest.mark.parametrize('length,logical_id',[(1728,8),(1664,9)])
def test_independent_pitch_uses_native_constructor_and_compiled_maps(tmp_path,length,logical_id):
    root = Path(__file__).resolve().parents[1]
    executable = root / 'build/ISSDNative.exe'
    rom = root / 'International Superstar Soccer Deluxe (USA).sfc'
    if not executable.exists() or not rom.exists():
        pytest.skip('Requires built native executable and private cartridge')
    profile = {'version': 1, 'base_layout': 0,
               'geometry': {'length_units': length, 'width_units': 576}}
    pack = {'name': 'Geometry proof', 'stadium_count': logical_id+1, 'teams': [],
            'stadiums': [{'stadium_id': logical_id, 'name': 'CUSTOM', 'stadium_profile': profile}]}
    run = stage(LaunchRequest(executable,rom,logical_id,pack,tmp_path,tmp_path/'sessions'))
    environment = dict(run.environment, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    output = run.directory/'state'
    result = subprocess.run([str(executable),'--rom',str(rom),'--config',str(run.config),
        '--mods-dir',str(run.mods),'--save-dir',str(run.saves),'--script',str(run.script),
        '--headless','7000','--dump-state',str(output)],cwd=run.directory,env=environment,
        capture_output=True,text=True,timeout=300)
    assert result.returncode == 0, result.stdout[-3000:]+result.stderr[-3000:]
    ram = output.with_suffix('.wram').read_bytes()
    word = lambda address: int.from_bytes(ram[address:address+2],'little')
    assert word(0x70) in (8,0x13), f'Native match not reached: {word(0x70):x}'
    assert word(0x1fa2) == logical_id and word(0x86) == 0
    assert word(0x12a2) == length, 'Profile not used by original pitch constructor'
    for address, expected in derived_fields(length,576).items():
        assert word(address) == expected, f'Derived native field {address:04x}'
    compiled = compile_geometry(profile,read_template(rom.read_bytes(),0))
    assert not compiled.diagnostics
    for layer in range(2):
        assert ram[0x18000+layer*0x2000:0x1a000+layer*0x2000] == compiled.metatiles[layer]
        assert ram[0x1d000+layer*0x1000:0x1e000+layer*0x1000] == compiled.world_maps[layer]
