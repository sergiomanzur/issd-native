"""Normal-input custom goal playback, load, native art and real localHD render."""
from pathlib import Path
import subprocess,struct
import pytest
from PIL import Image,ImageChops
from tools.mod_studio.stadium_assets import create_artwork,import_image
from tools.mod_studio.game_launch import LaunchRequest,stage


@pytest.mark.parametrize('logical,length,goal_frame',[(8,1728,7424),(31,1664,6744)])
def test_custom_natural_replay_reload_keeps_geometry_and_local_art_across_widths(tmp_path,logical,length,goal_frame):
    root = Path(__file__).resolve().parents[1]
    exe,rom = root/'build/ISSDNative.exe',root/'International Superstar Soccer Deluxe (USA).sfc'
    if not exe.exists() or not rom.exists():
        pytest.skip('Requires native build and private cartridge')
    profile = {'version':1,'base_layout':0,'geometry':{'length_units':length,'width_units':576}}
    assert not create_artwork(profile,rom.read_bytes(),tmp_path/'initial').diagnostics
    image = Image.new('RGBA',(16,16))
    for y in range(16):
        for x in range(16):
            image.putpixel((x,y),((255,0,0,255) if logical==8 else (0,255,0,255)) if (x+y)%2 else (0,0,255,255))
    image.save(tmp_path/'image.png')
    imported = import_image(tmp_path/'image.png',tmp_path/'initial/stadium.json',tmp_path/'art',0,hd_scale=2)
    assert not [d for d in imported.diagnostics if d.severity=='error']
    # A deliberately simple authored background ensures the HD character is
    # visible in this real camera. This edits pack resources, never guest RAM.
    for layer in imported.manifest['layers']:
        (tmp_path/'art'/layer['metatiles']).write_bytes(struct.pack('<H',0x0a00)*4096)
        (tmp_path/'art'/layer['world_map']).write_bytes(bytes(4096))
    profile['artwork']='art/stadium.json'
    pack = {'name':'Replay proof','stadium_count':32,'teams':[],
        'stadiums':[{'stadium_id':logical,'name':'CUSTOM','stadium_profile':profile}]}
    run = stage(LaunchRequest(exe,rom,logical,pack,tmp_path,tmp_path/'sessions'))
    if logical==8:
        entries={int(line.split()[0]):line.split(maxsplit=1)[1] for line in run.script.read_text().splitlines()}
        for frame in range(6000,goal_frame,180):
            entries.update({frame:'B',frame+10:'NONE'})
        run.script.write_text(''.join(f'{f} {b}\n' for f,b in sorted(entries.items())))
    base_config = run.config.read_text().replace('engine_mode = 0','engine_mode = 1')
    run.config.write_text(base_config)
    environment = dict(run.environment,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    args = [str(exe),'--rom',str(rom),'--config',str(run.config),'--mods-dir',str(run.mods),'--save-dir',str(run.saves)]
    fresh = subprocess.run(args+['--script',str(run.script),'--headless',str(goal_frame),'--save-state',str(goal_frame),
        '--dump-state',str(run.directory/'goal')],cwd=run.directory,env=environment,capture_output=True,text=True,timeout=300)
    assert fresh.returncode == 0,fresh.stdout[-3000:]+fresh.stderr[-3000:]
    ram = (run.directory/'goal.wram').read_bytes()
    word = lambda data,address:int.from_bytes(data[address:address+2],'little')
    assert (word(ram,0x70),word(ram,0x72),word(ram,0xea2))==(19,6,1),'No naturally scored goal replay'
    seed = (run.saves/'quicksave.sav').read_bytes()
    native_reference = None
    for scale in (1,2):
        baseline_ram,baseline_image = None,None
        for aspect,wide,width in ((1,0,256),(0,1,256),(3,1,358),(2,1,398),(4,1,504)):
            folder = tmp_path/f'reload-{scale}-{aspect}-{wide}'
            folder.mkdir();saves=folder/'saves';saves.mkdir()
            (saves/'quicksave.sav').write_bytes(seed)
            config=folder/'config.cfg'
            config.write_text(base_config+f'\naspect_ratio={aspect}\ntrue_widescreen={wide}\nball_outline=0\nball_shadow=0\nplayer_markers=0\nplayer_names=0\nradar_position=0\nradar_scale=1\nradar_opacity=100\nhud_scale=1\n')
            script=folder/'input.txt';script.write_text('0 NONE\n20 X\n30 NONE\n')
            command=[str(exe),'--rom',str(rom),'--config',str(config),'--mods-dir',str(run.mods),
                     '--save-dir',str(saves),'--script',str(script),'--load-state','1','--headless','400',
                     '--dump-state',str(folder/'state'),'--screenshot',str(folder/'final.bmp'),'--capture-scale',str(scale)]
            result=subprocess.run(command,cwd=folder,env=environment,capture_output=True,text=True,timeout=120)
            (folder/'native.log').write_text(result.stdout+result.stderr)
            assert result.returncode == 0,result.stdout[-3000:]+result.stderr[-3000:]
            actual=(folder/'state.wram').read_bytes()
            assert (word(actual,0x1fa2),word(actual,0x86),word(actual,0x12a2),word(actual,0x18b4))==(logical,0,length,1)
            assert (saves/'quicksave.sav').read_bytes()==seed
            with Image.open(folder/'final.bmp') as opened:picture=opened.convert('RGB')
            assert picture.size==(width*scale,224*scale)
            if baseline_ram is None:
                baseline_ram,baseline_image=actual,picture
                if scale==1:native_reference=picture
                else:
                    assert ImageChops.difference(native_reference.resize(picture.size,Image.Resampling.NEAREST),picture).getbbox(), 'LocalHD did not change real rendered output'
            else:
                assert actual==baseline_ram,'Presentation changed accepted guest state'
                margin=(width-256)//2*scale
                center=picture.crop((margin,0,margin+256*scale,224*scale))
                assert ImageChops.difference(baseline_image,center).getbbox() is None,'Native replay center changed with width'
