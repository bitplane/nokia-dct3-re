"""Own NPE-3 proactive DISPLAY TEXT protocol and physical clearance."""
from tools.sim_toolkit_trace_check import require_in_order


def verify(text):
    if '[LUA ERROR]' in text:
        raise ValueError('physical Toolkit fixture failed')
    compact = text.replace('[:sim_card] ', '')
    require_in_order(compact, [
        'read-binary fid=6fae offset=0 length=1 first=03',
        'header cla=a0 ins=10 p1=00 p2=00 p3=09',
        'SIM status ins=10 sw=9000',
        'proactive DISPLAY TEXT ready',
        'SIM completion ins=f2 sw=9116',
        'header cla=a0 ins=12 p1=00 p2=00 p3=16',
        '6210_toolkit_physical: action=dismiss',
        'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
        'terminal-response data=810301218002028281030100',
        'SIM status ins=14 sw=9000',
    ])
