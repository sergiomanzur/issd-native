"""Assemble explicit, ROM-free Windows/Linux/Android/editor beta assets."""
from pathlib import Path, PurePosixPath, PureWindowsPath
import argparse
import hashlib
import importlib.metadata
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def _safe_name(name):
    path = PurePosixPath(name)
    if (not name or '\\' in name or path.is_absolute() or
            PureWindowsPath(name).drive or '..' in path.parts):
        raise ValueError(f'Unsafe archive member: {name}')


def _validate_files(files, prefix):
    if prefix:
        _safe_name(prefix.rstrip('/'))
    for name, source in files.items():
        _safe_name(name)
        if not Path(source).is_file():
            raise FileNotFoundError(source)


def archive_zip(destination, files, prefix='', empty_dirs=()):
    destination = Path(destination)
    _validate_files(files, prefix)
    for folder in empty_dirs:
        _safe_name(folder)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name+'.tmp')
    try:
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as package:
            for folder in empty_dirs:
                package.writestr(f'{prefix}{folder}/', b'')
            for relative, source in sorted(files.items()):
                package.write(source, f'{prefix}{relative}')
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination


def archive_tar(destination, files, prefix=''):
    destination = Path(destination)
    _validate_files(files, prefix)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name+'.tmp')
    try:
        with tarfile.open(temporary, 'w:gz') as package:
            for relative, source in sorted(files.items()):
                info = package.gettarinfo(str(source), f'{prefix}{relative}')
                info.mode = 0o755 if relative == 'ISSDNative' else 0o644
                info.uid = info.gid = 0
                info.uname = info.gname = ''
                with Path(source).open('rb') as data:
                    package.addfile(info, data)
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination


def editor_sources():
    files = {f'tools/{name}': ROOT/'tools'/name for name in
             ('mod_studio_launch.py', 'validate_mod.py', 'stadium_profile.py')}
    for source in (ROOT/'tools/mod_studio').glob('*.py'):
        files[f'tools/mod_studio/{source.name}'] = source
    for name in ('baked.json', 'README.md'):
        files[f'tools/mod_studio/{name}'] = ROOT/'tools/mod_studio'/name
    for source in (ROOT/'tools/mod_studio/assets').rglob('*.png'):
        files[source.relative_to(ROOT).as_posix()] = source
    return files


def editor_licenses():
    files = {'licenses/mod-studio/Python.txt': Path(sys.prefix)/'LICENSE.txt',
             'licenses/mod-studio/Tk.txt': Path(sys.prefix)/'tcl/tk8.6/license.terms'}
    for package, name in (('Pillow', 'Pillow.txt'), ('pyinstaller', 'PyInstaller.txt')):
        distribution = importlib.metadata.distribution(package)
        candidates = [file for file in distribution.files
                      if '/licenses/' in str(file) and
                      Path(str(file)).name in ('LICENSE', 'COPYING.txt')]
        if len(candidates) != 1:
            raise ValueError(f'Locate the {package} distribution license before packaging')
        files[f'licenses/mod-studio/{name}'] = distribution.locate_file(candidates[0])
    return files


def android_apk(android_build, version, version_code):
    metadata = json.loads((android_build/'output-metadata.json').read_text())
    element, = metadata['elements']
    if element['versionName'] != version or element['versionCode'] != version_code:
        raise ValueError('Rebuild Android: APK version/name code does not match release')
    _safe_name(element['outputFile'])
    source = android_build/element['outputFile']
    if not source.is_file():
        raise FileNotFoundError(source)
    return source


def assemble(windows_build, linux_build, android_build, studio_exe):
    version = (ROOT/'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+-beta\.\d+', version):
        raise ValueError('Expected a beta version in VERSION')
    tag = f'v{version}'
    output = ROOT/'dist/releases'/tag
    output.mkdir(parents=True, exist_ok=True)
    gradle = (ROOT/'android/app/build.gradle').read_text()
    version_code = int(re.search(r'\bversionCode\s+(\d+)', gradle).group(1))
    apk_source = android_apk(android_build, version, version_code)
    common = {'README.md': ROOT/'README.md', 'CHANGELOG.md': ROOT/'CHANGELOG.md',
              'VERSION': ROOT/'VERSION', 'RELEASE_NOTES.md': ROOT/f'docs/releases/{tag}.md',
              f'docs/releases/{tag}.md': ROOT/f'docs/releases/{tag}.md',
              'licenses/SNESRecomp.txt': ROOT/'deps/snesrecomp/LICENSE',
              'licenses/SDL2.txt': ROOT/'deps/SDL2/LICENSE.txt'}
    documents = ('GAMEPLAY_TWEAKS LOCAL_MULTIPLAYER CAMPAIGN_SAVES CAMPAIGN_CARTRIDGE_FLOW '
                 'PASSWORD_BRIDGE MODDING CONTROLS SUPPORTED_FEATURES ACCEPTANCE_TESTS '
                 'MATCH_SHORTCUTS ORIGINAL_BUG_FIXES GRAPHICS_SETTINGS GRAPHICS_MATCH_ACCEPTANCE '
                 'TEAM_CHANGES_ACCEPTANCE NETPLAY_DESIGN AUDIO_SYSTEM AUDIO_TIMING_FIX '
                 'GOAL_REPLAY_FIX SHOOTOUT_ACCEPTANCE SUBSTITUTION_FLOW_ACCEPTANCE '
                 'CONTINUOUS_CAMPAIGN_ACCEPTANCE CUP_WINNING_ACCEPTANCE EXTRA_STADIUM_ACCEPTANCE '
                 'STADIUM_PROFILE_CONTRACT INDEPENDENT_STADIUM_ACCEPTANCE MOD_STUDIO_QUICKSTART '
                 'RELEASE_VALIDATION_2026_10_10').split()
    common.update({f'docs/{name}.md': ROOT/f'docs/{name}.md' for name in documents})
    for source in (ROOT/'deps/snesrecomp/third_party/psxrecomp_color_lut').glob('LICENSE-*.txt'):
        common[f'licenses/psxrecomp_color_lut/{source.name}'] = source
    source_files = editor_sources()
    common.update({f'mod-studio-source/{name}': source for name, source in source_files.items()})
    common.update(editor_licenses())
    sys.path.insert(0, str(ROOT))
    from tools.mod_studio.export import export_pack
    example = ROOT/'mods/independent_stadium_example'
    pack = json.loads((example/'mod.json').read_text())
    assets = []
    with tempfile.TemporaryDirectory(prefix='issd-release-example-') as temporary:
        exported = Path(temporary)/'example'
        export_pack(pack, example, exported)
        example_files = {source.relative_to(exported).as_posix(): source
                         for source in exported.rglob('*') if source.is_file()}
        example_files['README.md'] = example/'README.md'
        for name, source in example_files.items():
            member = 'independent_stadium_example.json' if name == 'mod.json' else name
            if name == 'README.md':
                member = 'README-independent-stadium-example.md'
            common[f'mods/{member}'] = source
        windows = dict(common, **{'ISSDNative.exe': windows_build/'ISSDNative.exe',
                                  'SDL2.dll': windows_build/'SDL2.dll',
                                  'aot_boot_deny.txt': ROOT/'recomp/aot_boot_deny.txt',
                                  'ISSDModStudio.exe': studio_exe})
        assets.append(archive_zip(output/f'ISSDNative-{tag}-windows-x64.zip', windows,
                                  prefix=f'ISSDNative-{tag}-windows-x64/'))
        linux = dict(common, **{'ISSDNative': linux_build/'ISSDNative',
                                'aot_boot_deny.txt': ROOT/'recomp/aot_boot_deny.txt'})
        assets.append(archive_tar(output/f'ISSDNative-{tag}-linux-x64.tar.gz', linux,
                                  prefix=f'ISSDNative-{tag}-linux-x64/'))
        apk = output/f'ISSDNative-{tag}-android.apk'
        if not apk_source.is_file():
            raise FileNotFoundError(apk_source)
        temporary_apk = apk.with_suffix('.apk.tmp')
        shutil.copy2(apk_source, temporary_apk)
        os.replace(temporary_apk, apk)
        assets.append(apk)
        assets.append(archive_zip(output/f'ISSDNative-{tag}-android.zip',
                                  dict(common, **{apk.name: apk})))
        studio = dict(common, **{'ISSDModStudio.exe': studio_exe})
        assets.append(archive_zip(output/f'ISSDModStudio-{tag}-windows-x64.zip', studio,
                                  prefix=f'ISSDModStudio-{tag}-windows-x64/'))
        assets.append(archive_zip(output/f'independent_stadium_example-{tag}.zip', example_files))
    tracked = subprocess.check_output(['git', 'ls-files', '-z', 'mods'], cwd=ROOT).decode().split('\0')
    for manifest, directory in (('liga_mx_expansion.json', 'liga_mx'),
                                ('world_cup_2026.json', 'world_cup_2026')):
        files = {path.removeprefix('mods/'): ROOT/path for path in tracked
                 if path == f'mods/{manifest}' or path.startswith(f'mods/{directory}/')}
        if manifest not in files:
            raise ValueError(f'Missing tracked mod manifest: {manifest}')
        assets.append(archive_zip(output/f'{Path(manifest).stem}.zip', files))
    checksums = output/'SHA256SUMS.txt'
    checksums.write_text(''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n'
                                 for path in assets), encoding='ascii')
    for path in [*assets, checksums]:
        print(f'{path.relative_to(ROOT)} ({path.stat().st_size:,} bytes)')
    return assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--windows-build', type=Path, default=ROOT/'build')
    parser.add_argument('--linux-build', type=Path, default=ROOT/'build/release-validation/linux')
    parser.add_argument('--android-build', type=Path, default=ROOT/'android/app/build/outputs/apk/release')
    parser.add_argument('--studio-exe', type=Path, default=ROOT/'tools/mod_studio/dist/ISSDModStudio.exe')
    options = parser.parse_args()
    assemble(options.windows_build.resolve(), options.linux_build.resolve(),
             options.android_build.resolve(), options.studio_exe.resolve())


if __name__ == '__main__':
    main()
