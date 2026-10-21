"""Physical PIN exhaustion, failed PUK, cold recovery and replacement-PIN reuse."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_pin_check import check_trace as check_pin, serialized_profile, check_frame
from tools.nsb7_sms_check import tx_bytes
from tools.sim_security_trace_check import statuses, ATTEMPTS_OFFSET, CHV_OFFSET, padded_pin
from tools.make_sim_card_profile import make_profile
from tools.nsb7_storage_check import inspect


def card_state(retries, puk=10, pin='1234'):
    card = bytearray(serialized_profile())
    card[ATTEMPTS_OFFSET] = retries
    card[ATTEMPTS_OFFSET+2] = puk
    card[CHV_OFFSET:CHV_OFFSET+8] = padded_pin(pin)
    return bytes(card)


def check_cards(pairs):
    expected = [(make_profile(True), card_state(2)), (card_state(2), card_state(1)),
                (card_state(1), card_state(0)), (card_state(0), card_state(0, 9)),
                (card_state(0, 9), card_state(3, pin='4321')),
                (card_state(3, pin='4321'), card_state(3, pin='4321'))]
    if pairs != expected:
        raise ValueError('complete cold card chain differs from PIN/PUK/new-PIN transitions')


def check_puk(text, wrong, remaining=9):
    if '[LUA ERROR]' in text or 'nsb7_puk_input: complete' not in text:
        raise ValueError('physical PUK fixture incomplete')
    puk = '12345679' if wrong else '12345678'
    rows = list(re.finditer(r'nsb7_puk_input: wrong=([01]) phase=(\w+) key=(\w+) state=([01]) t=([0-9.]+)', text))
    expected = []
    codes = []
    for phase, digits, start in [('puk', puk, 8), ('new', '4321', 19), ('confirm', '4321', 26)]:
        for index, key in enumerate(list(digits) + ['Menu']):
            codes.append(0x19 if key == 'Menu' else int(key))
            expected += [(str(int(wrong)), phase, key, state, f'{start+index}.{fraction}')
                         for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if [row.groups() for row in rows] != expected:
        raise ValueError('physical PUK/replacement/confirmation sequence differs')
    for index, code in enumerate(codes):
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('PUK contact did not decode')
    if not 0 <= remaining <= 9:
        raise ValueError('wrong-PUK retry must be in 0..9')
    status = (0x9840 if remaining == 0 else 0x9804) if wrong else 0x9000
    if statuses(text, 0x2c) != [(status, 0 if wrong else 3, remaining if wrong else 10)]:
        raise ValueError('UNBLOCK result/retries differ')
    if any(statuses(text, ins) for ins in (0x20, 0x24, 0x26, 0x28)):
        raise ValueError('unexpected credential command during PUK recovery')
    wire = tx_bytes(text)
    command = bytes.fromhex('a02c000010') + puk.encode('ascii') + padded_pin('4321')
    if wire.count(command) != 1 or b'\xa0\xdc' in wire:
        raise ValueError('UNBLOCK APDU or no-file-write control differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'first', 'second', 'blocked', 'wrong_puk', 'recovered', 'accepted'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    runs = [args.first, args.second, args.blocked, args.wrong_puk, args.recovered, args.accepted]
    try:
        contract(args.image.read_bytes())
        texts = [(run / 'error.log').read_text() for run in runs]
        for text, retries in zip(texts[:3], (2, 1, 0)):
            check_pin(text, False, retries=retries)
        check_puk(texts[3], True)
        check_puk(texts[4], False)
        check_pin(texts[5], True, pin='4321')
        pairs = []
        for run in runs:
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            pairs.append(((run / 'initial_sim_card.bin').read_bytes(),
                          (run / 'nvram/nsb7hle/sim_card').read_bytes()))
        check_cards(pairs)
        puk_prompt = 'bb522918e8d4e86e6f8b7602f4ed7f44385f17675d4a939b4fba50a5ac33d68f'
        phone_prompt = '24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2'
        check_frame(args.blocked / 'snap/nsb7_pin_final.png', puk_prompt)
        check_frame(args.wrong_puk / 'snap/nsb7_puk_final.png', puk_prompt)
        check_frame(args.recovered / 'snap/nsb7_puk_final.png', phone_prompt)
        check_frame(args.accepted / 'snap/nsb7_pin_final.png', phone_prompt)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PUK: FAIL: {error}\n')
    print('PASS: physical PIN exhaustion, wrong PUK retry, cold new-PIN recovery and cold replacement-PIN acceptance')


if __name__ == '__main__':
    main()
