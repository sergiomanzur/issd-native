"""Original inputs, modded moving banners and competition flag copies."""
import json
import os
from pathlib import Path
import subprocess
from PIL import Image, ImageChops
import pytest
from test_coin_widescreen_acceptance import ROOT, ROM, EXE, original_exhibition_input

pytestmark=pytest.mark.skipif(not EXE.exists() or not ROM.exists(),reason="needs native build and local ROM")

@pytest.mark.parametrize("mode,frames,pixels",[("exhibition",1940,768),("exhibition",1960,768),("exhibition",2000,128),("exhibition",2010,0),("cup",1000,3072),("world",1000,2304)])
def test_mod_identities_follow_native_sprites(tmp_path,mode,frames,pixels):
    mods=tmp_path/"mods";mods.mkdir()
    color=(13,237,113)
    Image.new("RGBA",(24,16),(*color,255)).save(mods/"flag.bmp")
    (mods/"fixture.json").write_text(json.dumps({"name":"Identity Fixture","teams":[
        {"team_id":i,"plate_name":f"TEAM{i:02}","flag":"flag.bmp"} for i in range(36)]}),encoding="utf-8")
    script=original_exhibition_input()
    entries={int(line.split()[0]):line.split()[1] for line in script.splitlines()}
    if mode!="exhibition":entries.update({500:"DOWN",510:"NONE"})
    if mode=="world":entries.update({530:"DOWN",540:"NONE"})
    script="".join(f"{f} {button}\n" for f,button in sorted(entries.items()))
    baseline_ram=baseline_picture=None
    for label,enabled,aspect,width in [("stock",False,0,256),("mod",True,0,256),("mod-wide",True,4,504)]:
        folder=tmp_path/label;folder.mkdir()
        cfg=folder/"isolated.cfg"
        cfg.write_text(f"engine_mode=0\ninternal_res=0\ntrue_widescreen=1\naspect_ratio={aspect}\n"+
            ("active_mod_packs=Identity Fixture\n" if enabled else ""),encoding="ascii")
        inputs=folder/"input.txt";inputs.write_text(script,encoding="ascii")
        result=subprocess.run([str(EXE),"--rom",str(ROM),"--headless",str(frames),"--config",str(cfg),
            "--save-dir",str(folder/"saves"),"--mods-dir",str(mods),"--script",str(inputs),
            "--dump-state","state","--screenshot","frame.bmp"],cwd=folder,capture_output=True,text=True,
            timeout=120,env=dict(os.environ,SDL_AUDIODRIVER="dummy",SDL_VIDEODRIVER="dummy"))
        (folder/"run.log").write_text(result.stdout+result.stderr,encoding="utf-8")
        assert result.returncode==0,result.stdout+result.stderr
        assert "[interp_cap]" not in result.stderr
        ram=(folder/"state.wram").read_bytes()
        picture=Image.open(folder/"frame.bmp").convert("RGB");picture.save(folder/"frame.png")
        assert picture.size==(width,224)
        if not enabled:baseline_ram=ram;baseline_picture=picture.copy();continue
        assert ram==baseline_ram,"identity presentation changed the game"
        assert sum(pixel==color for pixel in picture.getdata())==pixels
        margin=(width-256)//2
        center=picture.crop((margin,0,margin+256,224))
        difference=ImageChops.difference(center,baseline_picture).getbbox()
        if pixels:assert difference is not None
        else:assert difference is None,"departed banners left replacement pixels behind"
        if label=="mod":mod_picture=center.copy()
        else:
            assert ImageChops.difference(center,mod_picture).getbbox() is None
            for box in ((0,0,margin,224),(margin+256,0,width,224)):
                assert color not in set(picture.crop(box).getdata()),"custom flag leaked into margin"
