"""Profile fields are engine units; invalid drafts must produce useful paths."""
import importlib.util
from pathlib import Path

PATH = Path(__file__).resolve().parents[1] / 'tools/stadium_profile.py'


def validate(stadium, directory):
    assert PATH.exists(), 'Shared profile validation is missing'
    from tools import stadium_profile as module
    return module.validate_profile(stadium, directory)


def stadium(length=1728, width=576):
    return {'stadium_id': 8, 'stadium_profile': {
        'version': 1, 'base_layout': 0,
        'geometry': {'length_units': length, 'width_units': width}}}


def test_display_yards_do_not_define_playable_geometry(tmp_path):
    value = stadium()
    value.update(pitch_length=138, pitch_width=90)
    assert validate(value, tmp_path) == []
    assert value['stadium_profile']['geometry']['length_units'] == 1728


def test_legacy_entry_is_valid(tmp_path):
    assert validate({'stadium_id': 8, 'name': 'LEGACY'}, tmp_path) == []


def test_invalid_integer_types_have_field_paths(tmp_path):
    for value in (True, 1728.0, '1728', None):
        errors = validate(stadium(value), tmp_path)
        assert any(e.path == ('stadium_profile', 'geometry', 'length_units')
                   and e.severity == 'error' for e in errors)


def test_dimensions_obey_template_and_alignment(tmp_path):
    for length in (1504, 1697, 1824):
        assert validate(stadium(length), tmp_path)
    assert validate(stadium(1664), tmp_path) == []


def test_camera_rejects_invalid_and_unproven_intervals(tmp_path):
    for camera in ({'min_y': 704, 'max_y': 96}, {'max_x_cap': 65535},
                   {'min_y': True}, {'max_y': 100.0}):
        value = stadium()
        value['stadium_profile']['geometry']['camera'] = camera
        assert validate(value, tmp_path)


def test_missing_or_escaping_artwork_rejected(tmp_path):
    for path in ('missing/stadium.json', '../outside.json', 'C:/outside.json'):
        value = stadium()
        value['stadium_profile']['artwork'] = path
        assert validate(value, tmp_path)


def test_unknown_version_and_invalid_profile_objects(tmp_path):
    value = stadium()
    value['stadium_profile']['version'] = 2
    assert validate(value, tmp_path)
    for profile in (None, [], False):
        assert validate({'stadium_id': 8, 'stadium_profile': profile}, tmp_path)
