"""Exact architectural and SIM replay checks for open Toolkit menus."""
import re
from tools.radio_state_roundtrip import verify_roundtrip
from tools.sim_toolkit_trace_check import require_in_order


def verify_state(text, product, actions):
    if '[LUA ERROR]' in text or f'{product}_menu_state: FAIL' in text:
        raise ValueError('menu restoration fixture incomplete')
    states = re.findall(rf'{re.escape(product)}_menu_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [s[0] for s in states] != ['saved', 'restored'] or states[0][1:] != states[1][1:]:
        raise ValueError('menu architectural state did not restore exactly')
    verify_roundtrip(text, ('sim_device:',), f'{product} Toolkit menu')
    compact = text.replace('[:sim_card] ', '')
    for phase in ('reference', 'restored'):
        begin_marker = f'state_replay: phase={phase} event=begin'
        end_marker = f'state_replay: phase={phase} event=end'
        if compact.count(begin_marker) != 1 or compact.count(end_marker) != 1:
            raise ValueError('missing or duplicate menu replay interval')
        begin = compact.index(begin_marker)
        end = compact.index(end_marker, begin)
        require_in_order(compact[begin:end], (
            f'{product}_{actions}: action=menu_select phase={phase}',
            'header cla=a0 ins=c2 p1=00 p2=00 p3=09',
            'envelope data=d30702020181100101',
            'menu selection item=1 accepted', 'SIM status ins=c2 sw=9000',
        ))
