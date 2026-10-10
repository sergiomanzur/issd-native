import json
from PIL import Image
from test_stadium_assets import manifest


def exporter():
    from tools.mod_studio import export
    return export.export_pack


def pack():
    return {'name': 'Independent', 'stadium_count': 9, 'unknown': {'keep': True},
        'teams': [], 'stadiums': [{'stadium_id': 8, 'name': 'HOME',
        'stadium_profile': {'version': 1, 'base_layout': 0,
            'geometry': {'length_units': 1728, 'width_units': 576},
            'artwork': 'art/stadium.json'}}]}


def test_export_is_relocatable_and_excludes_unreferenced_private_files(tmp_path):
    source = tmp_path / 'source'
    art = source / 'art'
    art.mkdir(parents=True)
    (art / 'stadium.json').write_text(json.dumps(manifest(art)))
    (source / 'private.sfc').write_bytes(b'cartridge sentinel')
    (source / 'campaign.sav').write_bytes(b'save sentinel')
    (source / 'orphan.bin').write_bytes(b'old history artifact')
    destination = tmp_path / 'exported'
    filename = exporter()(pack(), source, destination)
    source.rename(tmp_path / 'unavailable')
    exported = json.loads(filename.read_text())
    assert exported['unknown'] == {'keep': True}
    from tools.stadium_profile import validate_profile
    assert not validate_profile(exported['stadiums'][0], destination)
    assert not any((destination / name).exists() for name in ('private.sfc', 'campaign.sav', 'orphan.bin'))


def test_missing_dependency_does_not_publish_partial_export(tmp_path):
    import pytest
    source = tmp_path / 'source'
    source.mkdir()
    with pytest.raises(ValueError):
        exporter()(pack(), source, tmp_path / 'export')
    assert not (tmp_path / 'export').exists()


def test_export_copies_only_referenced_team_photo_and_preserves_paths(tmp_path):
    source = tmp_path / 'source'
    source.mkdir()
    Image.new('RGBA', (16, 16), 'red').save(source / 'photo.bmp')
    value = {'name': 'Photo', 'teams': [{'team_id': 0, 'photo': 'photo.bmp'}], 'stadiums': []}
    filename = exporter()(value, source, tmp_path / 'export')
    assert (filename.parent / 'photo.bmp').exists()
    assert json.loads(filename.read_text())['teams'][0]['photo'] == 'photo.bmp'
