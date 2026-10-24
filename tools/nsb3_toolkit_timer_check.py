"""Qualify an explicit NSB-3 TIMER MANAGEMENT rejection, not timer expiry."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_toolkit_check import check_storage, command_contract, check_actions
from tools.nsb3_toolkit_check import DISPLAY, INKEY, INPUT, MENU, TIMER
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import wire
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import statuses


def check_trace(text):
    check_pin(text, True)
    if 'nsb3_board_reset_request:' in text or 'Error in' in text or 'LUA ERROR' in text:
        raise ValueError('reset or failed observer')
    commands = wire(text)
    if [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14, 0xc2)] != command_contract(14):
        raise ValueError('requires exact timer request/rejection and no expiry envelope')
    if statuses(text, 0xc2) != [(0x9113, 3, 10)] or statuses(text, 0x10) != [(0x9000, 3, 10)]:
        raise ValueError('timer notification/profile status differs')
    if statuses(text, 0x14) != [(value, 3, 10) for value in (0x9115, 0x911a, 0x9128, 0x9000, 0x9000)]:
        raise ValueError('timer response status chain differs')
    if text.count('SIM completion ins=f2 sw=9116') != 1:
        raise ValueError('initial pending notification missing')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32) for row in commands):
        raise ValueError('unrelated card mutation')
    rx = bytes.fromhex(''.join(re.findall(r'nsb3_sim_wire: direction=RX data=([0-9a-f]{2}) ', text)))
    if any(rx.count(payload + b'\x90\x00') != 1 for payload in (DISPLAY, INKEY, INPUT, MENU, TIMER)):
        raise ValueError('fetched command payload differs')
    check_actions(text, 'nsb3_toolkit_physical', ('dismiss',))
    check_actions(text, 'nsb3_toolkit_input', ('inkey_5', 'inkey_confirm', 'input_4', 'input_2', 'input_confirm'))
    check_actions(text, 'nsb3_toolkit_menu', ('fieldtest', 'last', 'open', 'select'))
    for marker, instruction, count in (
            ('nsb3_toolkit_physical: action=dismiss down=1', 0x14, 0),
            ('nsb3_toolkit_input: action=inkey_5 down=1', 0x14, 1),
            ('nsb3_toolkit_input: action=input_confirm down=1', 0x14, 2),
            ('nsb3_toolkit_menu: action=select down=1', 0xc2, 0)):
        if text.count(marker) != 1 or sum(row[0] == instruction for row in wire(text.split(marker, 1)[0])) != count:
            raise ValueError('protocol result preceded physical action')


def check(source, run):
    check_storage(source, run, 14)
    check_trace((run / 'error.log').read_text())
    for name, expected in (
            ('display', '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c'),
            ('input_filled', '169b9f1bb144e9cd4e061a74980b1479956c9431c17482149b7701300b6e83b0'),
            ('menu_open', 'ecf749905f760960630345d99b41bbbcdf11da3bd72c26dbf7ea6da71f6a7111'),
            ('menu_select', 'a2cc199c75ea302146b2174ad6f1094aa807d391504e45650ddf8aa2bdea5ff7')):
        frame_check(run / f'snap/nsb3_toolkit_{name}.png', expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check(args.source, args.run)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 timer capability: FAIL: {exc}\n')
    print('PASS: TIMER MANAGEMENT explicitly rejected with result31; timer start/expiry unproved')


if __name__ == '__main__':
    main()
