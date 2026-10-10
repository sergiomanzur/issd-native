from pathlib import Path
import subprocess
import os
import pytest
from PIL import Image
from tools.mod_studio.stadium_assets import create_artwork,import_image
from tools.mod_studio.game_launch import LaunchRequest,stage


def test_native_owned_art_is_uploaded_and_palette_enters_original_mirror(tmp_path):
    root = Path(__file__).resolve().parents[1]
    executable,rom = root/'build/ISSDNative.exe',root/'International Superstar Soccer Deluxe (USA).sfc'
    if not executable.exists() or not rom.exists():
        pytest.skip('Requires built game and private cartridge')
    profile = {'version':1,'base_layout':0,'geometry':{'length_units':1728,'width_units':576}}
    assert not create_artwork(profile,rom.read_bytes(),tmp_path/'initial').diagnostics
    image = tmp_path/'red.png'
    Image.new('RGBA',(8,8),'red').save(image)
    imported = import_image(image,tmp_path/'initial/stadium.json',tmp_path/'authored',0)
    assert not [d for d in imported.diagnostics if d.severity == 'error']
    profile['artwork'] = 'authored/stadium.json'
    pack = {'name':'Art proof','stadium_count':9,'teams':[],
        'stadiums':[{'stadium_id':8,'name':'CUSTOM','stadium_profile':profile}]}
    run = stage(LaunchRequest(executable,rom,8,pack,tmp_path,tmp_path/'sessions'))
    output = run.directory/'state'
    result = subprocess.run([str(executable),'--rom',str(rom),'--config',str(run.config),
        '--mods-dir',str(run.mods),'--save-dir',str(run.saves),'--script',str(run.script),
        '--headless','7000','--dump-state',str(output)],cwd=run.directory,
        env=dict(run.environment,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),
        capture_output=True,text=True,timeout=300)
    assert result.returncode == 0,result.stdout[-2000:]+result.stderr[-2000:]
    ram = output.with_suffix('.wram').read_bytes()
    assert int.from_bytes(ram[0x12a2:0x12a4],'little') == 1728
    vram = output.with_suffix('.ppu').read_bytes()[-65536:]
    assert vram[0x8000:0x8020] == imported.compiled.tile_writes[0].data,'Authored pixels were not uploaded'
    assert ram[0x2c40:0x2ca0] == imported.compiled.palette,'Original native palette mirror does not contain authored colors'


def test_loaded_match_refreshes_changed_and_deleted_art_without_geometry_changes(tmp_path):
    import json
    from tools.mod_studio.stadium_geometry import read_preview_resources
    root=Path(__file__).resolve().parents[1]
    executable,rom=root/'build/ISSDNative.exe',root/'International Superstar Soccer Deluxe (USA).sfc'
    if not executable.exists() or not rom.exists():
        pytest.skip('Requires built game and private cartridge')
    profile={'version':1,'base_layout':0,'geometry':{'length_units':1728,'width_units':576}}
    assert not create_artwork(profile,rom.read_bytes(),tmp_path/'initial').diagnostics
    Image.new('RGBA',(8,8),'red').save(tmp_path/'red.png')
    imported=import_image(tmp_path/'red.png',tmp_path/'initial/stadium.json',tmp_path/'red',0)
    profile['artwork']='red/stadium.json'
    pack={'name':'Cosmetic reload','stadium_count':9,'teams':[],
          'stadiums':[{'stadium_id':8,'name':'CUSTOM','stadium_profile':profile}]}
    first=stage(LaunchRequest(executable,rom,8,pack,tmp_path,tmp_path/'sessions'))
    def execute(run,frames,extra):
        result=subprocess.run([str(executable),'--rom',str(rom),'--config',str(run.config),
            '--mods-dir',str(run.mods),'--save-dir',str(run.saves),*extra,'--headless',str(frames),
            '--dump-state',str(run.directory/'state')],cwd=run.directory,
            env=dict(run.environment,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),
            capture_output=True,text=True,timeout=300)
        (run.directory/'acceptance.log').write_text(result.stdout+result.stderr)
        assert result.returncode==0,result.stdout[-2000:]+result.stderr[-2000:]
        return (run.directory/'state.wram').read_bytes(),(run.directory/'state.ppu').read_bytes()[-65536:]
    execute(first,7000,['--script',str(first.script),'--save-state','7000'])
    seed=(first.saves/'quicksave.sav').read_bytes()
    native,palette=read_preview_resources(rom.read_bytes(),0)
    Image.new('RGBA',(8,8),'blue').save(tmp_path/'blue.png')
    blue=import_image(tmp_path/'blue.png',tmp_path/'initial/stadium.json',tmp_path/'blue',0)
    variants=[('same','red/stadium.json',imported.compiled.tile_writes[0].data,imported.compiled.palette),
              ('blue','blue/stadium.json',blue.compiled.tile_writes[0].data,blue.compiled.palette)]
    deleted=json.loads((tmp_path/'red/stadium.json').read_text())
    deleted['tiles']=[];deleted.pop('palette',None)
    (tmp_path/'red/deleted.json').write_text(json.dumps(deleted))
    variants.extend([('partial','red/deleted.json',native[0x8000:0x8020],palette[0x40:0xa0]),
                     ('none',None,native[0x8000:0x8020],palette[0x40:0xa0])])
    physics=None
    for name,art,tiles,colors in variants:
        if art:profile['artwork']=art
        else:profile.pop('artwork',None)
        run=stage(LaunchRequest(executable,rom,8,pack,tmp_path,tmp_path/'sessions'))
        (run.saves/'quicksave.sav').write_bytes(seed)
        ram,vram=execute(run,200,['--load-state','1'])
        assert vram[0x8000:0x8020]==tiles,f'{name}: saved stale tile upload'
        assert ram[0x2c40:0x2ca0]==colors,f'{name}: saved stale palette'
        state=tuple(ram[a:a+2] for a in (0x70,0x72,0xa8,0x12a2,0x12a4,0x16d0,0x16d2,0xda2,0xea2,0x1fa2))
        if physics is None:physics=state
        else:assert state==physics,f'{name}: cosmetic refresh changed match progression'
        assert (run.saves/'quicksave.sav').read_bytes()==seed
