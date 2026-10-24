"""One organically fetched DISPLAY TEXT, completed by two save/load branches."""
import re
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE, wire
from tools.sim_fdn_protocol_check import apdus
from tools.sim_security_trace_check import statuses


def check_replay(text, *, prefix, terminal_profile, reset_markers=()):
    if any(marker in text for marker in (*reset_markers,'LUA ERROR','Error in')):
        raise ValueError('reset or failed observer')
    commands=list(apdus(wire(text,'TX',prefix)))
    if any(row[0] not in (0xa4,0x20,0xb0,0xb2,0xc0,0xf2,0x10,0x12,0x14) for row in commands):
        raise ValueError('unrelated SIM operation')
    response=(0x14,0,0,len(RESPONSE),RESPONSE)
    expected=[(0x10,0,0,len(terminal_profile),terminal_profile),
              (0x12,0,0,len(DISPLAY),b''),response,response]
    if [row[:5] for row in commands if row[0] in (0x10,0x12,0x14)] != expected:
        raise ValueError('requires one FETCH and two exact replayed responses')
    if statuses(text,0x10) != [(0x9000,3,10)] or statuses(text,0x14) != [(0x9000,3,10)]*2:
        raise ValueError('pending card command did not complete twice')
    if (text.count('SIM completion ins=f2 sw=9116') != 1
            or wire(text,'RX',prefix).count(DISPLAY+b'\x90\x00') != 1):
        raise ValueError('unique pending command/payload missing or refetched')
    for phase,count in (('reference',0),('restored',1)):
        marker=f'{prefix}_state_contact: phase={phase} down=1'
        if text.count(marker) != 1:
            raise ValueError('missing unique physical replay contact')
        before=text.split(marker,1)[0]
        if sum(row[0]==0x14 for row in apdus(wire(before,'TX',prefix))) != count:
            raise ValueError('response precedes physical replay contact')
        if DISPLAY+b'\x90\x00' not in wire(before,'RX',prefix):
            raise ValueError('physical contact precedes card payload')
