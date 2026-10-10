"""A certificate must contain measured construction evidence, not a success flag."""
import copy
import importlib.util
from pathlib import Path

PATH = Path(__file__).resolve().parents[1] / 'tools/ghidra/probe_stadium_contract.py'


def validator():
    assert PATH.exists(), 'Stadium evidence validator has not been implemented'
    spec = importlib.util.spec_from_file_location('stadium_contract_probe', PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.validate_evidence


def certificate():
    return {
        'version': 1, 'rom_sha256': 'a' * 64,
        'source_hashes': {'ISSDNative/main.c': 'b' * 64},
        'complete': True,
        'layouts': [dict(
            base_layout=i, fresh_exhibition=True, guest_memory_modified=False,
            routes={route: [
                {'kind': kind, 'tick': tick, 'generation': 1}
                for tick, kind in enumerate((
                    'constructor', 'palette_complete', 'decompression_complete',
                    'dma_complete', 'streamer', 'ppu'))]
                for route in ('live', 'replay')},
            scenery_envelope={'verified': True, 'pitch_clear': True,
                              'nets_clear': True, 'consumer_audit_complete': True},
        ) for i in range(8)],
    }


def test_complete_measured_certificate_accepted():
    assert validator()(certificate())


def test_claimed_complete_does_not_replace_missing_layout():
    value = certificate()
    value['layouts'].pop()
    assert not validator()(value)


def test_replay_route_is_required():
    value = certificate()
    del value['layouts'][0]['routes']['replay']
    assert not validator()(value)


def test_streamer_cannot_precede_dma_completion():
    value = certificate()
    events = value['layouts'][0]['routes']['live']
    events[3]['tick'], events[4]['tick'] = events[4]['tick'], events[3]['tick']
    assert not validator()(value)


def test_uploads_may_complete_before_pitch_constructor():
    value = certificate()
    events = value['layouts'][0]['routes']['live']
    # Native exhibition setup streams backgrounds in mode 4, then constructs
    # pitch physics in mode 6. The first gameplay PPU must follow both.
    events[0]['tick'] = 9
    events[5]['tick'] = 10
    assert validator()(value)


def test_palette_and_decompression_can_finish_in_either_order():
    value = certificate()
    events = value['layouts'][0]['routes']['live']
    events[1]['tick'], events[2]['tick'] = events[2]['tick'], events[1]['tick']
    assert validator()(value)


def test_constructor_and_uploads_must_finish_before_presentation():
    for index in (0, 1, 2):
        value = certificate()
        value['layouts'][0]['routes']['live'][index]['tick'] = 6
        assert not validator()(value)


def test_unverified_scenery_and_modified_guest_are_rejected():
    for key in ('verified', 'pitch_clear', 'nets_clear', 'consumer_audit_complete'):
        value = certificate()
        value['layouts'][0]['scenery_envelope'][key] = False
        assert not validator()(value)
    value = certificate()
    value['layouts'][0]['guest_memory_modified'] = True
    assert not validator()(value)


def test_mixed_transfer_generations_are_rejected():
    value = certificate()
    value['layouts'][0]['routes']['replay'][3]['generation'] = 2
    assert not validator()(value)


def test_wrong_types_and_duplicate_layouts_are_rejected():
    value = certificate()
    value['layouts'][0]['base_layout'] = False
    assert not validator()(value)
    value = certificate()
    value['layouts'][1] = copy.deepcopy(value['layouts'][0])
    assert not validator()(value)


def test_probe_rejects_wrong_selected_native_state():
    spec = importlib.util.spec_from_file_location('stadium_probe_state', PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    ram = bytearray(0x20000)
    for address, value in ((0x70, 8), (0x86, 0), (0x1fa2, 0),
                           (0x12a2, 1792), (0x12a4, 576)):
        ram[address:address+2] = value.to_bytes(2, 'little')
    assert module.validate_original_state(ram, 0)
    for address in (0x70, 0x86, 0x1fa2, 0x12a2, 0x12a4):
        changed = bytearray(ram)
        changed[address] ^= 1
        assert not module.validate_original_state(changed, 0)
    assert not module.validate_original_state(ram[:100], 0)
