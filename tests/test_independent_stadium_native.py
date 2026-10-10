"""Published original artwork selected through ordinary original menus."""
from pathlib import Path
import json,subprocess
import pytest
from tools.mod_studio.game_launch import LaunchRequest,stage
from tools.mod_studio.stadium_assets import compile_manifest

@pytest.mark.parametrize('logical,length',[(8,1728),(31,1664)])
def test_distributable_original_art_has_independent_native_geometry(tmp_path,logical,length):
    root=Path(__file__).resolve().parents[1]
    exe,rom=root/'build/ISSDNative.exe',root/'International Superstar Soccer Deluxe (USA).sfc'
    if not exe.exists() or not rom.exists():pytest.skip('Requires native build and private cartridge')
    source=root/'mods/independent_stadium_example'
    pack=json.loads((source/'mod.json').read_text())
    entry=next(s for s in pack['stadiums'] if s['stadium_id']==logical)
    manifest=source/entry['stadium_profile']['artwork']
    art=compile_manifest(json.loads(manifest.read_text()),manifest.parent)
    assert not art.diagnostics
    run=stage(LaunchRequest(exe,rom,logical,pack,source,tmp_path/'sessions'))
    result=subprocess.run([str(exe),'--rom',str(rom),'--config',str(run.config),
        '--mods-dir',str(run.mods),'--save-dir',str(run.saves),'--script',str(run.script),
        '--headless','7000','--dump-state',str(run.directory/'state'),
        '--screenshot',str(run.directory/'frame.bmp')],cwd=run.directory,
        env=dict(run.environment,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),
        capture_output=True,text=True,timeout=300)
    (run.directory/'acceptance.log').write_text(result.stdout+result.stderr)
    assert result.returncode==0,result.stdout[-2000:]+result.stderr[-2000:]
    ram=(run.directory/'state.wram').read_bytes()
    word=lambda address:int.from_bytes(ram[address:address+2],'little')
    assert word(0x70) in (8,19)
    assert (word(0x1fa2),word(0x86),word(0x12a2),word(0x12a4),word(0x12ea))==(logical,0,length,576,length+56)
    vram=(run.directory/'state.ppu').read_bytes()[-65536:]
    assert vram[0x8000:0x8000+len(art.tile_writes[0].data)]==art.tile_writes[0].data
    assert ram[0x2c40:0x2ca0]==art.palette
    for layer in (0,1):assert ram[0x1d000+layer*4096:0x1e000+layer*4096]==art.geometry.world_maps[layer]
