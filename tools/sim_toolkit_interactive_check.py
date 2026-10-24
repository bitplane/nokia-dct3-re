"""Wire-level lab-card DISPLAY TEXT / GET INKEY / GET INPUT contract.

Product wrappers own authentication, storage, configuration and reviewed pixels.
"""
import re
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE, wire
from tools.sim_fdn_protocol_check import apdus
from tools.sim_security_trace_check import statuses

INKEY = bytes.fromhex('d0138103022200820281828d0804') + b'Press 5'
INPUT = bytes.fromhex('d0188103032300820281828d0904') + b'Enter 42' + bytes.fromhex('91020102')
INKEY_RESPONSE = bytes.fromhex('8103022200020282810301000d020435')
INPUT_RESPONSE = bytes.fromhex('8103032300020282810301000d03043432')


def check_interactive(text, *, prefix, terminal_profile, reset_markers=()):
    if any(marker in text for marker in (*reset_markers,'LUA ERROR','Error in')):
        raise ValueError('reset or failed observer')
    if re.search(r'illegal instruction|unimplemented|unsupported|unhandled|fatal',text,re.I):
        raise ValueError('execution failed')
    tx,rx = wire(text,'TX',prefix),wire(text,'RX',prefix)
    commands = list(apdus(tx))
    if any(row[0] not in (0xa4,0x20,0xb0,0xb2,0xc0,0xf2,0x10,0x12,0x14) for row in commands):
        raise ValueError('unrelated SIM operation')
    expected = [(0x10,0,0,len(terminal_profile),terminal_profile)]
    for payload,response in ((DISPLAY,RESPONSE),(INKEY,INKEY_RESPONSE),(INPUT,INPUT_RESPONSE)):
        expected += [(0x12,0,0,len(payload),b''),(0x14,0,0,len(response),response)]
        if rx.count(payload+b'\x90\x00') != 1:
            raise ValueError('missing unique card command payload')
    if [row[:5] for row in commands if row[0] in (0x10,0x12,0x14)] != expected:
        raise ValueError('interactive profile/FETCH/response sequence differs')
    if 'read-binary fid=6fae offset=0 length=1 first=03' not in text or text.count('SIM completion ins=f2 sw=9116') != 1:
        raise ValueError('Phase-2+ pending command differs')
    if statuses(text,0x10) != [(0x9000,3,10)] or statuses(text,0x14) != [(0x9115,3,10),(0x911a,3,10),(0x9000,3,10)]:
        raise ValueError('interactive response status differs')
    marker = prefix+'_toolkit_physical: action=dismiss'
    if re.findall(re.escape(marker)+r' down=([01]) t=([0-9.]+)',text) != [('1','68.000000000'),('0','68.200000000')]:
        raise ValueError('physical display dismissal differs')
    actions = ('inkey_5','inkey_confirm','input_4','input_2','input_confirm')
    action_marker = prefix+'_toolkit_input:'
    if re.findall(re.escape(action_marker)+r' action=(\w+) down=([01])',text) != [(action,down) for action in actions for down in ('1','0')]:
        raise ValueError('physical input sequence differs')
    if text.count(action_marker) != 10 or text.count(prefix+'_toolkit_physical:') != 2:
        raise ValueError('extra or malformed physical actions')
    for contact,count,payload in (
            (marker+' down=1',0,DISPLAY),
            (action_marker+' action=inkey_5 down=1',1,INKEY),
            (action_marker+' action=inkey_confirm down=1',1,INKEY),
            (action_marker+' action=input_4 down=1',2,INPUT),
            (action_marker+' action=input_2 down=1',2,INPUT),
            (action_marker+' action=input_confirm down=1',2,INPUT)):
        before=text.split(contact,1)[0]
        if sum(row[0]==0x14 for row in apdus(wire(before,'TX',prefix))) != count:
            raise ValueError('response precedes physical input/confirmation')
        if payload+b'\x90\x00' not in wire(before,'RX',prefix):
            raise ValueError('physical input precedes card command')
