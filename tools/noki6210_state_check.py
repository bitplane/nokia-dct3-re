"""Exact CPU/RAM/time restoration and NPE-3 protocol replay."""
import re
from tools.radio_state_roundtrip import verify_roundtrip


def verify(text, scenario, *, pin_enabled=False):
    if '[LUA ERROR]' in text or '6210_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    snapshots = re.findall(r'6210_state: scenario=(\w+) event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(snapshots) != 2 or [s[:2] for s in snapshots] != [(scenario, 'saved'), (scenario, 'restored')]:
        raise ValueError('missing exact save/load snapshots')
    if snapshots[0][2:] != snapshots[1][2:]:
        raise ValueError('CPU/RAM/time did not restore exactly')
    before_save = text.split('6210_state: scenario=' + scenario + ' event=saved', 1)[0]
    cursor = 0
    prerequisites = ('LAPDm Location Updating Accept acknowledged nr=1',
                     'LAPDm Channel Release acknowledged nr=2')
    if pin_enabled:
        prerequisites = ('6210_security_physical: action=confirm',
                         'SIM status ins=20 sw=9000') + prerequisites
    for event in prerequisites:
        position = before_save.find(event, cursor)
        if position < 0:
            raise ValueError('registration did not complete before save: ' + event)
        cursor = position + len(event)
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'),
                     '6210 ' + scenario)
