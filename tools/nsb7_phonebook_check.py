"""Qualify own NSB-7 physical SIM contact save and independent cold retrieval."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_security_check import check_trace as check_security
from tools.nsb7_storage_check import inspect
from tools.nsk3_phonebook_check import transmitted, expected_record, check_card
from tools.sim_fdn_protocol_check import validity_targets
from tools.nsk1_phonebook_roundtrip import frame_check

SAVE_KEYS = (0x1a, 0x18, 0x19, 2, 0x19, 1, 2, 3, 4, 5, 0x19)
READ_KEYS = (0x1a, 0x19, 0x19, 0x19)
HOST_KEYS = {0x1a: 'Right Softkey / C', 0x18: 'Scroll Down', 0x19: 'Left Softkey / Menu',
             **{i: 'Keypad ' + str(i) for i in range(1, 6)}}


def check_pin_card(initial, saved):
    from tools.nsb7_pin_services_check import check_source
    check_source(initial)
    expected = bytearray(initial)
    expected[:32] = expected_record()
    if saved != bytes(expected):
        raise ValueError('PIN phonebook changed unrelated card bytes')


def check_trace(text, readback=False, pin=False):
    check_security(text)
    if pin:
        from tools.nsb7_pin_services_check import check_authentication
        check_authentication(text.split('nsb7_menu_input:', 1)[0])
    keys = READ_KEYS if readback else SAVE_KEYS
    rows = list(re.finditer(r'nsb7_menu_input: key=(.*?) state=([01]) ', text))
    if [(row[1], row[2]) for row in rows] != [(HOST_KEYS[key], state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical phonebook contact inventory/order differs')
    for index, key in enumerate(keys):
        held = text[rows[index*2].end():rows[index*2+1].start()]
        if f'nsb7_key_decode: key={key:02x} observer_writes=0' not in held:
            raise ValueError('phonebook contact did not decode')
    tx = transmitted(text, 4096, 'nsb7_sim_tx')
    validity_targets(tx, 0x44)  # Validate complete PPS/APDU framing, including UPDATE body.
    if readback:
        if bytes.fromhex('a0dc') in tx:
            raise ValueError('cold retrieval unexpectedly writes the SIM')
    elif tx.count(bytes.fromhex('a0dc010420') + expected_record()) != 1:
        raise ValueError('expected one complete A/12345 UPDATE RECORD')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('save', type=Path)
    parser.add_argument('cold', type=Path)
    parser.add_argument('--pin-source', type=Path, help='own registered PIN-enabled source run')
    args = parser.parse_args()
    try:
        cards = []
        for run, readback in ((args.save, False), (args.cold, True)):
            check_trace((run / 'error.log').read_text(), readback, pin=args.pin_source is not None)
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            card = (run / 'nvram/nsb7hle/sim_card').read_bytes()
            check_card(card)
            cards.append(card)
        if cards[0] != cards[1]:
            raise ValueError('card storage changed during cold retrieval')
        if args.pin_source:
            initial = (args.pin_source / 'nvram/nsb7hle/sim_card').read_bytes()
            check_pin_card(initial, cards[0])
        frame_check(args.save / 'snap/nsb7_menu_56.png', 'fadcc0b3b9dbabd90d23602e2b3d698ddb88606a2bdd25748a009ae499d2f402')
        frame_check(args.cold / 'snap/nsb7_menu_32.png', '3a222e40dd0379311978dcbb674c1990e374e2e99f752c8e85a97c7451e187c0')
        frame_check(args.cold / 'snap/nsb7_menu_35.png', 'acf677164f77fcd6dc7df53c3e98dee65f4ec28e6c6ed69965a3dea358544e9e')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 phonebook: FAIL: {error}\n')
    print('PASS: physical A/12345 SIM save and cold Details retrieval; native RF unproved')


if __name__ == '__main__':
    main()
