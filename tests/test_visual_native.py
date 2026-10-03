"""Presentation-only controls on an advancing real-ROM match."""
import math
from pathlib import Path
from PIL import Image
import pytest
from test_running_animation_native import ROOT, ROM, EXE, run

pytestmark=pytest.mark.skipif(not EXE.exists() or not ROM.exists(),reason="needs built exe and local ROM")

def test_color_boost_and_retired_outline_preserve_game_state(tmp_path):
    seed=run(tmp_path/"seed",600,extra=("--auto-start","60","--save-state","600"))
    snapshot=seed/"saves/quicksave.sav"
    outputs=[]
    for label,settings in (("original",""),("legacy-outline","ball_outline=1\n"),("boost","color_boost=1\n")):
        folder=tmp_path/label
        # run writes config first; pass overrides through a separate configured
        # launch using the same isolated paths and supported snapshot context.
        folder.mkdir()
        import shutil, subprocess, os
        saves=folder/"saves";saves.mkdir();shutil.copyfile(snapshot,saves/"quicksave.sav")
        cfg=folder/"isolated.cfg";cfg.write_text("internal_res=0\n"+settings,encoding="ascii")
        result=subprocess.run([str(EXE),"--rom",str(ROM),"--headless","40","--config",str(cfg),
            "--save-dir",str(saves),"--mods-dir",str(folder/"empty-mods"),"--load-state","0",
            "--dump-state","final","--screenshot","frame.bmp"],cwd=folder,capture_output=True,text=True,
            timeout=120,env=dict(os.environ,SDL_VIDEODRIVER="dummy",SDL_AUDIODRIVER="dummy"))
        assert result.returncode==0,result.stdout+result.stderr
        outputs.append(folder)
    assert len({(f/"final.wram").read_bytes() for f in outputs})==1
    original=Image.open(outputs[0]/"frame.bmp").convert("RGB")
    legacy=Image.open(outputs[1]/"frame.bmp").convert("RGB")
    boost=Image.open(outputs[2]/"frame.bmp").convert("RGB")
    assert original.tobytes()==legacy.tobytes()
    assert original.tobytes()!=boost.tobytes()
    expected=[]
    for r,g,b in original.getdata():
        gray=(77*r+150*g+29*b+128)>>8
        expected.append(tuple(max(0,min(255,c+math.trunc((c-gray)/8))) for c in (r,g,b)))
    # The bottom notification is a host overlay composited after Color Boost;
    # its translucent pixels intentionally do not undergo another color pass.
    game_pixels=original.width*(original.height-32)
    assert list(boost.getdata())[:game_pixels]==expected[:game_pixels]
