import copy
import importlib.util
from pathlib import Path

PATH = Path(__file__).resolve().parents[1] / 'tools/mod_studio/document.py'


def document(pack):
    assert PATH.exists(), 'Undoable document model missing'
    spec = importlib.util.spec_from_file_location('mod_document', PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.Document(pack)


def test_undo_restores_unknown_fields_selection_and_savepoint():
    pack = {'name': 'Original', 'stadiums': [{'unknown': {'values': [1, 2]}}]}
    doc = document(pack)
    doc.selection = 'stadium:0'
    doc.begin_edit()
    doc.pack['stadiums'][0]['unknown']['values'].append(3)
    doc.selection = 'pack'
    doc.commit_edit()
    assert doc.dirty
    assert doc.undo() and doc.selection == 'stadium:0'
    assert doc.pack['stadiums'][0]['unknown']['values'] == [1, 2]
    assert not doc.dirty
    assert doc.redo() and doc.pack['stadiums'][0]['unknown']['values'] == [1, 2, 3]
    assert doc.selection == 'pack'
    doc.mark_saved()
    assert not doc.dirty
    doc.undo()
    assert doc.dirty
    doc.redo()
    assert not doc.dirty


def test_drag_transaction_is_one_undo_and_cancel_is_clean():
    doc = document({'geometry': {'length': 1792}})
    doc.begin_edit()
    for length in (1760, 1728, 1696):
        doc.set_value(('geometry', 'length'), length)
    doc.commit_edit()
    assert doc.undo() and doc.pack['geometry']['length'] == 1792
    assert not doc.undo()
    doc.begin_edit()
    doc.set_value(('geometry', 'length'), 1664)
    doc.cancel_edit()
    assert doc.pack['geometry']['length'] == 1792 and not doc.dirty


def test_new_edit_discards_redo_without_aliasing_input_values():
    doc = document({'name': 'A'})
    doc.set_value(('name',), 'B')
    doc.undo()
    data = {'children': [1]}
    doc.set_value(('extra',), data)
    data['children'].append(2)
    assert doc.pack['extra'] == {'children': [1]}
    assert not doc.redo()


def test_noop_does_not_add_history():
    doc = document({'name': 'A'})
    doc.set_value(('name',), 'A')
    assert not doc.undo() and not doc.dirty


def test_external_mutations_can_be_checkpointed_and_grouped():
    pack = {'name': 'A'}
    doc = document(pack)
    for value in ('AB', 'ABC', 'ABCD'):
        pack['name'] = value
        doc.checkpoint(group='name')
    assert doc.undo() and pack['name'] == 'A'
    assert not doc.undo()
    assert doc.redo() and pack['name'] == 'ABCD'
