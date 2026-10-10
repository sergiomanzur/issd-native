"""Shared stadium-profile validation for the checker and Mod Studio.

Validation establishes authoring constraints, not native execution acceptance.
The runtime must additionally require a verified construction contract.
"""
from dataclasses import dataclass
from pathlib import Path, PureWindowsPath
import json


@dataclass(frozen=True)
class Diagnostic:
    severity: str
    path: tuple
    message: str


# Length, width, min/max Y, left shear anchor, X cap, right shear anchor.
TEMPLATES = (
    (1792, 576, 96, 672, 256, 2368, 2000),
    (1856, 640, 96, 704, 192, 2464, 2000),
    (1984, 704, 64, 736, 192, 2688, 2304),
    (2048, 640, 64, 736, 192, 2688, 2304),
    (1920, 640, 96, 704, 192, 2560, 2304),
    (1920, 576, 128, 640, 192, 2464, 2000),
    (1792, 704, 64, 736, 192, 2464, 2000),
    (2176, 704, 64, 704, 192, 2880, 2368),
)
CAMERA_FIELDS = ('min_y', 'max_y', 'left_shear_anchor', 'max_x_cap',
                 'right_shear_anchor')


def resolve_asset(pack_dir, name):
    """Resolve a portable, existing asset without escaping its pack directory."""
    if not isinstance(name, str) or not name or '\\' in name:
        raise ValueError('Use a nonempty relative path with forward slashes')
    path = Path(name)
    if path.is_absolute() or PureWindowsPath(name).drive or '..' in path.parts:
        raise ValueError('Asset must stay inside the pack directory')
    root = Path(pack_dir).resolve()
    resolved = (root / path).resolve()
    if not resolved.is_relative_to(root):
        raise ValueError('Asset resolves outside the pack directory')
    if not resolved.is_file():
        raise ValueError('Asset file does not exist')
    return resolved


def validate_profile(stadium, pack_dir):
    errors = []

    def error(path, message):
        errors.append(Diagnostic('error', ('stadium_profile',) + path, message))

    if not isinstance(stadium, dict):
        return [Diagnostic('error', (), 'Stadium must be an object')]
    if 'stadium_profile' not in stadium:
        return errors
    profile = stadium['stadium_profile']
    if not isinstance(profile, dict):
        error((), 'Profile must be an object')
        return errors
    if type(profile.get('version')) is not int or profile['version'] != 1:
        error(('version',), 'Only stadium profile version 1 is supported')
    base = profile.get('base_layout')
    if type(base) is not int or not 0 <= base < len(TEMPLATES):
        error(('base_layout',), 'Choose an original layout from 0 to 7')
        return errors
    template = TEMPLATES[base]
    geometry = profile.get('geometry')
    if not isinstance(geometry, dict):
        error(('geometry',), 'Geometry must be an object')
        return errors
    for key, minimum, maximum in (
            ('length_units', 1536, template[0]),
            ('width_units', 512, template[1])):
        value = geometry.get(key)
        if type(value) is not int:
            error(('geometry', key), 'Use an integer number of engine units')
        elif not minimum <= value <= maximum or value % 32:
            error(('geometry', key),
                  f'Use a multiple of 32 between {minimum} and {maximum}')
    width = geometry.get('width_units')
    # Until each transverse net/scenery intersection is measured, a shifted
    # goal opening cannot be certified from a shrink-only rectangle argument.
    if type(width) is int and width != template[1]:
        error(('geometry', 'width_units'),
              'Changed width requires a verified net and scenery envelope')
    camera = geometry.get('camera', {})
    if not isinstance(camera, dict):
        error(('geometry', 'camera'), 'Camera must be an object')
    else:
        resolved = dict(zip(CAMERA_FIELDS, template[2:]))
        for key, value in camera.items():
            if key not in resolved:
                error(('geometry', 'camera', key), 'Unknown camera field')
            elif type(value) is not int or not 0 <= value <= 65535:
                error(('geometry', 'camera', key), 'Use an unsigned 16-bit integer')
            elif value != resolved[key]:
                error(('geometry', 'camera', key),
                      'Custom camera bounds require verified map coverage')
            else:
                resolved[key] = value
        if resolved['min_y'] > resolved['max_y']:
            error(('geometry', 'camera'), 'Minimum Y must not exceed maximum Y')
    if 'artwork' in profile:
        try:
            manifest_path = resolve_asset(pack_dir, profile['artwork'])
            manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
            try:
                from .mod_studio.stadium_assets import compile_manifest
            except ImportError:
                from mod_studio.stadium_assets import compile_manifest
            compiled = compile_manifest(manifest, manifest_path.parent)
            if isinstance(manifest, dict) and manifest.get('base_layout') != base:
                error(('artwork', 'base_layout'), 'Artwork must use the profile base layout')
            if isinstance(manifest,dict) and isinstance(manifest.get('geometry'),dict):
                for field in ('length_units','width_units'):
                    if manifest['geometry'].get(field) != geometry.get(field):
                        error(('artwork','geometry',field),'Recompile artwork pitch maps after changing geometry')
            for diagnostic in compiled.diagnostics:
                error(('artwork',) + diagnostic.path, diagnostic.message)
        except (ValueError, OSError) as exc:
            error(('artwork',), str(exc))
    return errors


def validate_pack_data(pack, pack_dir, report):
    """Append structured paths while preserving the existing Report API."""
    if not hasattr(report, 'diagnostics'):
        report.diagnostics = []
    if not isinstance(pack, dict) or not isinstance(pack.get('stadiums', []), list):
        return  # The existing checker reports invalid outer structure.
    for index, stadium in enumerate(pack.get('stadiums', [])):
        for diagnostic in validate_profile(stadium, pack_dir):
            located = Diagnostic(diagnostic.severity,
                                 ('stadiums', index) + diagnostic.path,
                                 diagnostic.message)
            report.diagnostics.append(located)
            path = '.'.join(str(part) for part in located.path)
            report.errors.append(f'{path}: {located.message}')
