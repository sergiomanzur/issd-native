from pathlib import Path
import subprocess
from test_config_persistence import compile_c

ROOT=Path(__file__).resolve().parents[1]

def test_launcher_validation_and_navigation(tmp_path):
    source=tmp_path/'launcher_model.c'
    source.write_text('''#include "issd_launcher_model.h"
#include <assert.h>
int main(int n,char**v){(void)n;char why[160];IssdConfig c;issd_config_init_defaults(&c);
assert(issd_launcher_rom_ok(v[1],why,sizeof why));
assert(!issd_launcher_rom_ok(v[2],why,sizeof why));
assert(!issd_launcher_rom_ok("",why,sizeof why));
assert(issd_launcher_move(0,7,-1)==6);
assert(issd_launcher_move(6,7,1)==0);
assert(issd_launcher_move(0,0,1)==0);
issd_launcher_adjust(&c,ISSD_LAUNCHER_GAMEPLAY,3,1);
assert(c.gameplay_short_goal_kicks);
issd_launcher_adjust(&c,ISSD_LAUNCHER_GAMEPLAY,5,1);assert(c.match_minutes==1);
issd_launcher_adjust(&c,ISSD_LAUNCHER_GAMEPLAY,5,-1);assert(!c.match_minutes);
issd_launcher_adjust(&c,ISSD_LAUNCHER_CONTROLS,3,1);assert(c.player_profiles[3].schema==1);
return 0;}
''')
    invalid=tmp_path/'wrong.sfc';invalid.write_bytes(b'wrong')
    exe=tmp_path/'launcher_model.exe'
    compile_c(exe,ROOT,[source,ROOT/'ISSDNative/issd_launcher_model.c',ROOT/'ISSDNative/issd_config.c',
        ROOT/'deps/snesrecomp/runner/src/sha256.c'],[ROOT/'ISSDNative',ROOT/'deps/snesrecomp/runner/src'])
    result=subprocess.run([str(exe),str(ROOT/'International Superstar Soccer Deluxe (USA).sfc'),str(invalid)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
