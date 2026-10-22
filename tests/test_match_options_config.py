from pathlib import Path
import subprocess
from test_config_persistence import compile_c

def test_new_options_persist_without_changing_cartridge_enum(tmp_path):
    root = Path(__file__).resolve().parents[1]
    source = tmp_path / 'options_config.c'
    source.write_text('''#include "issd_config.h"
#include <assert.h>
int main(int n,char**v){(void)n;IssdConfig a,b;issd_config_init_defaults(&a);
assert(!a.gameplay_short_goal_kicks && !a.gameplay_final_attack && !a.match_minutes);
a.gameplay_short_goal_kicks=a.gameplay_final_attack=a.team_strength=true;
a.match_minutes=20;a.match_custom.duration=2;
assert(issd_config_save(&a,v[1]));issd_config_init_defaults(&b);assert(issd_config_load(&b,v[1]));
assert(b.gameplay_short_goal_kicks && b.gameplay_final_attack && b.team_strength);
assert(b.match_minutes==20 && b.match_custom.duration==2);return 0;}
''')
    exe = tmp_path / 'cfg.exe'
    compile_c(exe, root, [source, root / 'ISSDNative/issd_config.c'], [root / 'ISSDNative'])
    result = subprocess.run([str(exe), str(tmp_path / 'config.cfg')], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
