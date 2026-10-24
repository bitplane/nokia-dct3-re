"""Physical NSB-7 wrong PIN and cold correct PIN with exact card retries."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_sms_check import tx_bytes
from tools.make_sim_card_profile import make_current_profile, make_profile
from tools.sim_security_trace_check import statuses, ATTEMPTS_OFFSET


def serialized_profile():
    return make_current_profile(True)


def check_frame(path, expected):
    from PIL import Image
    with Image.open(path) as image:
        if image.size != (84, 48) or hashlib.sha256(image.convert('RGB').tobytes()).hexdigest() != expected:
            raise ValueError('SIM security prompt/result pixels differ: ' + str(path))


def check_trace(text, correct, pin=None, retries=None):
    if '[LUA ERROR]' in text or 'nsb7_pin_input: complete' not in text:
        raise ValueError('physical PIN fixture incomplete')
    digits = pin if pin is not None else ('1234' if correct else '4321')
    keys = list(digits) + ['Menu']
    rows = list(re.finditer(r'nsb7_pin_input: correct=([01]) key=(\w+) state=([01]) t=([0-9.]+)', text))
    expected = [(str(int(correct)), key, state, f'{8+i}.{fraction}')
                for i, key in enumerate(keys)
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if [row.groups() for row in rows] != expected:
        raise ValueError('physical PIN keys differ')
    for index, key in enumerate(keys):
        code = 0x19 if key == 'Menu' else int(key)
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('PIN contact did not decode')
    remaining = retries if retries is not None else (3 if correct else 2)
    status = 0x9000 if correct else 0x9840 if remaining == 0 else 0x9804
    if statuses(text, 0x20) != [(status, remaining, 10)]:
        raise ValueError('SIM VERIFY result/retries differ')
    if any(statuses(text, instruction) for instruction in (0x24, 0x26, 0x28, 0x2c)):
        raise ValueError('unexpected security mutation')
    wire = tx_bytes(text)
    verify = bytes.fromhex('a020000108') + digits.encode('ascii') + b'\xff'*4
    if wire.count(verify) != 1 or b'\xa0\xdc' in wire:
        raise ValueError('VERIFY APDU or no-file-write control differs')


def check_cards(initial, wrong, cold_input, correct):
    if initial != make_profile(True):
        raise ValueError('wrong-PIN input is not the declared laboratory card')
    # Current serializer appends PLMN selection then erased SMSS/AoC defaults.
    expected = bytearray(serialized_profile())
    expected[ATTEMPTS_OFFSET] = 2
    if wrong != bytes(expected) or cold_input != wrong:
        raise ValueError('wrong-PIN storage or cold retry input differs')
    expected[ATTEMPTS_OFFSET] = 3
    if correct != bytes(expected):
        raise ValueError('correct PIN changed more than the retry count')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'wrong', 'correct'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        for run, correct in ((args.wrong, False), (args.correct, True)):
            check_trace((run / 'error.log').read_text(), correct)
        check_cards((args.wrong / 'initial_sim_card.bin').read_bytes(),
                    (args.wrong / 'nvram/nsb7hle/sim_card').read_bytes(),
                    (args.correct / 'initial_sim_card.bin').read_bytes(),
                    (args.correct / 'nvram/nsb7hle/sim_card').read_bytes())
        images = [(args.wrong, 'nsb7_pin_prompt.png', 'a12b3c3fdb4a3d1e8ffeb5fc8986c3bc008bb86b1f2e927440848fe0420ffc0c'),
                  (args.correct, 'nsb7_pin_prompt.png', 'a12b3c3fdb4a3d1e8ffeb5fc8986c3bc008bb86b1f2e927440848fe0420ffc0c'),
                  (args.wrong, 'nsb7_pin_result.png', 'ad7af892b0b44757f1512e08f26513c49fe5434c8d573b066c767549424bf660'),
                  (args.correct, 'nsb7_pin_final.png', '24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2')]
        for run, name, expected in images:
            check_frame(run / 'snap' / name, expected)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 SIM PIN: FAIL: {error}\n')
    print('PASS: physical wrong PIN, exact saved retry decrement and cold correct PIN retry restoration')


if __name__ == '__main__':
    main()
