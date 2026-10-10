"""Owned, isolated native test sessions with nonblocking process control."""
import copy
from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import time
import uuid
from .export import export_pack, _validate
from .stadium_geometry import read_template,compile_geometry


@dataclass(frozen=True)
class LaunchRequest:
    executable: Path
    rom: Path
    stadium_id: int
    pack: dict
    source_dir: Path
    test_root: Path


@dataclass(frozen=True)
class StagedRun:
    directory: Path
    config: Path
    mods: Path
    saves: Path
    script: Path
    log: Path
    environment: dict
    request: LaunchRequest


@dataclass(frozen=True)
class RunStatus:
    state: str
    exit_code: int | None
    message: str
    log: Path | None


def menu_script(stadium_id, count):
    if (type(stadium_id) is not int or type(count) is not int or
            not 8 <= count <= 32 or not 0 <= stadium_id < count):
        raise ValueError('Choose a stadium within this pack\'s 8–32 slots')
    entries = {}
    for frame in range(60, 361, 60):
        entries.update({frame: 'START', frame+10: 'NONE'})
    for frame in (560, 800, 950, 1100, 1250):
        entries.update({frame: 'A', frame+10: 'NONE'})
    if stadium_id == 0:
        entries.update({1460: 'LEFT', 1470: 'NONE'})
    else:
        for step in range(stadium_id-1):
            frame = 1460+step*60
            entries.update({frame: 'RIGHT', frame+10: 'NONE'})
    confirm = 1600+max(1,stadium_id-1)*60
    for step in range(13):
        frame = confirm+step*150
        entries.update({frame: 'A', frame+10: 'NONE'})
    return ''.join(f'{frame} {button}\n' for frame,button in sorted(entries.items()))


def stage(request):
    executable, rom = Path(request.executable).resolve(), Path(request.rom).resolve()
    if not executable.is_file():
        raise ValueError('Choose an existing game executable')
    if not rom.is_file():
        raise ValueError('Choose your cartridge dump')
    pack = copy.deepcopy(request.pack)
    _validate(pack,Path(request.source_dir).resolve())
    cartridge,templates = None,{}
    for stadium in pack.get('stadiums',[]):
        profile = stadium.get('stadium_profile')
        if not profile or profile.get('artwork'):
            continue  # final authored maps were validated by the shared compiler
        if cartridge is None:
            cartridge = rom.read_bytes()
        base = profile['base_layout']
        if base not in templates:
            templates[base] = read_template(cartridge,base)
        compiled = compile_geometry(profile,templates[base])
        if compiled.diagnostics:
            raise ValueError(f"Stadium {stadium.get('stadium_id')}: "+
                             '\n'.join(d.message for d in compiled.diagnostics))
    count = pack.get('stadium_count', 8)
    script = menu_script(request.stadium_id, count)
    name = f'Studio Test {uuid.uuid4().hex[:12]}'
    pack['name'] = name
    root = Path(request.test_root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    directory = root / uuid.uuid4().hex
    directory.mkdir()
    mods, saves = directory/'mods', directory/'saves'
    saves.mkdir()
    export_pack(pack, request.source_dir, mods)
    config = directory/'isolated.cfg'
    config.write_text(f'active_mod_packs = {name}\nengine_mode = 0\ninternal_res = 0\n'
        'gameplay_goalkeeper_ai = false\ngameplay_player_ai = false\n'
        'gameplay_bug_fixes = false\n', encoding='utf-8')
    inputs = directory/'input.txt'
    inputs.write_text(script, encoding='utf-8')
    environment = {key:value for key,value in os.environ.items()
        if not key.startswith(('ISSD_', 'SNESRECOMP_')) and
        key not in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER')}
    normalized = LaunchRequest(executable,rom,request.stadium_id,copy.deepcopy(request.pack),
                               Path(request.source_dir).resolve(),root)
    return StagedRun(directory,config,mods,saves,inputs,directory/'native.log',environment,normalized)


class GameRunner:
    def __init__(self):
        self.child = None
        self.run = None
        self._log = None
        self._stop_deadline = None
        self.status = RunStatus('staged',None,'No test launched',None)

    def start(self, run):
        if self.child and self.child.poll() is None:
            raise RuntimeError('Stop the current test before launching another')
        self.poll()
        self.run = run
        self._stop_deadline = None
        try:
            self._log = run.log.open('wb')
            arguments = [str(run.request.executable),'--rom',str(run.request.rom),
                '--config',str(run.config),'--mods-dir',str(run.mods),
                '--save-dir',str(run.saves),'--script',str(run.script)]
            self.child = subprocess.Popen(arguments,cwd=run.directory,
                stdout=self._log,stderr=subprocess.STDOUT,env=run.environment)
            self.status = RunStatus('running',None,'Test running',run.log)
        except OSError as exc:
            if self._log:
                self._log.close(); self._log = None
            self.child = None
            self.status = RunStatus('failed',None,str(exc),run.log)
        return self.status

    def poll(self):
        if not self.child:
            return self.status
        code = self.child.poll()
        if code is None:
            if self._stop_deadline is not None and time.monotonic() >= self._stop_deadline:
                self.child.kill()
            return self.status
        if self._log:
            self._log.close(); self._log = None
        stopped = self._stop_deadline is not None
        state = 'stopped' if stopped else ('exited' if code == 0 else 'failed')
        message = 'Test stopped' if stopped else f'Test exited with code {code}'
        self.status = RunStatus(state,code,message,self.run.log)
        return self.status

    def stop(self):
        if self.child and self.child.poll() is None:
            self._stop_deadline = time.monotonic()+.75
            self.child.terminate()
            self.status = RunStatus('running',None,'Stopping test…',self.run.log)
        return self.poll()
