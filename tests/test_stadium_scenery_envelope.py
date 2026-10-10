"""Original interpreter partition checks; controlled coordinates, not playthrough evidence."""
from pathlib import Path
import re
import subprocess
import pytest
from test_config_persistence import c_compiler


def test_original_scenery_callbacks_clear_every_supported_pitch_and_net_envelope(tmp_path):
    root = Path(__file__).resolve().parents[1]
    rom = root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom.exists():
        pytest.skip('Requires private cartridge')
    assembly = (root/'deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm').read_text()
    callbacks = assembly.split('CODE_83A09C:',1)[1].split('CODE_83A820:',1)[0]
    bounds = {0,72,-72,576,640,704,224,240,256,288,304,320,336,352,368,384,400}
    for word in re.findall(r'(?:CMP|CPY|CPX)\.w #\$([0-9A-Fa-f]{4})',callbacks):
        value = int(word,16)
        bounds.add(value-65536 if value>=32768 else value)
    for length in range(1536,2177,32):
        bounds.update((length,length+72))
    ordered = sorted(bounds)
    points = {value+delta for value in ordered for delta in (-1,0,1)}
    points.update((left+right)//2 for left,right in zip(ordered,ordered[1:]))
    assert len(points) <= 1024, 'Expand the harness partition allocation before adding thresholds'
    fixture = tmp_path/'partitions.txt'
    fixture.write_text('\n'.join(str(value&65535) for value in sorted(points)))
    executable = tmp_path/'scenery.exe'
    sources = [root/'tests/test_stadium_scenery_envelope.c',root/'deps/snesrecomp/runner/src/snes/interp816.c']
    include = root/'deps/snesrecomp/runner/src/snes'
    compiler = c_compiler()
    command = ([compiler,'/nologo','/O2','/I'+str(include),*map(str,sources),'/Fe:'+str(executable)]
               if compiler == 'cl' else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',
                   '-I',str(include),*map(str,sources),'-o',str(executable)])
    built = subprocess.run(command,capture_output=True,text=True,cwd=root)
    assert built.returncode == 0,built.stdout+built.stderr
    result = subprocess.run([str(executable),str(rom),str(fixture)],capture_output=True,text=True,timeout=180)
    assert result.returncode == 0,result.stdout+result.stderr
    assert 'original-interpreter scenery cases passed' in result.stdout
