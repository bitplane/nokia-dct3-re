"""Shared accepted DISPLAY TEXT ordering; product fixtures own their frames."""
from tools.sim_toolkit_trace_check import require_in_order


def display_text_events(product, profile_length=9):
    return (
        'read-binary fid=6fae offset=0 length=1 first=03',
        f'header cla=a0 ins=10 p1=00 p2=00 p3={profile_length:02x}',
        'SIM status ins=10 sw=9000',
        'proactive DISPLAY TEXT ready',
        'SIM completion ins=f2 sw=9116',
        'header cla=a0 ins=12 p1=00 p2=00 p3=16',
        f'{product}_toolkit_physical: action=dismiss',
        'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
        'terminal-response data=810301218002028281030100',
        'SIM status ins=14 sw=9000',
    )


def verify_display_text(text, product, profile_length=9):
    if '[LUA ERROR]' in text:
        raise ValueError(f'{product} physical Toolkit fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''),
                     display_text_events(product, profile_length))
