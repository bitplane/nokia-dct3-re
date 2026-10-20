"""Physical NSB-1 SIM security Off/On with linked cold request behavior."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_card_run, check_frame, check_cold_link
from tools.nsb1_pin_roundtrip import check_card, check_contacts, check_correct, ACCEPTED_RGB_SHA256
from tools.sim_security_trace_check import statuses, ENABLED_OFFSET

OFF = 'c907cc5c762c90cd1a8f54e57fbf668615576cc41ab34d71b5b696522523096e'
ON = '8e0c9f5e4fa5bba4213365fedeb1a68f5b394c87b858abc85cae85f6998818a2'
PHONE_ACCEPTED = '8fe18df5185a637968b1e15fe088b069c1978d70688e6cd4ccf0eae1e587eef2'


def card_result(before, phase):
    if phase not in range(4) or len(before) != 3570:
        raise ValueError('known phase/current laboratory card required')
    if before[ENABLED_OFFSET] != (1 if phase in (0, 3) else 0):
        raise ValueError('initial PIN-request state differs')
    after = bytearray(before)
    if phase in (0, 2):
        after[ENABLED_OFFSET] = 0 if phase == 0 else 1
    return bytes(after)


def check_trace(trace, phase):
    pin = phase in (0, 3)
    if pin:
        check_contacts(trace, 'pin', 5)
        check_contacts(trace, 'phone', 6)
    else:
        if 'nsb1_pin_contact:' in trace:
            raise ValueError('disabled cold composition entered SIM PIN')
        contacts = re.findall(r'nsb1_phonecode_contact: index=(\d+) down=([01]) ', trace)
        if contacts != [(str(i), down) for i in range(1, 7) for down in ('1', '0')]:
            raise ValueError('separate physical phone code missing')
        if re.findall(r'nsb1_security_gate: retry=([0-9a-f]+) match=([0-9a-f]+) ', trace) != [('00', '00000001')]:
            raise ValueError('separate phone-code acceptance differs')
    if statuses(trace, 0x20) != ([(0x9000, 3, 10)] if pin else []):
        raise ValueError('cold PIN authentication results differ')
    for instruction, selected in ((0x26, phase == 0), (0x28, phase == 2)):
        if statuses(trace, instruction) != ([(0x9000, 3, 10)] if selected else []):
            raise ValueError('SIM request command differs')
    if any(statuses(trace, ins) for ins in (0x24, 0x2c, 0xdc, 4, 0x44)):
        raise ValueError('unrelated SIM mutation')
    settings = re.findall(r'nsb1_settings_contact: index=(\d+) down=([01]) ', trace)
    expected = [(str(i), down) for i in range(1, 12) for down in ('1', '0')] if phase in (0, 2) else []
    if settings != expected:
        raise ValueError('physical settings input differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'off', 'off_cold', 'on', 'on_cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        previous = args.seed
        check_card((previous / 'nvram/nsb1r4t/sim_card').read_bytes(), 3)
        for phase, run in enumerate((args.off, args.off_cold, args.on, args.on_cold)):
            check_cold_link(run, previous)
            old = previous / 'nvram/nsb1r4t'
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, 54 if phase in (0, 2) else 28)
            if card != card_result((old / 'sim_card').read_bytes(), phase):
                raise ValueError('whole-card request transition differs')
            check_trace(trace, phase)
            if phase in (0, 2):
                check_frame(run / 'snap/nsb1_security_settings_49.png', OFF if phase == 0 else ON)
            if phase == 0:
                check_frame(run / 'snap/nsb1_pin_reload_4.png', ACCEPTED_RGB_SHA256)
            elif phase in (1, 2):
                check_frame(run / 'snap/nsb1_phonecode_3.png', PHONE_ACCEPTED)
            else:
                check_correct(trace, card, run / 'snap')
            previous = run
    except (OSError, ValueError, KeyError) as error:
        print(f'NSB-1 PIN request failed: {error}', file=sys.stderr)
        return 1
    print('NSB-1 physical SIM request Off/On and cold challenge persistence PASS; fitted mask/network unproved')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
