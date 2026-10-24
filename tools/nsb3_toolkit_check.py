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
from tools.sim_toolkit_interactive_check import INKEY, INPUT, INKEY_RESPONSE, INPUT_RESPONSE, MENU, MENU_RESPONSE, SELECT_ITEM, ITEM_RESPONSE

DISPLAY = bytes.fromhex('d0148103012180820281028d0904') + b'DCT3 SAT'
RESPONSE = bytes.fromhex('810301218002028281030100')
TIMER = bytes.fromhex('d01181030f270082028182a40101a503000005')
TIMER_REJECTION = bytes.fromhex('81030f270002028281030131')


def command_contract(profile, menu_item=1):
    """Observed lab-card command contracts, independent of device execution."""
    if profile not in (1, 3, 4, 7, 14):
        raise ValueError('unknown Toolkit command contract')
    if profile in (4, 7, 14) and menu_item not in (1, 2):
        raise ValueError('unknown Toolkit menu item')
    if profile in (7, 14) and menu_item != 1:
        raise ValueError('follow-up requires Continue')
    result = [(0x10, 0, 0, 5, bytes.fromhex('0b41977700')),
              (0x12, 0, 0, 22, b''), (0x14, 0, 0, 12, RESPONSE)]
    if profile != 1:
        result += [(0x12, 0, 0, 21, b''), (0x14, 0, 0, 16, INKEY_RESPONSE),
                   (0x12, 0, 0, 26, b''), (0x14, 0, 0, 17, INPUT_RESPONSE)]
    if profile in (4, 7, 14):
        result += [(0x12, 0, 0, 40, b''), (0x14, 0, 0, 12, MENU_RESPONSE),
                   (0xc2, 0, 0, 9, bytes.fromhex('d307020201811001') + bytes((menu_item,)))]
    if profile == 7:
        result += [(0x12, 0, 0, 34, b''), (0x14, 0, 0, 15, ITEM_RESPONSE)]
    elif profile == 14:
        result += [(0x12, 0, 0, 19, b''), (0x14, 0, 0, 12, TIMER_REJECTION)]
    return result


def check_actions(text, marker, actions):
    rows = re.findall(re.escape(marker) + r': action=(\w+) down=([01])', text)
    if rows != [(action, down) for action in actions for down in ('1', '0')]:
        raise ValueError('physical Toolkit actions differ: ' + marker)


def check_trace(text, interactive=False, menu_item=None, return_wait=False, select_item=False):
    if select_item:
        if menu_item not in (None, 1) or return_wait:
            raise ValueError('SELECT ITEM requires its own Continue control')
        menu_item = 1
    if return_wait and menu_item != 1:
        raise ValueError('Wait control requires Continue selection')
    if menu_item not in (None, 1, 2):
        raise ValueError('unknown menu item')
    interactive = interactive or menu_item is not None
    check_pin(text, True)
    if 'nsb3_board_reset_request:' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('reset or failed observer')
    commands = wire(text)
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14, 0xc2)]
    profile = 7 if select_item else 4 if menu_item is not None else 3 if interactive else 1
    expected = command_contract(profile, menu_item)
    if toolkit != expected:
        raise ValueError('own Toolkit APDU sequence or result differs')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32) for row in commands):
        raise ValueError('unrelated card mutation')
    results = [(0x9115, 3, 10), (0x911a, 3, 10), (0x9000, 3, 10)] if interactive else [(0x9000, 3, 10)]
    if menu_item is not None:
        results[-1:] = [(0x9128, 3, 10), (0x9000, 3, 10)]
    if select_item:
        results.append((0x9000, 3, 10))
    envelope_status = 0x9122 if select_item else 0x9000
    if statuses(text, 0xc2) != ([(envelope_status, 3, 10)] if menu_item is not None else []):
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
    if select_item and rx.count(SELECT_ITEM + b'\x90\x00') != 1:
        raise ValueError('missing exact follow-up SELECT ITEM payload')
    contacts = re.findall(r'nsb3_toolkit_physical: action=dismiss down=([01]) t=([0-9.]+)', text)
    if contacts != [('1', '68.000000000'), ('0', '68.150000000')]:
        raise ValueError('physical dismissal differs')
    if text.index('SIM completion ins=f2 sw=9116') > text.index('nsb3_toolkit_physical: action=dismiss down=1'):
        raise ValueError('dismissal precedes pending command')
    before = text.split('nsb3_toolkit_physical: action=dismiss down=1', 1)[0]
    if any(row[0] == 0x14 for row in wire(before)):
        raise ValueError('terminal response preceded physical dismissal')
    actions = ('inkey_5', 'inkey_confirm', 'input_4', 'input_2', 'input_confirm') if interactive else ()
    check_actions(text, 'nsb3_toolkit_input', actions)
    if interactive:
        for action, prior_responses in (('inkey_5', 1), ('input_confirm', 2)):
            prefix = text.split(f'nsb3_toolkit_input: action={action} down=1', 1)[0]
            if sum(row[0] == 0x14 for row in wire(prefix)) != prior_responses:
                raise ValueError('input result preceded physical entry/confirmation')
    menu_actions = ()
    if menu_item is not None:
        menu_actions = ('fieldtest', 'last', 'open') + (('next',) if menu_item == 2 else ()) + ('select',)
        if return_wait:
            menu_actions += ('end', 'return')
    check_actions(text, 'nsb3_toolkit_menu', menu_actions)
    if menu_item is not None:
        prefix = text.split('nsb3_toolkit_menu: action=select down=1', 1)[0]
        if any(row[0] == 0xc2 for row in wire(prefix)):
            raise ValueError('menu envelope preceded physical selection')
    check_actions(text, 'nsb3_toolkit_select_item', ('next', 'confirm') if select_item else ())
    if select_item:
        prefix = text.split('nsb3_toolkit_select_item: action=confirm down=1', 1)[0]
        if sum(row[0] == 0x14 for row in wire(prefix)) != 4:
            raise ValueError('item response preceded physical confirmation')


def check_storage(source, run, profile):
    """Common own-storage and explicit external-card provenance contract."""
    check_link(source, run)
    metadata = json.loads((run / 'cold_source.json').read_text())
    if metadata.get('external_toolkit_card') != profile_metadata(profile):
        raise ValueError('external Toolkit profile provenance differs')
    storage = run / 'nvram/nsb3r4t'
    if hashlib.sha256((storage / 'flash').read_bytes()).hexdigest() != IMAGE_SHA256:
        raise ValueError('own executable changed')
    if (run / 'initial_sim_card.bin').read_bytes() != make_current_profile(True) or (storage / 'sim_card').read_bytes() != make_current_profile(True):
        raise ValueError('complete card changed')


def check(source, run, interactive=False, menu_item=None, return_wait=False, select_item=False):
    if select_item:
        if menu_item not in (None, 1) or return_wait:
            raise ValueError('SELECT ITEM requires its own Continue control')
        menu_item = 1
    interactive = interactive or menu_item is not None
    profile = 7 if select_item else 4 if menu_item is not None else 3 if interactive else 1
    check_storage(source, run, profile)
    check_trace((run / 'error.log').read_text(), interactive, menu_item, return_wait, select_item)
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
        if not select_item:
            # Immediate Wait is recorded, not promoted to settled application UI.
            frame_check(run / 'snap/nsb3_toolkit_menu_select.png', 'a2cc199c75ea302146b2174ad6f1094aa807d391504e45650ddf8aa2bdea5ff7')
    if select_item:
        for name, expected in (
                ('prompt', 'd52d2e45c89c1929d44f4f9c35f1ec025a8d2fbd79a3e57ee2a623eeb1c147f3'),
                ('selected', '8c8be627546fe82db2276ebddf9f3fe1dd358e1d267b2a2a56e66fef4944c42b'),
                ('result', 'ecf749905f760960630345d99b41bbbcdf11da3bd72c26dbf7ea6da71f6a7111')):
            frame_check(run / f'snap/nsb3_toolkit_item_{name}.png', expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--interactive', action='store_true')
    mode.add_argument('--menu-item', type=int, choices=(1, 2))
    mode.add_argument('--select-item', action='store_true')
    args = parser.parse_args()
    try:
        check(args.source, args.run, args.interactive, args.menu_item, select_item=args.select_item)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 Toolkit: FAIL: {exc}\n')
    if args.select_item:
        boundary = 'physical SELECT ITEM and clean return'
    elif args.menu_item is not None:
        boundary = 'menu installation/selection; final UI settlement unproved'
    else:
        boundary = 'physical Toolkit display/input lifecycle'
    print(f'PASS: own {boundary}; native registration unproved')


if __name__ == '__main__':
    main()
