"""Physical NSK-1 PIN2 change and cold authenticated restoration."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin_roundtrip import check_run as check_pin, check_contacts, CODE_ERROR
from tools.nsk1_pin_change_roundtrip import check_change_contacts
from tools.nsk1_security_compare import check_phase
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, statuses

CHANGED = 'd145e1eb7e1008b3271c709e01b483fdb017b2eb9e29e80b3f545da93ccff14f'


def expected_card(before, restore=False, wrong=False):
    old, new = (b'8765', b'5678') if restore else (b'5678', b'8765')
    if len(before) != 3570 or before[CHV_OFFSET+8:CHV_OFFSET+16] != old + b'\xff'*4:
        raise ValueError('known complete card and current PIN2 required')
    if before[ATTEMPTS_OFFSET+1] not in (2, 3):
        raise ValueError('healthy or once-rejected PIN2 required')
    after = bytearray(before)
    if wrong:
        if before[ATTEMPTS_OFFSET+1] != 3:
            raise ValueError('first rejection requires three retries')
        after[ATTEMPTS_OFFSET+1] = 2
    else:
        after[CHV_OFFSET+8:CHV_OFFSET+12] = new
        after[ATTEMPTS_OFFSET+1] = 3
    return bytes(after)


def check_cold_link(run, previous):
    source = previous / 'nvram/nsk1r4t'
    before = (source / 'sim_card').read_bytes()
    manifest = json.loads((run / 'input_manifest.json').read_text())
    if (manifest.get('source_card_sha256') != hashlib.sha256(before).hexdigest()
            or manifest.get('source_manifest_sha256') != hashlib.sha256((previous / 'input_manifest.json').read_bytes()).hexdigest()
            or manifest.get('ram_or_save_state_copied') is not False
            or (run / 'initial_eeprom.bin').read_bytes() != (source / 'eeprom').read_bytes()):
        raise ValueError('cold persistent input link differs')
    return before


def check_run(run, previous, restore=False, wrong=False):
    before = check_cold_link(run, previous)
    _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    check_change_contacts(trace, pin2=True, restore=restore, wrong=wrong)
    if (trace.index('SIM status ins=20 sw=9000') > trace.index('nsk1_pin_contact: phase=phone index=1 down=1 ')
            or trace.index('nsk1_pin_change_contact: phase=menu index=1 ') < trace.index('nsk1_pin_contact: phase=phone index=6 down=0 ')):
        raise ValueError('SIM/phone/menu authentication order differs')
    if statuses(trace, 0x20) != [(0x9000, 3, 10)] or statuses(trace, 0x24) != [(0x9804 if wrong else 0x9000, 3, 10)]:
        raise ValueError('startup PIN1 or PIN2 change failed')
    if any(statuses(trace, ins) for ins in (4, 0x44, 0x26, 0x28, 0x2c, 0xdc, 0xd6)):
        raise ValueError('unrelated card operation')
    card = (run / 'nvram/nsk1r4t/sim_card').read_bytes()
    if card != expected_card(before, restore, wrong):
        raise ValueError('PIN2 change modified unrelated card bytes')
    frame_check(run / 'snap/nsk1_pin_change_20.png', CODE_ERROR if wrong else CHANGED)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'change', 'restore'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--rejection-cycle', nargs=2, type=Path, metavar=('WRONG', 'RECOVER'))
    args = parser.parse_args()
    try:
        _, wrong = check_pin(args.source / 'wrong')
        check_pin(args.source / 'correct', True, wrong)
        check_run(args.change, args.source / 'correct')
        check_run(args.restore, args.change, True)
        if args.rejection_cycle:
            rejected, recovered = args.rejection_cycle
            check_run(rejected, args.source / 'correct', wrong=True)
            check_run(recovered, rejected)
    except (ValueError, OSError, KeyError) as error:
        print(f'NSK-1 PIN2 change failed: {error}', file=sys.stderr)
        return 1
    print('NSK-1 physical PIN2 change and cold authenticated restore PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
