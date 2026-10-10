import os
from pathlib import Path
import subprocess
import time
import pytest
from test_config_persistence import compile_c


def launcher():
    from tools.mod_studio import game_launch
    return game_launch


def request(module, tmp_path, executable):
    rom = tmp_path / 'private.sfc'
    rom.write_bytes(b'untouched cartridge')
    pack = {'name': 'My pack', 'stadium_count': 9, 'teams': [],
            'stadiums': [{'stadium_id': 8, 'name': 'HOME'}]}
    return module.LaunchRequest(executable, rom, 8, pack, tmp_path, tmp_path / 'sessions')


def test_stage_is_isolated_and_menu_scripts_cover_zero_and_last_slot(tmp_path):
    module = launcher()
    executable = tmp_path / 'game.exe'
    executable.write_bytes(b'executable sentinel')
    value = request(module, tmp_path, executable)
    first, second = module.stage(value), module.stage(value)
    assert first.directory != second.directory
    assert first.saves.is_dir() and not list(first.saves.iterdir())
    assert 'active_mod_packs = Studio Test ' in first.config.read_text()
    assert first.mods.joinpath('mod.json').is_file()
    assert first.script.read_text() == module.menu_script(8, 9)
    assert '1460 LEFT' in module.menu_script(0, 32)
    assert module.menu_script(31, 32).count('RIGHT') == 30
    assert value.rom.read_bytes() == b'untouched cartridge'
    assert value.pack['name'] == 'My pack'


def test_stage_reports_template_metatile_overflow_before_creating_session(tmp_path):
    module=launcher()
    root=Path(__file__).resolve().parents[1]
    rom=root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom.exists():pytest.skip('Requires private cartridge')
    executable=tmp_path/'game.exe';executable.write_bytes(b'executable sentinel')
    pack={'name':'Budget','stadium_count':9,'teams':[],'stadiums':[{'stadium_id':8,'name':'CUSTOM',
        'stadium_profile':{'version':1,'base_layout':1,'geometry':{'length_units':1568,'width_units':640}}}]}
    request=module.LaunchRequest(executable,rom,8,pack,tmp_path,tmp_path/'sessions')
    with pytest.raises(ValueError,match='256 native metatiles'):
        module.stage(request)
    assert not request.test_root.exists()


def test_runner_is_nonblocking_and_stops_only_its_owned_child(tmp_path):
    module = launcher()
    repo = Path(__file__).resolve().parents[1]
    source = tmp_path / 'child.c'
    source.write_text('#ifdef _WIN32\n#include <windows.h>\n#else\n#include <unistd.h>\n#endif\n'
        '#include <stdlib.h>\n#include <stdio.h>\nint main(void){puts("started");fflush(stdout);\n'
        '#ifdef _WIN32\nSleep(2000);\n#else\nsleep(2);\n#endif\n'
        'return getenv("TEST_FAIL") ? 7 : 0;}\n')
    executable = tmp_path / 'child.exe'
    compile_c(executable, repo, [source], [])
    unrelated = subprocess.Popen([str(executable)], stdout=subprocess.DEVNULL)
    try:
        run = module.stage(request(module, tmp_path, executable))
        runner = module.GameRunner()
        started = time.monotonic()
        runner.start(run)
        assert time.monotonic()-started < 1.5
        assert runner.poll().state == 'running'
        runner.stop()
        for _ in range(50):
            if runner.poll().state == 'stopped':
                break
            time.sleep(.02)
        assert runner.poll().state == 'stopped'
        assert unrelated.poll() is None
        failed = module.stage(request(module, tmp_path, executable))
        failed.environment['TEST_FAIL'] = '1'
        runner.start(failed)
        for _ in range(150):
            status = runner.poll()
            if status.state == 'failed':
                break
            time.sleep(.02)
        assert status.state == 'failed' and status.exit_code == 7
        assert 'started' in failed.log.read_text()
    finally:
        if unrelated.poll() is None:
            unrelated.terminate()
        unrelated.wait(timeout=3)
