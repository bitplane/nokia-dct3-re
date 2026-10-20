"""Physical NSB-1 PIN2 exhaustion, cold PUK2 recovery and reauthentication."""
import argparse
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_cold_link, check_card_run, check_frame
from tools.nsb1_pin_roundtrip import check_card, WRONG_RGB_SHA256
from tools.nsb1_pin_change_check import changed_card, check_operation, PIN2_CHANGED_FRAME
from tools.sim_security_trace_check import ATTEMPTS_OFFSET

BLOCKED_FRAME = '5e2f7fb2de8b983abd2f4cb3f6dc4591bc810b859df1ce6d5f89e8fe675e68d8'
PUK_FRAME = '9546d2c57981767c95749f443f892fd3ff7841e2486d75586848f565e3afbf76'


def exhausted_card(before, remaining):
    check_card(before, 3)
    if remaining not in (0, 1, 2):
        raise ValueError('expected depleted PIN2 retry count')
    result = bytearray(before)
    result[ATTEMPTS_OFFSET + 1] = remaining
    return bytes(result)


def rejected_puk_card(before):
    result = bytearray(exhausted_card(before, 0))
    result[ATTEMPTS_OFFSET + 3] = 9
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'two', 'one', 'zero', 'recover', 'cold'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--wrong-puk-cycle', nargs=3, type=Path, metavar=('WRONG', 'RECOVER', 'COLD'))
    args = parser.parse_args()
    try:
        before = (args.seed / 'nvram/nsb1r4t/sim_card').read_bytes()
        check_card(before, 3)
        previous = args.seed
        for run, remaining in ((args.two, 2), (args.one, 1), (args.zero, 0)):
            check_cold_link(run, previous)
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, 70)
            if card != exhausted_card(before, remaining):
                raise ValueError('exhaustion changed more than PIN2 retries')
            check_operation(trace, 0x24, 0x9840 if remaining == 0 else 0x9804, 20)
            check_frame(run / 'snap/nsb1_pin_change_65.png', BLOCKED_FRAME if remaining == 0 else WRONG_RGB_SHA256)
            previous = run
        recovered = changed_card(before, True)
        for run, recovering in ((args.recover, True), (args.cold, False)):
            check_cold_link(run, previous)
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, 80 if recovering else 70)
            if card != (recovered if recovering else before):
                raise ValueError('PUK2 recovery/ordinary restore changed unrelated card state')
            check_operation(trace, 0x2c if recovering else 0x24, 0x9000, 24 if recovering else 20)
            check_frame(run / 'snap' / ('nsb1_pin_change_45.png' if recovering else 'nsb1_pin_change_65.png'),
                        PUK_FRAME if recovering else PIN2_CHANGED_FRAME)
            previous = run
        if args.wrong_puk_cycle:
            previous = args.zero
            for phase, run in enumerate(args.wrong_puk_cycle):
                check_cold_link(run, previous)
                code = json.loads((run / 'exit_status.json').read_text())['returncode']
                trace, card = check_card_run(run, code, 70 if phase == 2 else 80)
                expected = (rejected_puk_card(before), recovered, before)[phase]
                if card != expected:
                    raise ValueError('wrong-PUK2/cold recovery changed unrelated card state')
                check_operation(trace, 0x24 if phase == 2 else 0x2c,
                                0x9804 if phase == 0 else 0x9000, 20 if phase == 2 else 24)
                expected_frame = (WRONG_RGB_SHA256, PUK_FRAME, PIN2_CHANGED_FRAME)[phase]
                time = (75, 45, 65)[phase]
                check_frame(run / 'snap' / f'nsb1_pin_change_{time}.png', expected_frame)
                previous = run
    except (ValueError, OSError, KeyError) as error:
        print(f'NSB-1 PUK2 cycle failed: {error}', file=sys.stderr)
        return 1
    print('NSB-1 organic PIN2 exhaustion, cold PUK2 recovery and cold ordinary restore PASS; native radio unproved')
    if args.wrong_puk_cycle:
        print('NSB-1 wrong-PUK2 rejection, cold retry persistence and recovery PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
