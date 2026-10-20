"""Own physical NSB-1 PIN1 change and independent cold authentication."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_card_run, check_frame, check_cold_link
from tools.nsb1_pin_roundtrip import check_card, check_contacts, PIN_PROMPT_RGB_SHA256, ACCEPTED_RGB_SHA256, WRONG_RGB_SHA256
from tools.sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, padded_pin, statuses

CHANGED_FRAME = 'e3d554448ecdac351002f8a3f94582448fc1f03cc5a9b3071e9b0e60272f018a'


def changed_card(before, pin2=False):
    check_card(before, 3)
    result = bytearray(before)
    offset = CHV_OFFSET + (8 if pin2 else 0)
    result[offset:offset + 8] = padded_pin('8765' if pin2 else '4321')
    return bytes(result)


def check_operation(trace, instruction, result, count):
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    unblock_success = instruction == 0x2c and result == 0x9000
    if statuses(trace, 0x20) != [(0x9000, 3, 10)] * (2 if unblock_success else 1):
        raise ValueError('startup PIN authentication differs')
    if unblock_success:
        sequence = re.findall(r'SIM status ins=(20|2c) sw=([0-9a-f]{4}) ', trace)
        if sequence != [('20', '9000'), ('2c', '9000'), ('20', '9000')]:
            raise ValueError('unblock/follow-up VERIFY ordering differs')
    for ins in (0x24, 0x26, 0x28, 0x2c, 0xdc, 4, 0x44):
        expected = [(result, 3, 10)] if ins == instruction else []
        if statuses(trace, ins) != expected:
            raise ValueError('credential result or unrelated card operation differs')
    contacts = re.findall(r'nsb1_pin_change_contact: index=(\d+) down=([01]) ', trace)
    expected = [(str(i), down) for i in range(1, count + 1) for down in ('1', '0')]
    if contacts != expected:
        raise ValueError('physical PIN-change contacts differ')


def check_trace(trace, cold=False, rejected=False):
    check_operation(trace, None if cold else 0x24, 0x9804 if rejected else 0x9000, 0 if cold else 20)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'change', 'cold'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--pin2-roundtrip', action='store_true', help='third run cold-authenticates8765 by changing PIN2 back to5678')
    parser.add_argument('--rejected', type=Path)
    parser.add_argument('--retry', type=Path)
    args = parser.parse_args()
    try:
        before = (args.seed / 'nvram/nsb1r4t/sim_card').read_bytes()
        changed = changed_card(before, args.pin2_roundtrip)
        previous = args.seed
        for run, cold in ((args.change, False), (args.cold, True)):
            check_cold_link(run, previous)
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, 28 if cold and not args.pin2_roundtrip else 70)
            if card != (before if cold and args.pin2_roundtrip else changed):
                raise ValueError('whole-card PIN change/persistence differs')
            check_trace(trace, cold and not args.pin2_roundtrip)
            check_frame(run / 'snap/nsb1_pin_reload_4.png', ACCEPTED_RGB_SHA256)
            if args.pin2_roundtrip:
                check_frame(run / 'snap/nsb1_pin_change_65.png', PIN2_CHANGED_FRAME)
            elif cold:
                check_frame(run / 'snap/nsb1_pin_reload_1.png', PIN_PROMPT_RGB_SHA256)
            else:
                check_frame(run / 'snap/nsb1_pin_change_65.png', CHANGED_FRAME)
            previous = run
        if bool(args.rejected) != bool(args.retry) or (args.rejected and not args.pin2_roundtrip):
            raise ValueError('rejected/retry controls require a paired PIN2 round trip')
        if args.rejected:
            wrong = bytearray(before)
            wrong[ATTEMPTS_OFFSET + 1] = 2
            previous = args.seed
            for run, rejected in ((args.rejected, True), (args.retry, False)):
                check_cold_link(run, previous)
                code = json.loads((run / 'exit_status.json').read_text())['returncode']
                trace, card = check_card_run(run, code, 70)
                if card != (wrong if rejected else changed):
                    raise ValueError('rejected/retry whole-card result differs')
                check_trace(trace, rejected=rejected)
                check_frame(run / 'snap/nsb1_pin_change_65.png', WRONG_RGB_SHA256 if rejected else PIN2_CHANGED_FRAME)
                previous = run
    except (ValueError, OSError, KeyError) as error:
        print(f'NSB-1 PIN change failed: {error}', file=sys.stderr)
        return 1
    if args.pin2_roundtrip:
        print('NSB-1 physical PIN2 change and cold8765-authenticated restore PASS; native radio unproved')
        if args.rejected:
            print('NSB-1 rejected current PIN2 preserves credentials and cold correct retry restores counter PASS')
    else:
        print('NSB-1 physical ordinary PIN change and cold4321 authentication PASS; native radio unproved')
    return 0


PIN2_CHANGED_FRAME = 'd145e1eb7e1008b3271c709e01b483fdb017b2eb9e29e80b3f545da93ccff14f'

if __name__ == '__main__':
    raise SystemExit(main())
