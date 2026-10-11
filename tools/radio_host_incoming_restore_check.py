"""Check NSM-5 authenticated incoming host restoration; no speech claim."""
import re
from tools.radio_call_lifecycle_common import require_ordered


def verify(text):
    patterns = [
        ('PIN accepted', r'SIM status ins=20 sw=9000'),
        ('registered', r'gsm_call_adapter: network registered=1 arfcn=86'),
        ('host paging', r'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging'),
        ('physical Answer', r'5210_pin_physical: key=Send(?:\s|$)'),
        ('connected', r'gsm_call_adapter: incoming state id=1 epoch=1 phase=connected'),
        ('saved', r'5210_pin_state: event=saved'),
        ('restored', r'5210_pin_state: event=restored'),
        ('restored connected', r'gsm_call_adapter: incoming state id=1 epoch=2 phase=connected'),
        ('stale termination rejected', r'gsm_call_adapter: stale epoch=1 current=2 type=termination id=1 result=rejected'),
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
