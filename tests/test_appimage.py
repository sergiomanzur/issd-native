"""Run AppRun against a test-owned payload, including paths with spaces."""
from pathlib import Path
import os
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[1]

def test_apprun_preserves_user_mods_and_resolves_relative_paths(tmp_path):
    if os.name=='nt':
        wsl=shutil.which('wsl')
        if not wsl:pytest.skip('requires bash/WSL')
        def linux(p):return '/mnt/'+p.drive[0].lower()+p.as_posix()[2:]
        command=[wsl,'-d','Ubuntu-24.04','--','bash']
    else:
        linux=str;command=['bash']
    app=tmp_path/'Matchday AppDir';data=tmp_path/'owned data';launch=tmp_path/'launch here'
    (app/'usr/bin').mkdir(parents=True);(app/'usr/lib').mkdir()
    (app/'usr/share/issd-native/mods').mkdir(parents=True)
    (data/'mods').mkdir(parents=True);launch.mkdir()
    (data/'mods/custom.json').write_text('MY MOD')
    (data/'mods/default.json').write_text('MY EDIT')
    (app/'usr/share/issd-native/mods/default.json').write_text('BUNDLED')
    (launch/'my cartridge.sfc').write_bytes(b'test-owned fixture')
    payload=app/'usr/bin/ISSDNative'
    payload.write_text('#!/bin/bash\nprintf "%s\\n" "$PWD" "$@" > "$ISSD_DATA_DIR/observed.txt"\n', newline="\n")
    run=app/'AppRun';shutil.copyfile(ROOT/'packaging/appimage/AppRun',run)
    script=tmp_path/'run.sh'
    script.write_text(f'''#!/bin/bash
set -eu
chmod +x '{linux(payload)}'
cd '{linux(launch)}'
ISSD_DATA_DIR='{linux(data)}' bash '{linux(run)}' --rom 'my cartridge.sfc' --config 'my config.cfg' --headless 1
''', newline='\n')
    result=subprocess.run(command+[linux(script)],capture_output=True,text=True,timeout=40)
    assert result.returncode==0,result.stdout+result.stderr
    args=(data/'observed.txt').read_text().splitlines()
    assert linux(data)==args[0]
    assert linux(launch/'my cartridge.sfc') in args
    assert linux(launch/'my config.cfg') in args
    assert (data/'mods/custom.json').read_text()=='MY MOD'
    assert (data/'mods/default.json').read_text()=='MY EDIT'
