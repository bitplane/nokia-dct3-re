"""Verify correlated host connect and handset-owned clearing; no speech claim."""
import re
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, number):
    patterns = (
        ('own host request', rf'gsm_call_adapter: request id=1 epoch=1 digits={re.escape(number)} clients=1\b'),
        ('wrong request rejected', r'gsm_call_adapter: decision id=2 outcome=1 result=rejected'),
        ('connect accepted', r'gsm_call_adapter: decision id=1 outcome=0 result=accepted'),
        ('duplicate rejected', r'gsm_call_adapter: decision id=1 outcome=0 result=rejected'),
        ('connected', r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
        ('ended', r'gsm_call_adapter: state id=1 epoch=1 phase=ended'),
    )
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in patterns), 'host outgoing connect')
    if text.count('gsm_call_adapter: decision id=1 outcome=0 result=accepted') != 1:
        raise ValueError('expected exactly one accepted connect decision')
    if text.count('outgoing decision queued id=1 outcome=0') != 1:
        raise ValueError('expected one queued connect decision')
