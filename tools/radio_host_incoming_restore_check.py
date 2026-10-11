"""Check NSM-5 authenticated incoming host restoration; no speech claim."""
import re
import json
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, *, stale_kind='termination'):
    if stale_kind not in ('termination', 'media'):
        raise ValueError('unsupported restoration probe')
    patterns = [
        ('PIN accepted', r'SIM status ins=20 sw=9000'),
        ('registered', r'gsm_call_adapter: network registered=1 arfcn=86'),
        ('host paging', r'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging'),
        ('physical Answer', r'5210_pin_physical: key=Send(?:\s|$)'),
        ('connected', r'gsm_call_adapter: incoming state id=1 epoch=1 phase=connected'),
        ('saved', r'5210_pin_state: event=saved'),
        ('restored', r'5210_pin_state: event=restored'),
        ('restored connected', r'gsm_call_adapter: incoming state id=1 epoch=2 phase=connected'),
        ('stale input rejected', rf'gsm_call_adapter: stale epoch=1 current=2 type={stale_kind} id=1 result=rejected'),
        ('physical End', r'5210_pin_state_physical: key=End(?:\s|$)'),
        ('Disconnect', r'GSM service uplink sapi=0 pd=03 message=25'),
        ('Release', r'GSM service downlink kind=25 sapi=0 pd=03 message=2d'),
        ('Release Complete', r'GSM service uplink sapi=0 pd=03 message=2a'),
        ('restored ended', r'gsm_call_adapter: incoming state id=1 epoch=2 phase=ended'),
    ]
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in patterns),
                    '5210 host incoming restoration')
    if len(re.findall(r'gsm_call_adapter: incoming id=1 result=accepted\b', text)) != 1:
        raise ValueError('incoming host call must be queued exactly once')
    if len(re.findall(r'gsm_call_adapter: incoming state id=1 epoch=\d+ phase=paging\b', text)) != 1:
        raise ValueError('restoration replayed incoming paging')


def verify_media(text, snapshot, frames=80):
    verify(text, stale_kind='media')
    state = json.loads(snapshot)
    expected = state.get('media_downlink_sequence')
    if (state.get('type') != 'incoming_call_state' or state.get('epoch') != 2 or
            state.get('request_id') != 1 or state.get('phase') != 'connected' or
            type(expected) is not int or expected < 0):
        raise ValueError('restored host cursor observation absent or wrong identity')
    start = text.index('gsm_call_adapter: incoming state id=1 epoch=2 phase=connected')
    end = text.index('5210_pin_state_physical: key=End', start)
    accepted = [int(sequence) for sequence in re.findall(
        r'gsm_call_adapter: media direction=downlink id=1 sequence=(\d+) result=accepted\b',
        text[start:end])]
    if len(accepted) < frames or accepted[:frames] != list(range(expected, expected + frames)):
        raise ValueError('restored incoming media quota/cursor continuity failed')
