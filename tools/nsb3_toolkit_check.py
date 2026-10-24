"""Own NSB-3 organic TERMINAL PROFILE/FETCH/physical DISPLAY TEXT completion."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb3_phonebook_check import wire
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import statuses
from tools.nsb3_toolkit_prepare import profile_metadata

DISPLAY = bytes.fromhex('d0148103012180820281028d0904') + b'DCT3 SAT'
RESPONSE = bytes.fromhex('810301218002028281030100')
INKEY = bytes.fromhex('d0138103022200820281828d0804') + b'Press 5'
INPUT = bytes.fromhex('d0188103032300820281828d0904') + b'Enter 42' + bytes.fromhex('91020102')
INKEY_RESPONSE = bytes.fromhex('8103022200020282810301000d020435')
INPUT_RESPONSE = bytes.fromhex('8103032300020282810301000d03043432')
MENU = bytes.fromhex('d0268103042500820281828509') + b'DCT3 menu' + bytes.fromhex('8f0901') + b'Continue' + bytes.fromhex('8f0502') + b'Exit'
MENU_RESPONSE = bytes.fromhex('810304250002028281030100')


def check_trace(text, interactive=False, menu_item=None):
    if menu_item not in (None, 1, 2):
        raise ValueError('unknown menu item')
    interactive = interactive or menu_item is not None
    check_pin(text, True)
    if 'nsb3_board_reset_request:' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('reset or failed observer')
    commands = wire(text)
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14, 0xc2)]
    expected = [(0x10, 0, 0, 5, bytes.fromhex('0b41977700')),
                (0x12, 0, 0, 22, b''), (0x14, 0, 0, 12, RESPONSE)]
    if interactive:
        expected += [(0x12, 0, 0, 21, b''), (0x14, 0, 0, 16, INKEY_RESPONSE),
                     (0x12, 0, 0, 26, b''), (0x14, 0, 0, 17, INPUT_RESPONSE)]
    if menu_item is not None:
        expected += [(0x12, 0, 0, 40, b''), (0x14, 0, 0, 12, MENU_RESPONSE),
                     (0xc2, 0, 0, 9, bytes.fromhex('d307020201811001') + bytes((menu_item,)))]
    if toolkit != expected:
        raise ValueError('own Toolkit APDU sequence or result differs')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32) for row in commands):
        raise ValueError('unrelated card mutation')
    results = [(0x9115, 3, 10), (0x911a, 3, 10), (0x9000, 3, 10)] if interactive else [(0x9000, 3, 10)]
    if menu_item is not None:
        results[-1:] = [(0x9128, 3, 10), (0x9000, 3, 10)]
    if statuses(text, 0xc2) != ([(0x9000, 3, 10)] if menu_item is not None else []):
        raise ValueError('menu envelope status differs')
    if statuses(text, 0x10) != [(0x9000, 3, 10)] or statuses(text, 0x14) != results:
        raise ValueError('Toolkit command completion status differs')
    if text.count('SIM completion ins=f2 sw=9116') != 1:
        raise ValueError('missing unique proactive-pending STATUS')
    rx = bytes.fromhex(''.join(re.findall(r'nsb3_sim_wire: direction=RX data=([0-9a-f]{2}) ', text)))
    if rx.count(DISPLAY + b'\x90\x00') != 1:
        raise ValueError('missing exact card-originated proactive payload')
    if interactive and any(rx.count(payload + b'\x90\x00') != 1 for payload in (INKEY, INPUT)):
        raise ValueError('missing exact card input command payload')
    if menu_item is not None and rx.count(MENU + b'\x90\x00') != 1:
        raise ValueError('missing exact card menu payload')
    contacts = re.findall(r'nsb3_toolkit_physical: action=dismiss down=([01]) t=([0-9.]+)', text)
    if contacts != [('1', '68.000000000'), ('0', '68.150000000')]:
        raise ValueError('physical dismissal differs')
    if text.index('SIM completion ins=f2 sw=9116') > text.index('nsb3_toolkit_physical: action=dismiss down=1'):
        raise ValueError('dismissal precedes pending command')
    before = text.split('nsb3_toolkit_physical: action=dismiss down=1', 1)[0]
    if any(row[0] == 0x14 for row in wire(before)):
        raise ValueError('terminal response preceded physical dismissal')
    actions = ('inkey_5', 'inkey_confirm', 'input_4', 'input_2', 'input_confirm') if interactive else ()
    observed = re.findall(r'nsb3_toolkit_input: action=(\w+) down=([01])', text)
    if observed != [(action, down) for action in actions for down in ('1', '0')]:
        raise ValueError('physical Toolkit input sequence differs')
    if interactive:
        for action, prior_responses in (('inkey_5', 1), ('input_confirm', 2)):
            prefix = text.split(f'nsb3_toolkit_input: action={action} down=1', 1)[0]
            if sum(row[0] == 0x14 for row in wire(prefix)) != prior_responses:
                raise ValueError('input result preceded physical entry/confirmation')
    menu_actions = ()
    if menu_item is not None:
        menu_actions = ('fieldtest', 'last', 'open') + (('next',) if menu_item == 2 else ()) + ('select',)
    observed_menu = re.findall(r'nsb3_toolkit_menu: action=(\w+) down=([01])', text)
    if observed_menu != [(action, down) for action in menu_actions for down in ('1', '0')]:
        raise ValueError('physical menu selection differs')
    if menu_item is not None:
        prefix = text.split('nsb3_toolkit_menu: action=select down=1', 1)[0]
        if any(row[0] == 0xc2 for row in wire(prefix)):
            raise ValueError('menu envelope preceded physical selection')


def check(source, run, interactive=False, menu_item=None):
    interactive = interactive or menu_item is not None
    check_link(source, run)
    metadata = json.loads((run / 'cold_source.json').read_text())
    if metadata.get('external_toolkit_card') != profile_metadata(4 if menu_item is not None else 3 if interactive else 1):
        raise ValueError('external Toolkit profile provenance differs')
    storage = run / 'nvram/nsb3r4t'
    if hashlib.sha256((storage / 'flash').read_bytes()).hexdigest() != IMAGE_SHA256:
        raise ValueError('own executable changed')
    if (run / 'initial_sim_card.bin').read_bytes() != make_current_profile(True) or (storage / 'sim_card').read_bytes() != make_current_profile(True):
        raise ValueError('complete card changed')
    check_trace((run / 'error.log').read_text(), interactive, menu_item)
    frame_check(run / 'snap/nsb3_toolkit_display.png', '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c')
    if not interactive:
        frame_check(run / 'snap/nsb3_toolkit_dismissed.png', 'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0')
    else:
        for name, expected in (
                ('inkey', 'e94ea9d879033d48822bb76a8f080f95e12456c3b897a1717be10fb4ddbbb77f'),
                ('input', '2b1546243a290d99ea42d5e70e173315b3d620903595782d2dc5f6fec483836c'),
                ('input_filled', '169b9f1bb144e9cd4e061a74980b1479956c9431c17482149b7701300b6e83b0'),
                ('interactive_final', 'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0')):
            frame_check(run / f'snap/nsb3_toolkit_{name}.png', expected)
    if menu_item is not None:
        frame_check(run / 'snap/nsb3_toolkit_menu_last.png', 'd85d1675d4cfecd00c35a6682014e8af53eddc6c6dd2a36683494d7cce1bc6a7')
        frame_check(run / 'snap/nsb3_toolkit_menu_open.png', 'ecf749905f760960630345d99b41bbbcdf11da3bd72c26dbf7ea6da71f6a7111')
        if menu_item == 2:
            frame_check(run / 'snap/nsb3_toolkit_menu_next.png', '7b83617cd69944ee0e73e5e29b42aefe1f291d5b45123e5aa1f36a29c11dd6b6')
        # Immediate Wait is recorded, not promoted to settled application UI.
        frame_check(run / 'snap/nsb3_toolkit_menu_select.png', 'a2cc199c75ea302146b2174ad6f1094aa807d391504e45650ddf8aa2bdea5ff7')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--interactive', action='store_true')
    mode.add_argument('--menu-item', type=int, choices=(1, 2))
    args = parser.parse_args()
    try:
        check(args.source, args.run, args.interactive, args.menu_item)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 Toolkit: FAIL: {exc}\n')
    boundary = 'menu installation/selection; final UI settlement unproved' if args.menu_item is not None else 'physical Toolkit display/input lifecycle'
    print(f'PASS: own {boundary}; native registration unproved')


if __name__ == '__main__':
    main()
