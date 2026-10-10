"""Summarize measured transfer dependencies without inventing scene boundaries.

The final payload marker describes one original transfer chunk, not a global
decompression-complete event. Replay can retain that live generation's upload.
This report intentionally uses a different schema from the legacy certificate.
"""
import hashlib
import json
from pathlib import Path


def classify(events, layout):
    def stable(event):
        return (event['generation'] > 0 and event['base_layout'] == layout
                and event['logical_id'] == layout
                and event['dma_queue_end'] == 0x3200
                and event['deferred_descriptors'] == 0
                and event['defer_mode'] == 0)
    live = next(e for e in events if e['kind'] == 'ppu' and e['mode'] == 8
                and e['base_layout'] == layout and e['logical_id'] == layout)
    generation = live['generation']
    previous = [e for e in events if e['generation'] == generation and e['tick'] < live['tick']]
    payload = next(e for e in reversed(previous) if e['kind'] == 'transfer_payload_ready')
    dma = next(e for e in previous if e['kind'] == 'dma_channel_return' and e['tick'] > payload['tick'])
    palette = next(e for e in reversed(previous) if e['kind'] == 'palette_return')
    constructor = next(e for e in reversed(previous) if e['kind'] == 'constructor_return')
    batch = next(e for e in reversed(previous) if e['kind'] == 'dma_batch_return' and stable(e))
    stream = next(e for e in previous if e['kind'] == 'streamer_entry' and e['tick'] > batch['tick'])
    assert palette['tick'] < dma['tick'] < batch['tick'] < stream['tick'] < live['tick']
    assert payload['tick'] < dma['tick'] and constructor['tick'] < live['tick']
    replay_constructor = next(e for e in events if e['kind'] == 'constructor_return'
                              and e['mode'] == 19 and e['generation'] == generation)
    replay = next(e for e in events if e['kind'] == 'ppu' and e['mode'] == 19
                  and e['submode'] == 6 and (e['score_home'] or e['score_away'])
                  and e['generation'] == generation
                  and e['tick'] > replay_constructor['tick'])
    replay_batch = next(e for e in reversed(events) if e['kind'] == 'dma_batch_return'
                        and replay_constructor['tick'] < e['tick'] < replay['tick'] and stable(e))
    replay_stream = next(e for e in events if e['kind'] == 'streamer_entry'
                         and replay_batch['tick'] < e['tick'] < replay['tick'])
    intervening = [e for e in events if live['tick'] < e['tick'] < replay['tick']]
    assert all(e['generation'] == generation for e in intervening)
    assert not any(e['kind'] == 'transfer_payload_ready' for e in intervening)
    selected = (palette, payload, dma, constructor, batch, stream, live,
                replay_constructor, replay_batch, replay_stream, replay)
    assert all(type(e['tick']) is int and e['tick'] >= 0
               and e['generation'] == generation and e['base_layout'] == layout
               and e['logical_id'] == layout for e in selected)
    assert live['tick'] < replay_constructor['tick'] < replay_batch['tick'] < replay_stream['tick'] < replay['tick']
    return {'base_layout': layout, 'generation': generation,
            'live': {'constructor': constructor, 'palette_return': palette,
                     'final_measured_chunk_ready': payload, 'chunk_dma_return': dma,
                     'queue_empty_batch': batch, 'streamer': stream, 'ppu_scanline_1': live},
            'replay': {'constructor': replay_constructor, 'streamer': replay_stream,
                       'queue_empty_batch': replay_batch, 'scored_ppu_scanline_1': replay,
                       'retains_live_upload_generation': generation}}


def report(directories, destination):
    rows = []
    hashes = {}
    for directory in map(Path, directories):
        path = directory / 'evidence.json'
        value = json.loads(path.read_text())
        assert value['engine_mode'] == 1
        hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        for layout in value['layouts']:
            assert layout['fresh_exhibition'] and not layout['guest_memory_modified']
            rows.append(classify(layout['raw_live_events'], layout['base_layout']))
    assert sorted(row['base_layout'] for row in rows) == list(range(8))
    result = {'schema': 'measured-stadium-transfer-dependencies-1',
              'scope': 'Original interpreter chunk delivery and retained replay generations; not a whole-ROM decompilation certificate.',
              'input_sha256': hashes, 'layouts': sorted(rows, key=lambda row: row['base_layout'])}
    Path(destination).write_text(json.dumps(result, indent=2))
    return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    arguments = parser.parse_args()
    print(f"Measured {len(report(arguments.directories, arguments.output)['layouts'])} original live/replay transfer routes")
