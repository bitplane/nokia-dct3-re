"""Own NSB-3 local FDN dialing contrast; not registration or connected calls."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_fdn_edit_check import expected_card
from tools.nsb3_fdn_check import check_trace as check_settings
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME
from tools.sim_fdn_protocol_check import check_protocol_phase

DENIED = 'f77c80d0e7545bd4d1c233f7d3564f9bbed1704ed5ee0362f25fb9b724294f3d'
# Retained-pixel result: a comparison oracle, never call-progress acceptance.
NON_REJECTED = 'd9ccba3661671ca558488467b4966deb3f80cc1d24a0889e5e6df58913460d7e'


def card_expected(enabled):
    card = bytearray(expected_card())
    card[3575] = 0 if enabled else 1
    return bytes(card)


def check_dial(text, digits, enabled):
    if digits not in ('12346', '54321') or 'nsb3_board_reset_request:' in text:
        raise ValueError('invalid dialing fixture or reset')
    check_pin(text, True)
    keys = ('Right Softkey / C',) + tuple('Keypad ' + d for d in digits) + ('Send',)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=([0-9.]+) physical_only=1', text)
    if [(key, down) for key, down, _ in contacts] != [(key, down) for key in keys for down in ('1', '0')]:
        raise ValueError('physical dialing sequence differs')
    times = [40, 42, 43, 44, 45, 46, 48]
    for index, (_, _, time) in enumerate(contacts):
        if abs(float(time) - (times[index // 2] + 0.15 * (index % 2))) > 0.001:
            raise ValueError('physical dialing cadence differs')
    raw = bytes.fromhex(''.join(re.findall(r'nsb3_sim_wire: direction=TX data=([0-9a-f]{2}) ', text)))
    check_protocol_phase(text, 1 if enabled else 3, raw)
    commands = wire(text)
    if any(row[0] in (0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0xc2) for row in commands):
        raise ValueError('unexpected card mutation')
    if enabled and digits == '12346':
        before_send = text.split('nsb3_security_key: key=Send down=1 ', 1)[0]
        # Decode the complete stream so a prior SELECT remains in scope.
        after_send = commands[len(wire(before_send)):]
        if (0xb2, 1, 4, 32, b'', 0x6f3b) not in after_send:
            raise ValueError('listed number lacks post-Send FDN lookup')


def check_chain(source, listed, unlisted, disabled, off):
    for run, parent, enabled in ((source, None, True), (listed, source, True),
                                 (unlisted, source, True), (disabled, source, False),
                                 (off, disabled, False)):
        storage = run / 'nvram/nsb3r4t'
        if hashlib.sha256((storage / 'flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if (storage / 'sim_card').read_bytes() != card_expected(enabled):
            raise ValueError('complete FDN card differs')
        if parent is not None:
            check_link(parent, run)
            text = (run / 'error.log').read_text()
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
            if run == disabled:
                check_settings(text, 2)
                frame_check(run / 'snap/nsb3_security_17.png',
                            '91dae780554d524047135c635fb02f28ca1dc26cd40acf834e2fcb8f678fe6eb')
            else:
                check_dial(text, '12346' if run == listed else '54321', enabled)
                frame_check(run / 'snap/nsb3_security_07.png', DENIED if run == unlisted else NON_REJECTED)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', nargs=5, type=Path, metavar='RUN')
    args = parser.parse_args()
    try:
        check_chain(*args.runs)
    except (OSError, ValueError, KeyError, IndexError) as exc:
        parser.exit(1, f'NSB-3 local FDN contrast: FAIL: {exc}\n')
    print('PASS: own local FDN contrast; native registration/calls remain unqualified')


if __name__ == '__main__':
    main()
