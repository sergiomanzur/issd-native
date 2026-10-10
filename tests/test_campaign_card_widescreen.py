from pathlib import Path
import subprocess
from test_config_persistence import compile_c
import pytest
from PIL import ImageChops
from test_stats_widescreen_acceptance import run, EXE, ROM


def test_campaign_card_clamps_text_and_reflection(tmp_path):
    root=Path(__file__).resolve().parents[1]
    exe=tmp_path/'card.exe'
    compile_c(exe,root,[root/p for p in (
        'tests/test_campaign_card_widescreen.c','ISSDNative/issd_widescreen.c',
        'ISSDNative/issd_animation.c','ISSDNative/issd_pose_history.c')],
        [root/'ISSDNative',root/'deps/snesrecomp/runner/src'])
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(),reason='needs native build and owned ROM')
@pytest.mark.parametrize('world',[False,True],ids=['cup','world-series'])
def test_retail_campaign_card_preserves_text_and_black_sides(tmp_path,world):
    baseline=None
    for aspect,width in ((0,256),(3,358),(2,398),(4,504)):
        folder=tmp_path/str(aspect);folder.mkdir()
        script=''.join(f'{f} START\n{f+10} NONE\n' for f in range(60,361,60))
        script+='500 DOWN\n510 NONE\n'
        if world:script+='530 DOWN\n540 NONE\n'
        script+='560 A\n570 NONE\n'
        path=folder/'input.txt';path.write_text(script)
        settings=f'true_widescreen={int(aspect!=0)}\naspect_ratio={aspect}\n'
        ram,picture=run(folder,720,settings,('--script',str(path)))
        assert int.from_bytes(ram[0x70:0x72],'little')==0x0c
        assert picture.size==(width,224)
        if baseline is None:baseline=(ram,picture);continue
        assert ram==baseline[0]
        margin=(width-256)//2
        assert ImageChops.difference(picture.crop((margin,0,margin+256,224)),baseline[1]).getbbox() is None
        for box in ((0,0,margin,224),(margin+256,0,width,224)):
            assert picture.crop(box).getextrema()==((0,0),(0,0),(0,0))
