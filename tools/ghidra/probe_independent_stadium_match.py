"""Fresh custom matches with input-only controller, original clock and score.

The diagnostic executable replaces only input generation and bounded shutdown;
its manifest hashes every linked production object. No snapshot continuation.
"""
import argparse,json,os,subprocess,sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.ghidra.probe_uninterrupted_campaign import build_probe
from tools.mod_studio.game_launch import LaunchRequest,stage
from tools.mod_studio.stadium_assets import create_artwork,import_image
from PIL import Image


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--folder',type=Path,required=True)
    parser.add_argument('--frames',type=int,default=160000)
    args=parser.parse_args();folder=args.folder.resolve();folder.mkdir(parents=True,exist_ok=False)
    exe=build_probe(folder,ROOT/'tools/ghidra/independent_stadium_probe_input.c')
    rom=ROOT/'International Superstar Soccer Deluxe (USA).sfc'
    def match(logical,length,color):
        source=folder/str(logical);source.mkdir()
        profile={'version':1,'base_layout':0,'geometry':{'length_units':length,'width_units':576}}
        assert not create_artwork(profile,rom.read_bytes(),source/'initial').diagnostics
        Image.new('RGBA',(8,8),color).save(source/'image.png')
        imported=import_image(source/'image.png',source/'initial/stadium.json',source/'art',0)
        assert not [d for d in imported.diagnostics if d.severity=='error']
        profile['artwork']='art/stadium.json'
        pack={'name':'Natural match proof','stadium_count':32,'teams':[],
              'stadiums':[{'stadium_id':logical,'name':'CUSTOM','stadium_profile':profile}]}
        run=stage(LaunchRequest(exe,rom,logical,pack,source,source/'sessions'))
        run.config.write_text(run.config.read_text().replace('engine_mode = 0','engine_mode = 1'))
        command=[str(exe),'--rom',str(rom),'--config',str(run.config),'--mods-dir',str(run.mods),
                 '--save-dir',str(run.saves),'--script',str(run.script),'--headless',str(args.frames),
                 '--dump-state',str(run.directory/'final')]
        with (run.directory/'native.log').open('w') as log:
            result=subprocess.run(command,cwd=run.directory,env=dict(run.environment,
                SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdout=log,stderr=subprocess.STDOUT,timeout=1200)
        events=[json.loads(line) for line in (run.directory/'match-events.jsonl').read_text().splitlines()]
        live=[e for e in events if e['mode']==8]
        outcome={'id':logical,'length':length,'exit_code':result.returncode,
            'guest_memory_modified':False,'snapshot_continuation':False,
            'fresh_live':any(e['id']==logical and e['length']==length and e['width']==576 for e in live),
            'goal_replay':any(e['mode']==19 and any(e['score']) for e in events),
            'halftime':any(e['mode']==18 and e['period']==0 for e in events),
            'second_half':any(e['mode']==8 and e['period']==1 for e in events),
            'fulltime':any(e['mode']==18 and e['period']==1 for e in events),
            'final':events[-1] if events else None,'directory':str(run.directory)}
        (source/'outcome.json').write_text(json.dumps(outcome,indent=2)+'\n')
        print(json.dumps(outcome),flush=True);return outcome
    with ThreadPoolExecutor(max_workers=2) as pool:
        outcomes=list(pool.map(lambda row:match(*row),[(8,1728,'red'),(31,1664,'blue')]))
    (folder/'outcomes.json').write_text(json.dumps(outcomes,indent=2)+'\n')
    if not all(o['exit_code']==0 and all(o[k] for k in ('fresh_live','goal_replay','halftime','second_half','fulltime')) for o in outcomes):
        raise SystemExit('Bounded natural match evidence is incomplete; inspect exact events')

if __name__=='__main__':main()
