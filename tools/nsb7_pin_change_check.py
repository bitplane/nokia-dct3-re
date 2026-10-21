"""Physical ordinary PIN1 change, rejected old PIN and cold new-PIN use."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_pin_check import check_trace as check_pin, serialized_profile, check_frame
from tools.nsb7_sms_check import tx_bytes
from tools.nsb7_storage_check import inspect
from tools.sim_security_trace_check import statuses, CHV_OFFSET, ATTEMPTS_OFFSET, padded_pin


def check_change(text, wrong, *, pin2=False, old=None, new=None, remaining=None, unblock=False,
                 puk_remaining=None):
    if unblock and not pin2:
        raise ValueError('this menu recovery fixture targets PIN2 only')
    if '[LUA ERROR]' in text or 'nsb7_change_input: complete' not in text:
        raise ValueError('change-PIN fixture incomplete')
    steps = [(16+i, 'Keypad '+digit, 'phone') for i, digit in enumerate('12345')]
    steps += [(21, 'Left Softkey / Menu', 'phone'), (26, 'Left Softkey / Menu', 'menu'),
              (28, 'Keypad 4', 'menu'), (30, 'Keypad 6', 'menu'), (32, 'Keypad 5', 'menu')]
    if pin2:
        steps.append((34, 'Keypad 2', 'menu'))
    steps.append((35, 'Left Softkey / Menu', 'menu'))
    old = old if old is not None else (('87654322' if wrong else '87654321') if unblock else
                                      '1111' if wrong else '5678' if pin2 else '1234')
    new = new if new is not None else ('8765' if pin2 else '4321')
    new_start = 38+len(old)+3
    for phase, digits, start in [('puk2' if unblock else 'old', old, 38),
                                 ('new', new, new_start), ('confirm', new, new_start+len(new)+3)]:
        steps += [(start+i, 'Keypad '+digit, phase) for i, digit in enumerate(digits)]
        steps.append((start+len(digits), 'Left Softkey / Menu', phase))
    rows = list(re.finditer(r'nsb7_change_input: phase=(\w+) key=(.*?) state=([01]) t=([0-9.]+)', text))
    expected = [(phase, key, state, f'{second}.{fraction}') for second, key, phase in steps
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if [row.groups() for row in rows] != expected:
        raise ValueError('physical old/new/confirm sequence differs')
    for index, (_, key, _) in enumerate(steps):
        code = 0x19 if key == 'Left Softkey / Menu' else int(key[-1])
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('change-PIN contact did not decode')
    check_pin(text.split('nsb7_change_input: phase=menu', 1)[0], True)
    remaining = remaining if remaining is not None else (0 if unblock and wrong else 2 if wrong else 3)
    instruction = 0x2c if unblock else 0x24
    puk_remaining = puk_remaining if puk_remaining is not None else (9 if unblock and wrong else 10)
    if not 0 <= puk_remaining <= 10:
        raise ValueError('PUK retry count outside 0..10')
    result = (0x9840 if (unblock and puk_remaining == 0) or (not unblock and remaining == 0)
              else 0x9804) if wrong else 0x9000
    if statuses(text, instruction) != [(result, 3 if pin2 else remaining, 10)]:
        raise ValueError('credential-command outcome/retries differ')
    if pin2 and re.findall(rf'SIM status ins={instruction:02x} sw=[0-9a-f]{{4}} chv=3/(\d+) puk=10/(\d+) enabled=1', text) != [(str(remaining), str(puk_remaining))]:
        raise ValueError('PIN2/PUK2 retry accounting differs')
    if any(statuses(text, ins) for ins in (0x26, 0x28, 0x24 if unblock else 0x2c)):
        raise ValueError('unrelated security mutation')
    command = bytes((0xa0, instruction, 0, 2 if pin2 else 1, 16)) + padded_pin(old) + padded_pin(new)
    wire = tx_bytes(text)
    if wire.count(command) != 1 or command not in tx_bytes(text[rows[-2].end():]) or b'\xa0\xdc' in wire:
        raise ValueError('credential APDU must follow confirmation without file writes')
    if unblock:
        automatic = bytes.fromhex('a020000208') + padded_pin(new)
        if statuses(text, 0x20) != [(0x9000, 3, 10)]*(1 if wrong else 2):
            raise ValueError('startup and post-unblock VERIFY outcomes differ')
        if wrong and automatic in wire:
            raise ValueError('failed PUK2 must not verify the proposed PIN2')
        if not wrong and (wire.count(automatic) != 1 or wire.index(automatic) < wire.index(command)):
            raise ValueError('recovered PIN2 must be automatically verified after UNBLOCK')


def check_cards(pairs):
    initial = serialized_profile()
    wrong = bytearray(initial)
    wrong[ATTEMPTS_OFFSET] = 2
    changed = bytearray(initial)
    changed[CHV_OFFSET:CHV_OFFSET+8] = padded_pin('4321')
    if pairs != [(initial, bytes(wrong)), (initial, bytes(changed)), (bytes(changed), bytes(changed))]:
        raise ValueError('complete rejected/changed/cold card contents differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'wrong', 'changed', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check_change((args.wrong / 'error.log').read_text(), True)
        check_change((args.changed / 'error.log').read_text(), False)
        check_pin((args.cold / 'error.log').read_text(), True, pin='4321')
        pairs = []
        for run in (args.wrong, args.changed, args.cold):
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            pairs.append(((run / 'initial_sim_card.bin').read_bytes(),
                          (run / 'nvram/nsb7hle/sim_card').read_bytes()))
        check_cards(pairs)
        check_frame(args.wrong / 'snap/nsb7_change_56.png',
                    'ad7af892b0b44757f1512e08f26513c49fe5434c8d573b066c767549424bf660')
        check_frame(args.changed / 'snap/nsb7_change_56.png',
                    '2107c97336e3fdb24b10e1b39d9d87374e7356a3a152adf126594eee8147f5dc')
        check_frame(args.cold / 'snap/nsb7_pin_final.png',
                    '24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PIN change: FAIL: {error}\n')
    print('PASS: wrong-current-PIN rejection, physical PIN1 change and cold new-PIN acceptance')


if __name__ == '__main__':
    main()
