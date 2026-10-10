"""Publish a portable pack containing only schema-defined dependencies."""
import copy
import json
import os
from pathlib import Path
import shutil
import tempfile
import struct
from PIL import Image
try:
    from .. import validate_mod as validator
    from ..stadium_profile import resolve_asset
except ImportError:
    import validate_mod as validator
    from stadium_profile import resolve_asset
from . import repo
from .stadium_assets import compile_manifest


def _validate(pack, directory):
    if not validator.known_formations():
        validator.FORMATION_NAMES = repo.formation_names()
    report = validator.Report()
    validator.validate_data(pack, report, directory, '<export>')
    if report.errors:
        raise ValueError('\n'.join(report.errors))


def export_pack(pack, source_dir, destination):
    source = Path(source_dir).resolve()
    destination = Path(destination).resolve()
    if destination.exists():
        raise ValueError('Choose a new export directory')
    exported = copy.deepcopy(pack)
    _validate(exported, source)
    resources, manifests = {}, {}

    def register(filename):
        filename = Path(filename).resolve()
        try:
            relative = filename.relative_to(source)
        except ValueError as exc:
            raise ValueError('An export dependency escapes the source pack') from exc
        if relative == Path('mod.json'):
            raise ValueError('A resource collides with the exported mod.json')
        if relative in resources and resources[relative] != filename:
            raise ValueError(f'Export path collision: {relative}')
        resources[relative] = filename
        return relative.as_posix()

    try:
        for team in exported.get('teams', []):
            for field in ('photo', 'flag'):
                if not team.get(field):
                    continue
                filename = resolve_asset(source, team[field])
                with filename.open('rb') as file:
                    header = file.read(54)
                if len(header) != 54 or header[:2] != b'BM' or struct.unpack_from('<H',header,28)[0] != 32:
                    raise ValueError(f'{field} must be a 32-bit BMP file')
                with Image.open(filename) as image:
                    if image.format != 'BMP':
                        raise ValueError(f'{field} must be a BMP file')
                    image.verify()
                team[field] = register(filename)
        for stadium in exported.get('stadiums', []):
            profile = stadium.get('stadium_profile') or {}
            if not profile.get('artwork'):
                continue
            filename = resolve_asset(source, profile['artwork'])
            manifest = json.loads(filename.read_text(encoding='utf-8'))
            compiled = compile_manifest(manifest, filename.parent)
            if compiled.diagnostics:
                raise ValueError('\n'.join(d.message for d in compiled.diagnostics))
            for record in manifest['layers']:
                for field in ('metatiles', 'world_map'):
                    resource = resolve_asset(filename.parent, record[field])
                    register(resource)
                    record[field] = resource.relative_to(filename.parent).as_posix()
            for record in manifest.get('tiles', [])+manifest.get('hd', []):
                resource = resolve_asset(filename.parent, record['file'])
                register(resource)
                record['file'] = resource.relative_to(filename.parent).as_posix()
            if 'palette' in manifest:
                resource = resolve_asset(filename.parent, manifest['palette'])
                register(resource)
                manifest['palette'] = resource.relative_to(filename.parent).as_posix()
            name = register(filename)
            manifests[Path(name)] = manifest
            profile['artwork'] = name
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(str(exc)) from exc
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.studio-export-', dir=destination.parent))
    try:
        for relative, filename in resources.items():
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if relative in manifests:
                target.write_text(json.dumps(manifests[relative],indent=2), encoding='utf-8')
            else:
                shutil.copyfile(filename, target)
        (stage / 'mod.json').write_text(json.dumps(exported,indent=2), encoding='utf-8')
        _validate(exported, stage)
        os.replace(stage, destination)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    return destination / 'mod.json'
