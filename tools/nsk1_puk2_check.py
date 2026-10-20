"""Own NSK-1 organic PIN2 exhaustion and cold PUK2 recovery."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin2_change_check import check_cold_link, check_run as check_change
from tools.nsk1_pin_roundtrip import check_run as check_pin, check_contacts, CODE_ERROR, ACCEPTED
from tools.nsk1_pin_change_roundtrip import check_change_contacts
from tools.nsk1_security_compare import check_phase
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, CHV_OFFSET, statuses

BLOCKED = '5e2f7fb2de8b983abd2f4cb3f6dc4591bc810b859df1ce6d5f89e8fe675e68d8'
REJECTED = 'dad155be27d2f6b1c665380ecea483fa6c2cef44b0b3b2cb43740e956d0f2ff0'


def exhausted_card(before):
    if len(before) != 3570 or before[ATTEMPTS_OFFSET+1] not in (1, 2, 3):
        raise ValueError('live PIN2 retry required')
    after = bytearray(before)
    after[ATTEMPTS_OFFSET+1] -= 1
    return bytes(after)


def check_exhaustion(run, previous):
    before = check_cold_link(run, previous)
    _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    check_change_contacts(trace, pin2=True, wrong=True)
    if statuses(trace, 0x20) != [(0x9000, 3, 10)]:
        raise ValueError('startup PIN1 authentication differs')
    after = exhausted_card(before)
    expected_sw = 0x9840 if after[ATTEMPTS_OFFSET+1] == 0 else 0x9804
    if statuses(trace, 0x24) != [(expected_sw, 3, 10)]:
        raise ValueError('PIN2 exhaustion status differs')
    if any(statuses(trace, ins) for ins in (4, 0x44, 0x26, 0x28, 0x2c, 0xdc, 0xd6)):
        raise ValueError('unrelated card operation during exhaustion')
    if (run / 'nvram/nsk1r4t/sim_card').read_bytes() != after:
        raise ValueError('exhaustion changed unrelated card bytes')
    frame_check(run / 'snap/nsk1_pin_change_20.png', BLOCKED if expected_sw == 0x9840 else CODE_ERROR)


def recovered_card(before, wrong=False):
    if (len(before) != 3570 or before[ATTEMPTS_OFFSET+1] != 0
            or not 1 <= before[ATTEMPTS_OFFSET+3] <= 10
            or before[CHV_OFFSET+8:CHV_OFFSET+16] != b'5678\xff\xff\xff\xff'):
        raise ValueError('organically exhausted PIN2 and unused PUK2 required')
    after = bytearray(before)
    if wrong:
        after[ATTEMPTS_OFFSET+3] -= 1
    else:
        after[ATTEMPTS_OFFSET+1] = 3
        after[ATTEMPTS_OFFSET+3] = 10
        after[CHV_OFFSET+8:CHV_OFFSET+12] = b'8765'
    return bytes(after)


def check_recovery(run, previous, wrong=False):
    before = check_cold_link(run, previous)
    _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    check_change_contacts(trace, pin2=True, puk2=True, wrong_puk2=wrong)
    sw = (0x9840 if before[ATTEMPTS_OFFSET+3] == 1 else 0x9804) if wrong else 0x9000
    if (statuses(trace, 0x20) != [(0x9000, 3, 10)]*(1 if wrong else 2)
            or statuses(trace, 0x2c) != [(sw, 3, 10)]
            or re.findall(r'SIM status ins=(20|2c) ', trace) != (['20', '2c'] if wrong else ['20', '2c', '20'])):
        raise ValueError('startup PIN1, UNBLOCK and automatic PIN2 verification differ')
    if any(statuses(trace, ins) for ins in (4, 0x44, 0x24, 0x26, 0x28, 0xdc, 0xd6)):
        raise ValueError('unrelated recovery operation')
    if (run / 'nvram/nsk1r4t/sim_card').read_bytes() != recovered_card(before, wrong):
        raise ValueError('PUK2 recovery changed unrelated card bytes')
    frame_check(run / 'snap/nsk1_pin_change_21.png', REJECTED if sw == 0x9840 else CODE_ERROR if wrong else ACCEPTED)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'two', 'one', 'zero'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--recovery', nargs=2, type=Path, metavar=('RECOVER', 'COLD'))
    parser.add_argument('--wrong-puk-cycle', nargs=3, type=Path, metavar=('WRONG', 'RECOVER', 'COLD'))
    args = parser.parse_args()
    try:
        _, wrong = check_pin(args.source / 'wrong')
        _, card = check_pin(args.source / 'correct', True, wrong)
        if card[ATTEMPTS_OFFSET+1] != 3 or card[CHV_OFFSET+8:CHV_OFFSET+16] != b'5678\xff\xff\xff\xff':
            raise ValueError('healthy PIN2 seed required')
        previous = args.source / 'correct'
        for run in (args.two, args.one, args.zero):
            check_exhaustion(run, previous)
            previous = run
        if args.recovery:
            recover, cold = args.recovery
            check_recovery(recover, args.zero)
            check_change(cold, recover, restore=True)
            if (cold / 'nvram/nsk1r4t/sim_card').read_bytes() != card:
                raise ValueError('cold restored card differs from original seed')
        if args.wrong_puk_cycle:
            rejected, recover, cold = args.wrong_puk_cycle
            check_recovery(rejected, args.zero, wrong=True)
            check_recovery(recover, rejected)
            check_change(cold, recover, restore=True)
            if (cold / 'nvram/nsk1r4t/sim_card').read_bytes() != card:
                raise ValueError('post-rejection restored card differs from original seed')
    except (ValueError, OSError, KeyError) as error:
        print(f'NSK-1 PIN2 exhaustion failed: {error}', file=sys.stderr)
        return 1
    print('NSK-1 organic PIN2 exhaustion PASS' + ('; cold PUK2 recovery and authentication PASS' if args.recovery or args.wrong_puk_cycle else '; recovery not implied'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
