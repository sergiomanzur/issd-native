"""Validate stadium construction certificates produced by native trace probes.

This module deliberately does not infer successful execution from disassembly
or a certificate's ``complete`` flag. Static investigation and runtime evidence
are separate inputs to the acceptance process.
"""
import re
import hashlib
import json
import os
import subprocess
from pathlib import Path

EVENTS = ('constructor', 'palette_complete', 'decompression_complete',
          'dma_complete', 'streamer', 'ppu')
HEX_DIGEST = re.compile(r'[0-9a-f]{64}\Z')
ORIGINAL_DIMENSIONS = ((1792,576), (1856,640), (1984,704), (2048,640),
                       (1920,640), (1920,576), (1792,704), (2176,704))


def validate_original_state(ram, layout):
    """Check the actual native dump, not only the requested picker input."""
    if (type(layout) is not int or not 0 <= layout < 8 or
            len(ram) != 0x20000):
        return False
    def word(address):
        return int.from_bytes(ram[address:address+2], 'little')
    length, width = ORIGINAL_DIMENSIONS[layout]
    mode = word(0x70)
    if mode != 8 and not (mode == 0x13 and (word(0xda2) or word(0xea2))):
        return False
    return all(word(address) == expected for address, expected in
               ((0x86, layout), (0x1fa2, layout),
                (0x12a2, length), (0x12a4, width)))


def _digest(value):
    return isinstance(value, str) and bool(HEX_DIGEST.fullmatch(value))


def _route(events):
    if not isinstance(events, list) or len(events) < len(EVENTS):
        return False
    # Each route records a single construction generation. Later transfers may
    # be present, but cannot substitute for that generation's completed upload.
    selected = []
    for kind in EVENTS:
        matches = [event for event in events
                   if isinstance(event, dict) and event.get('kind') == kind]
        if len(matches) != 1:
            return False
        selected.append(matches[0])
    ticks = [event.get('tick') for event in selected]
    generations = [event.get('generation') for event in selected]
    if any(type(tick) is not int or tick < 0 for tick in ticks):
        return False
    if any(type(generation) is not int or generation < 1
           for generation in generations):
        return False
    # Background preparation and pitch physics are separate native setup
    # phases. In exhibitions, initial streaming precedes pitch construction.
    # Check dependencies rather than inventing a total loader order.
    constructor, palette, decompression, dma, streamer, ppu = ticks
    return (len(set(generations)) == 1 and
            palette < dma and decompression < dma and
            dma < streamer < ppu and constructor < ppu)


def validate_evidence(value):
    """Return False for malformed or incomplete evidence without coercing types."""
    if not isinstance(value, dict) or type(value.get('version')) is not int:
        return False
    if value['version'] != 1 or value.get('complete') is not True:
        return False
    if not _digest(value.get('rom_sha256')):
        return False
    hashes = value.get('source_hashes')
    if not isinstance(hashes, dict) or not hashes:
        return False
    if any(not isinstance(path, str) or not path or not _digest(digest)
           for path, digest in hashes.items()):
        return False
    layouts = value.get('layouts')
    if not isinstance(layouts, list) or len(layouts) != 8:
        return False
    ids = set()
    for layout in layouts:
        if not isinstance(layout, dict):
            return False
        identity = layout.get('base_layout')
        if type(identity) is not int or not 0 <= identity <= 7 or identity in ids:
            return False
        ids.add(identity)
        if (layout.get('fresh_exhibition') is not True or
                layout.get('guest_memory_modified') is not False):
            return False
        envelope = layout.get('scenery_envelope')
        if not isinstance(envelope, dict) or any(
            envelope.get(key) is not True for key in
            ('verified', 'pitch_clear', 'nets_clear', 'consumer_audit_complete')
        ):
            return False
        routes = layout.get('routes')
        if not isinstance(routes, dict) or any(
            not _route(routes.get(route)) for route in ('live', 'replay')
        ):
            return False
    return ids == set(range(8))


def _exhibition_script(layout,frames=5000,pass_interval=0):
    entries = {}
    for frame in range(60, 361, 60):
        entries.update({frame: 'START', frame + 10: 'NONE'})
    for frame in (560, 800, 950, 1100, 1250):
        entries.update({frame: 'A', frame + 10: 'NONE'})
    if layout == 0:
        entries.update({1460: 'LEFT', 1470: 'NONE'})
    else:
        for step in range(layout - 1):
            frame = 1460 + step * 60
            entries.update({frame: 'RIGHT', frame + 10: 'NONE'})
    confirm = 1600 + max(1, layout - 1) * 60
    for step in range(13):
        frame = confirm + step * 150
        entries.update({frame: 'A', frame + 10: 'NONE'})
    if pass_interval:
        for frame in range(6000,frames,pass_interval):
            entries.update({frame:'B',frame+10:'NONE'})
    return ''.join(f'{frame} {button}\n' for frame, button in sorted(entries.items()))


def probe(executable: Path, rom: Path, output: Path, frames=5000, layouts=None, engine_mode=0,pass_interval=0) -> dict:
    """Capture original live routes without changing guest memory.

    A live probe alone never publishes a complete certificate. Replay routes,
    scenery partitions and the consumer audit must be supplied and verified
    separately. Preserve raw events rather than relabeling candidate PCs as
    proven completion boundaries.
    """
    executable, rom, output = (Path(path).resolve() for path in
                               (executable, rom, output))
    if type(engine_mode) is not int or engine_mode not in (0,1):
        raise ValueError('Choose native or interpreter engine mode')
    if type(pass_interval) is not int or (pass_interval != 0 and pass_interval < 20):
        raise ValueError('Use zero or a controller pass interval of at least20frames')
    output.mkdir(parents=True, exist_ok=True)
    if type(frames) is not int or frames < 5000:
        raise ValueError('Capture at least 5000 frames from a fresh boot')
    layouts = list(range(8)) if layouts is None else list(layouts)
    if (not layouts or len(set(layouts)) != len(layouts) or
            any(type(layout) is not int or not 0 <= layout < 8 for layout in layouts)):
        raise ValueError('Choose unique original layouts from 0 to 7')
    root = Path(__file__).resolve().parents[2]
    source_paths = set(root.glob('ISSDNative/*.[ch]'))
    source_paths.update(root.glob('recomp/generated/*.[ch]'))
    source_paths.update(root.glob('recomp/config/*.cfg'))
    source_paths.update(root/name for name in (
        'recomp/aot_boot_deny.txt','tools/ghidra/probe_stadium_contract.py',
        'tools/stadium_profile.py','tools/mod_studio/stadium_geometry.py',
        'tools/mod_studio/stadium_assets.py','cmake/issd_sources.cmake',
        'deps/snesrecomp/runner/src/common_rtl.c','deps/snesrecomp/runner/src/cpu_state.c',
        'deps/snesrecomp/runner/src/snes/cart.c','deps/snesrecomp/runner/src/snes/cart.h',
        'deps/snesrecomp/runner/src/snes/interp_bridge.c','deps/snesrecomp/runner/src/snes/ppu.c',
        'deps/snesrecomp/runner/src/snes/dma.c'))
    sources = sorted(path.relative_to(root).as_posix() for path in source_paths)
    evidence = {
        'version': 1, 'rom_sha256': hashlib.sha256(rom.read_bytes()).hexdigest(),
        'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
        'source_hashes': {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                          for name in sources},
        'complete': False, 'layouts': [], 'engine_mode':engine_mode,'controller_pass_interval':pass_interval,
    }
    for layout in layouts:
        directory = output / f'original-{layout}'
        directory.mkdir(exist_ok=True)
        for name in ('mods', 'saves'):
            (directory / name).mkdir(exist_ok=True)
        script = directory / 'input.txt'
        script.write_text(_exhibition_script(layout,frames,pass_interval), encoding='utf-8')
        config = directory / 'isolated.cfg'
        config.write_text(f'engine_mode = {engine_mode}\ninternal_res = 0\n'
                          'gameplay_goalkeeper_ai = false\n'
                          'gameplay_player_ai = false\n'
                          'gameplay_bug_fixes = false\n', encoding='utf-8')
        trace = directory / 'trace.jsonl'
        environment = dict(os.environ, SDL_VIDEODRIVER='dummy',
                           SDL_AUDIODRIVER='dummy', ISSD_STADIUM_TRACE=str(trace))
        command = [str(executable), '--rom', str(rom), '--config', str(config),
                   '--mods-dir', str(directory / 'mods'), '--save-dir',
                   str(directory / 'saves'), '--script', str(script),
                   '--headless', str(frames), '--dump-state', str(directory / 'state')]
        with (directory / 'native.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(command, cwd=directory, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=300)
        if result.returncode:
            raise RuntimeError(f'Original layout {layout} probe failed: {result.returncode}')
        if not trace.is_file():
            raise RuntimeError('Executable did not emit stadium instrumentation')
        ram = (directory / 'state.wram').read_bytes()
        if not validate_original_state(ram, layout):
            raise RuntimeError(f'Original layout {layout} did not reach its native match')
        events = [json.loads(line) for line in trace.read_text().splitlines()]
        evidence['layouts'].append({
            'base_layout': layout, 'fresh_exhibition': True,
            'guest_memory_modified': False, 'raw_live_events': events,
            'checkpoint_frame': frames,
            'raw_goal_replay_events': [event for event in events
                if event.get('mode') == 0x13 and event.get('submode') == 6
                and (event.get('score_home', 0) or event.get('score_away', 0))],
            'routes': {}, 'scenery_envelope': {'verified': False},
            'pending': ['classify transfer completion', 'replay route',
                        'scenery envelope', 'consumer audit'],
        })
        (output / 'evidence.json').write_text(json.dumps(evidence, indent=2),
                                             encoding='utf-8')
    return evidence


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=5000)
    parser.add_argument('--layouts', type=int, nargs='+')
    parser.add_argument('--engine-mode',type=int,choices=(0,1),default=0)
    parser.add_argument('--pass-interval',type=int,default=0)
    arguments = parser.parse_args()
    result = probe(arguments.exe, arguments.rom, arguments.output, arguments.frames, arguments.layouts,arguments.engine_mode,arguments.pass_interval)
    print(f"Captured {len(result['layouts'])} original live routes; "
          'complete certificate remains pending replay and collision evidence.')
