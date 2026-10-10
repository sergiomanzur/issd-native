"""Release archives use explicit public resources and preserve previous outputs."""
import tarfile
import zipfile
import json
import pytest
from tools import assemble_release as release


def test_zip_uses_explicit_inputs_and_preserves_previous_archive_on_failure(tmp_path):
    game=tmp_path/'game.exe';game.write_bytes(b'game')
    (tmp_path/'private.sfc').write_bytes(b'private cartridge')
    (tmp_path/'quicksave.sav').write_bytes(b'private save')
    archive=tmp_path/'package.zip'
    release.archive_zip(archive,{'ISSDNative.exe':game},prefix='beta/',empty_dirs=('mods',))
    original=archive.read_bytes()
    with zipfile.ZipFile(archive) as package:
        assert package.testzip() is None
        assert set(package.namelist())=={'beta/mods/','beta/ISSDNative.exe'}
    with pytest.raises(FileNotFoundError):
        release.archive_zip(archive,{'ISSDNative.exe':tmp_path/'missing.exe'})
    assert archive.read_bytes()==original


def test_linux_archive_retains_executable_permissions_and_public_file_list(tmp_path):
    game=tmp_path/'game';game.write_bytes(b'linux game')
    deny=tmp_path/'deny.txt';deny.write_text('deny')
    archive=tmp_path/'linux.tar.gz'
    release.archive_tar(archive,{'ISSDNative':game,'aot_boot_deny.txt':deny},prefix='beta/')
    with tarfile.open(archive) as package:
        assert package.getnames()==['beta/ISSDNative','beta/aot_boot_deny.txt']
        assert package.getmember('beta/ISSDNative').mode==0o755
        assert package.getmember('beta/aot_boot_deny.txt').mode==0o644
        assert package.extractfile('beta/ISSDNative').read()==b'linux game'


@pytest.mark.parametrize('name',['../outside','/absolute','C:/absolute','mods/../../outside'])
def test_archive_rejects_unsafe_member_names(tmp_path,name):
    file=tmp_path/'resource';file.write_bytes(b'data')
    for archive in (release.archive_zip,release.archive_tar):
        with pytest.raises(ValueError):archive(tmp_path/'output', {name:file})


@pytest.mark.parametrize('version,code',[('0.5.0-beta.1',6),('0.4.0-beta.1',7)])
def test_android_metadata_rejects_stale_name_or_upgrade_code_before_packaging(tmp_path,version,code):
    (tmp_path/'output-metadata.json').write_text(json.dumps({'elements':[
        {'versionName':version,'versionCode':code,'outputFile':'app-release.apk'}]}))
    with pytest.raises(ValueError,match='Rebuild Android'):
        release.android_apk(tmp_path,'0.5.0-beta.1',7)
