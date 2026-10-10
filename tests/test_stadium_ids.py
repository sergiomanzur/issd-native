import copy
from tools.mod_studio import model


def test_allocate_gap_and_clone_preserve_unknown_geometry():
    assert hasattr(model, 'free_stadium_id'), 'Safe stadium allocation missing'
    pack = {'stadium_count': 20, 'stadiums': [
        {'stadium_id': 8, 'stadium_profile': {'geometry': {'length_units': 1728},
                                          'future': [1, {'x': 2}]}},
        {'stadium_id': 10}]}
    original = copy.deepcopy(pack)
    assert model.free_stadium_id(pack) == 9
    clone = model.clone_stadium(pack['stadiums'][0], pack)
    assert clone['stadium_id'] == 9
    clone['stadium_profile']['future'][1]['x'] = 3
    assert pack == original


def test_full_catalog_fails_before_mutation():
    assert hasattr(model, 'free_stadium_id'), 'Safe stadium allocation missing'
    pack = {'stadium_count': 32, 'stadiums': [{'stadium_id': i} for i in range(8, 32)]}
    original = copy.deepcopy(pack)
    import pytest
    with pytest.raises(ValueError, match='32'):
        model.clone_stadium(pack['stadiums'][0], pack)
    assert pack == original
