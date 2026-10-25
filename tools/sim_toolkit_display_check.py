"""Shared wire contract for an external one-command DISPLAY TEXT card.

Callers supply their independently observed terminal profile, marker namespace
and physical contact interval. Success must follow physical dismissal;
explicit capability refusal must complete before that contact. Handset storage
and pixels stay product-owned.
"""
import re

from tools.sim_fdn_protocol_check import apdus
from tools.sim_security_trace_check import statuses

DISPLAY = bytes.fromhex('d0148103012180820281028d0904') + b'DCT3 SAT'
RESPONSE = bytes.fromhex('810301218002028281030100')
REFUSAL = bytes.fromhex('810301218002028281030130')


def check_observer_console(text):
    # MAME can report a failed Lua coroutine here and still exit successfully.
    if 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('failed Lua observer in MAME console')


def wire(text, direction, prefix, *, limit=8192):
    marker = re.escape(prefix) + r'_sim_wire: direction=' + re.escape(direction)
    rows = re.findall(marker + r' data=([0-9a-f]{2}) ', text)
    if not rows or len(rows) >= limit or len(rows) != text.count(
            f'{prefix}_sim_wire: direction={direction} '):
        raise ValueError('missing, malformed or capped SIM wire observation')
    return bytes.fromhex(''.join(rows))


def check_display(text, *, prefix, terminal_profile, control=False,
                  dismissal=('68.000000000', '68.200000000'), reset_markers=(),
                  capability_refusal=False):
    tx, rx = wire(text, 'TX', prefix), wire(text, 'RX', prefix)
    commands = list(apdus(tx))
    if any(row[0] not in (0xa4, 0x20, 0xb0, 0xb2, 0xc0, 0xf2, 0x10, 0x12, 0x14)
           for row in commands):
        raise ValueError('unrelated SIM operation')
    action = prefix + '_toolkit_physical: action=dismiss'
    contacts = re.findall(re.escape(action) + r' down=([01]) t=([0-9.]+)', text)
    if (contacts != [('1', dismissal[0]), ('0', dismissal[1])]
            or text.count(prefix + '_toolkit_physical:') != 2):
        raise ValueError('physical dismissal differs')
    if any(marker in text for marker in (*reset_markers, 'LUA ERROR', 'Error in')):
        raise ValueError('reset or failed observer')
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14)]
    response = REFUSAL if capability_refusal else RESPONSE
    expected = [] if control else [
        (0x10, 0, 0, len(terminal_profile), terminal_profile),
        (0x12, 0, 0, len(DISPLAY), b''),
        (0x14, 0, 0, len(response), response),
    ]
    if toolkit != expected:
        raise ValueError('own terminal profile, FETCH or terminal response differs')
    phase = '02' if control else '03'
    if f'read-binary fid=6fae offset=0 length=1 first={phase}' not in text:
        raise ValueError('card phase was not read')
    for instruction in (0x10, 0x14):
        if statuses(text, instruction) != ([] if control else [(0x9000, 3, 10)]):
            raise ValueError('Toolkit completion status differs')
    if control:
        if 'SIM completion ins=f2 sw=91' in text or DISPLAY in rx:
            raise ValueError('Phase-2 control produced proactive traffic')
    else:
        if text.count('SIM completion ins=f2 sw=9116') != 1 or rx.count(DISPLAY + b'\x90\x00') != 1:
            raise ValueError('missing unique pending STATUS or card-originated payload')
        before = text.split(action + ' down=1', 1)[0]
        if DISPLAY + b'\x90\x00' not in wire(before, 'RX', prefix):
            raise ValueError('dismissal precedes proactive payload')
        early_responses = [row[:5] for row in apdus(wire(before, 'TX', prefix)) if row[0] == 0x14]
        if capability_refusal:
            if (early_responses != [expected[-1]] or
                    statuses(before, 0x14) != [(0x9000, 3, 10)]):
                raise ValueError('automatic capability refusal did not complete before physical input')
        elif early_responses:
            raise ValueError('terminal response precedes physical dismissal')
