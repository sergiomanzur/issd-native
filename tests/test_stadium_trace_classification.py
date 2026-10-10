import copy
import pytest
from tools.ghidra.classify_stadium_trace import classify


def events():
    sequence = [('palette_return',4),('transfer_payload_ready',6),('dma_channel_return',6),
                ('constructor_return',6),('dma_batch_return',8),('streamer_entry',8),('ppu',8),
                ('constructor_return',19),('dma_batch_return',19),('streamer_entry',19),('ppu',19)]
    return [dict(kind=kind,tick=index+1,generation=2,base_layout=0,logical_id=0,
                 dma_queue_end=0x3200,deferred_descriptors=0,defer_mode=0,
                 mode=mode,submode=6 if mode==19 else 0,score_home=0,
                 score_away=1 if mode==19 else 0)
            for index,(kind,mode) in enumerate(sequence)]


def test_classification_retains_raw_boundaries_and_replay_generation():
    raw=events(); original=copy.deepcopy(raw)
    result=classify(raw,0)
    assert raw==original
    assert result['live']['final_measured_chunk_ready']['kind']=='transfer_payload_ready'
    assert result['replay']['retains_live_upload_generation']==2


@pytest.mark.parametrize('failure',['payload','dma','queue','generation','score','stream_layout','batch_order','fractional_tick'])
def test_classification_rejects_unproven_routes(failure):
    raw=events()
    if failure=='payload':raw=[e for e in raw if e['kind']!='transfer_payload_ready']
    if failure=='dma':raw=[e for e in raw if e['kind']!='dma_channel_return']
    if failure=='queue':
        for e in raw:
            if e['kind']=='dma_batch_return':e['dma_queue_end']+=20
    if failure=='generation':raw[-1]['generation']=3
    if failure=='score':raw[-1]['score_away']=0
    if failure=='stream_layout':
        for e in raw:
            if e['kind']=='streamer_entry':e['base_layout']=e['logical_id']=7
    if failure=='batch_order':raw[4]['tick']=2;raw.sort(key=lambda e:e['tick'])
    if failure=='fractional_tick':raw[4]['tick']=4.5
    with pytest.raises((StopIteration,AssertionError)):
        classify(raw,0)
