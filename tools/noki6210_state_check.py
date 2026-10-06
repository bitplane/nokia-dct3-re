"""Exact CPU/RAM/time restoration and NPE-3 protocol replay."""
import re
from tools.radio_state_roundtrip import verify_roundtrip


def verify(text, scenario):
    if '[LUA ERROR]' in text or '6210_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    snapshots = re.findall(r'6210_state: scenario=(\w+) event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(snapshots) != 2 or [s[:2] for s in snapshots] != [(scenario, 'saved'), (scenario, 'restored')]:
        raise ValueError('missing exact save/load snapshots')
    if snapshots[0][2:] != snapshots[1][2:]:
        raise ValueError('CPU/RAM/time did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'),
                     '6210 ' + scenario)
