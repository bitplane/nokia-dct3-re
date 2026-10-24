"""Qualify physical NSB-3 SIM PIN retry persistence, distinct from phone lock."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb3_menu_check import check_trace as check_phone_code, FRAME as MENU_FRAME
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import statuses, padded_pin, ATTEMPTS_OFFSET, CHV_OFFSET

PIN_PROMPT = 'ceafc51da2379ff21cd4fc90a07d717d546cc2176be35e2948eec80a5452bb2a'
WRONG_FRAME = 'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
PHONE_PROMPT = 'f14029ce68892bd3d5481aa41aea73af2cf6a3f783cd02dd3633c87dbf23e6b4'


def check_card(card, retries, pin='1234', enabled=True):
    if retries not in range(4) or not re.fullmatch(r'[0-9]{4,8}', pin):
        raise ValueError('invalid expected PIN1/retry state')
    expected = bytearray(make_current_profile(enabled))
    expected[ATTEMPTS_OFFSET] = retries
    expected[CHV_OFFSET:CHV_OFFSET + 8] = padded_pin(pin)
    if card != bytes(expected):
        raise ValueError('PIN-enabled whole card differs beyond qualified PIN1 retry count')


def check_trace(text, correct, attempts_before=3, pin=None, allowed_mutations=(), additional_verifies=()):
    if attempts_before not in (1, 2, 3):
        raise ValueError('requires live PIN1 attempts')
    digits = pin or ('1234' if correct else '4321')
    keys = tuple('Keypad ' + digit for digit in digits) + ('Left Softkey / Menu',)
    contacts = re.findall(r'nsb3_pin_key: correct=([01]) key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(str(int(correct)), key, down) for key in keys for down in ('1', '0')]:
        raise ValueError('physical SIM PIN contact order differs')
    remaining = 3 if correct else attempts_before - 1
    expected = (0x9000 if correct else 0x9840 if remaining == 0 else 0x9804, remaining, 10)
    if additional_verifies and not correct:
        raise ValueError('extra verification requires authenticated startup')
    if statuses(text, 0x20) != [expected] + [(0x9000, 3, 10)] * len(additional_verifies):
        raise ValueError('SIM VERIFY status/retry transition differs')
    commands = wire(text)
    verify = [row[:5] for row in commands if row[0] == 0x20]
    if verify != [(0x20, 0, 1, 8, padded_pin(digits))] + list(additional_verifies):
        raise ValueError('physical VERIFY APDU differs')
    if any(row[0] in (0x24, 0x26, 0x28, 0x2c) and row[0] not in allowed_mutations for row in commands):
        raise ValueError('unexpected SIM credential mutation')
    if correct:
        check_phone_code(text)
    elif 'nsb3_phonecode_key:' in text:
        raise ValueError('wrong-PIN phase must not enter phone code')


def check_source(wrong):
    initial = make_current_profile(True)
    if (wrong / 'initial_sim_card.bin').read_bytes() != initial:
        raise ValueError('wrong-PIN source is not the shared generated card')
    metadata = json.loads((wrong / 'input.json').read_text())
    if (metadata.get('laboratory_pin_enabled') is not True or
            metadata.get('initial_sim_card_sha256') != hashlib.sha256(initial).hexdigest()):
        raise ValueError('PIN-enabled fixture provenance differs')


def check_pair(wrong, correct):
    check_source(wrong)
    cards = []
    for run, accepted in ((wrong, False), (correct, True)):
        check_trace((run / 'error.log').read_text(), accepted)
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        check_card(card, 3 if accepted else 2)
        cards.append(card)
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
    if (correct / 'initial_sim_card.bin').read_bytes() != cards[0]:
        raise ValueError('cold process did not receive the persistent two-attempt card')
    provenance = json.loads((correct / 'cold_source.json').read_text())
    if provenance.get('copied_guest_ram_or_save_state') is not False:
        raise ValueError('cold provenance missing')
    for name in ('flash', 'eeprom', 'sim_card'):
        if provenance['storage_sha256'][name] != hashlib.sha256((wrong / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest():
            raise ValueError('cold source own storage differs')
    for run in (wrong, correct):
        frame_check(run / 'snap/nsb3_pin_prompt.png', PIN_PROMPT)
    frame_check(wrong / 'snap/nsb3_pin_result.png', WRONG_FRAME)
    frame_check(correct / 'snap/nsb3_pin_result.png', PHONE_PROMPT)
    frame_check(correct / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-correct', 'prepare-wrong', 'check'))
    parser.add_argument('wrong', type=Path)
    parser.add_argument('correct', type=Path)
    parser.add_argument('--attempts', type=int, choices=(1, 2, 3), default=2,
                        help='source remaining PIN1 attempts for cold preparation')
    args = parser.parse_args()
    try:
        if args.operation in ('prepare-correct', 'prepare-wrong'):
            prepare_reload(args.wrong, args.correct, lambda card: check_card(card, args.attempts))
        else:
            check_pair(args.wrong, args.correct)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 SIM PIN: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own SIM PIN contract; phone code remains separate')


if __name__ == '__main__':
    main()
