from tools.mod_studio.document import Document


def state():
    from tools.mod_studio import stadium
    return stadium.StadiumDraft(Document({'stadiums': [{'stadium_id': 8, 'name': 'HOME',
        'stadium_profile': {'version': 1, 'base_layout': 0, 'unknown': {'keep': 1},
         'geometry': {'length_units': 1728, 'width_units': 576, 'extra': 9}}}]}), 0)


def test_typed_and_dragged_geometry_share_one_history_model():
    draft = state()
    assert draft.set_text('length_units', '1664')
    assert draft.geometry['length_units'] == 1664
    draft.begin_drag()
    assert draft.drag_dimension('length_units', 1700) == 1696
    assert draft.drag_dimension('length_units', 1730) == 1728
    draft.end_drag()
    draft.document.undo()
    assert draft.geometry['length_units'] == 1664
    draft.document.undo()
    assert draft.geometry['length_units'] == 1728


def test_invalid_draft_disables_test_without_clamping_last_good_number():
    draft = state()
    for text in ('1664.5', '1664.0', '-1', '', '99999', '1665'):
        assert not draft.set_text('length_units', text)
        assert draft.geometry['length_units'] == 1728
        assert not draft.ready_for_test
        assert draft.diagnostics()[0].path[-1] == 'length_units'
    assert draft.set_text('length_units', '1664')
    assert draft.ready_for_test


def test_reset_preserves_identity_artwork_and_unknown_fields():
    draft = state()
    draft.profile['artwork'] = 'custom/stadium.json'
    draft.reset_geometry()
    assert draft.geometry['length_units'] == 1792
    assert draft.geometry['extra'] == 9
    assert draft.profile['unknown'] == {'keep': 1}
    assert draft.profile['artwork'] == 'custom/stadium.json'
    assert draft.stadium['stadium_id'] == 8 and draft.stadium['name'] == 'HOME'


def test_canvas_projection_is_invertible():
    from tools.mod_studio.stadium import canvas_to_world, world_to_canvas
    for point in ((0, 0), (1728, 576), (864, 288)):
        canvas = world_to_canvas(*point, scale=.2, origin=(80, 50))
        restored = canvas_to_world(*canvas, scale=.2, origin=(80, 50))
        assert all(abs(a-b) < .00001 for a, b in zip(point, restored))


def test_artwork_attachment_validates_before_document_mutation(tmp_path):
    import json
    import pytest
    from test_stadium_assets import manifest
    draft = state()
    draft.pack_dir = tmp_path
    with pytest.raises(ValueError):
        draft.attach_artwork('missing.json')
    assert 'artwork' not in draft.profile
    value = manifest(tmp_path)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    draft.attach_artwork('stadium.json')
    assert draft.profile['artwork'] == 'stadium.json'
    draft.document.undo()
    assert 'artwork' not in draft.profile
